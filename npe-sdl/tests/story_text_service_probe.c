#include "../npe.h"
#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "story_text_native.h"
static void frame(const char *path,uint8_t *pixels){
    FILE *in=fopen(path,"rb");assert(in);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static unsigned differences(const NpeWorld *w,const StoryTextState *s,int pointer){
    return (pointer&&w->text_file_position!=s->pointer)+(w->text_offset!=s->offset)+
        (w->text_more!=s->more)+(w->text_style!=s->style)+(memcmp(w->text_rect,s->rect,8)!=0);
}
static void controls(NpeFile *f,const StoryTextState *s){
    uint8_t bytes[8];
    for(unsigned i=0;i<4;++i){bytes[2*i]=(uint8_t)(s->rect[i]>>8);bytes[2*i+1]=(uint8_t)s->rect[i];}
    assert(!npe_rpg_runtime_write(f,0x52791e,bytes,8));
    for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(s->pointer>>(24u-8u*i));
    assert(!npe_rpg_runtime_write(f,0x52797a,bytes,4));
    bytes[0]=(uint8_t)(s->offset>>8);bytes[1]=(uint8_t)s->offset;bytes[2]=s->more;
    assert(!npe_rpg_runtime_write(f,0x527a1a,bytes,3));
    bytes[0]=s->style;assert(!npe_rpg_runtime_write(f,0x527ace,bytes,1));
}
int main(int argc,char **argv){assert(argc==2||(argc==3&&!strcmp(argv[2],"--legacy")));FILE *in=fopen(argv[1],"rb");assert(in);
    assert(!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0&&!fseek(in,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile f;char error[128];assert(!npe_parse(&f,bytes,(size_t)size,error,sizeof(error)));
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    NpeWorld *w=malloc(sizeof(*w));assert(w);unsigned failures=0,total=0;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[NPE_WIDTH*NPE_HEIGHT];char path[180];
    if(argc==3){unsigned cases=0;
        for(unsigned i=0;i<sizeof(story_text_cases)/sizeof(*story_text_cases);++i){const StoryTextCase *r=story_text_cases+i;
            if(r->closes)continue;
            NpeStoryRecord story;int found=0;
            for(unsigned id=0;id<f.offset_table_records[10];++id){
                assert(!npe_story_record(&f,id,&story));
                if((uint32_t)(story.text-f.bytes)==r->before.pointer){found=1;break;}
            }
            snprintf(path,sizeof(path),"tmp/native-story-text-%s-before.idx",r->name);frame(path,pixels);
            int result=-1;size_t next;
            if(found){story.header_value=r->before.style;
                result=r->before.style==1?npe_render_native_story_page(&f,&font,&story,r->before.offset,&next,pixels):
                    npe_render_native_window_story_page(&font,&story,r->before.offset,&next,pixels);}
            snprintf(path,sizeof(path),"tmp/native-story-text-%s-0.idx",r->name);frame(path,native);
            unsigned bad=result!=0;
            if(!result)for(unsigned k=0;k<sizeof(pixels);++k)bad+=pixels[k]!=native[k];
            fprintf(stderr,"legacy typed renderer %s: result%d, %u differences\n",r->name,result,bad);
            failures+=bad!=0;++cases;
        }
        printf("Legacy typed renderers: %u original live-control pages, %u failed\n",cases,failures);
        npe_font_free(&font);free(w);npe_free(&f);return failures?1:0;
    }
    for(unsigned i=0;i<sizeof(story_text_pages)/sizeof(*story_text_pages);++i){const StoryTextPages *r=story_text_pages+i;
        assert(!npe_world_init(&f,w,9,29,13)&&!npe_world_resources_open(&f,w));
        w->game_mode=5;w->story_event_id=(uint16_t)r->id;assert(npe_world_prepare_story(&f,w)==1);
        assert(!npe_rpg_runtime_text_prefix(&f)&&w->text_offset==1);
        if(r->window){snprintf(path,sizeof(path),"tmp/native-window-story-%u-before.idx",r->id);frame(path,pixels);}
        else memset(pixels,0xa5,sizeof(pixels));
        uint8_t before[32],after[32];assert(!npe_rpg_runtime_read(&f,NPE_RESIDENT_HEAP_BITMAP,before,32));
        for(unsigned j=0;j<r->count;++j){assert(!npe_render_world_story_page(&f,&font,w,pixels));
            snprintf(path,sizeof(path),"tmp/native-%sstory-%u-page-%u.idx",r->window?"window-":"",r->id,j);frame(path,native);
            unsigned bad=differences(w,r->pages+j,0);
            for(unsigned k=0;k<sizeof(pixels);++k)bad+=pixels[k]!=native[k];
            assert(!npe_rpg_runtime_read(&f,NPE_RESIDENT_HEAP_BITMAP,after,32));bad+=memcmp(before,after,32)!=0;
            fprintf(stderr,"story %u page%u: %u differences\n",r->id,j,bad);failures+=bad!=0;++total;
        }npe_rpg_runtime_close(&f);
    }
    for(unsigned i=0;i<sizeof(story_text_cases)/sizeof(*story_text_cases);++i){const StoryTextCase *r=story_text_cases+i;
        if(r->closes)continue; /* Exact continuation branch is checked in actual main. */
        assert(!npe_world_init(&f,w,9,29,13)&&!npe_world_resources_open(&f,w));
        controls(&f,&r->before);assert(!differences(w,&r->before,1));
        snprintf(path,sizeof(path),"tmp/native-story-text-%s-before.idx",r->name);frame(path,pixels);
        assert(!npe_render_world_story_page(&f,&font,w,pixels));
        snprintf(path,sizeof(path),"tmp/native-story-text-%s-0.idx",r->name);frame(path,native);
        unsigned bad=differences(w,&r->after,1);
        for(unsigned k=0;k<sizeof(pixels);++k)bad+=pixels[k]!=native[k];
        fprintf(stderr,"live controls %s: %u differences\n",r->name,bad);failures+=bad!=0;++total;
        npe_rpg_runtime_close(&f);
    }
    printf("Resident story reader: %u original pages/control continuations, %u failed\n",total,failures);

    const char *slot="tmp/story-text-own-slot.npes",*changed="tmp/story-text-own-wire.npes";
    assert(!npe_world_new_game(&f,w)&&!npe_world_save(&f,w,slot));
    unsigned lifetime=0,cold=0;
    for(unsigned bound=0;bound<2;++bound)for(unsigned i=0;i<sizeof(story_text_lifetimes)/sizeof(*story_text_lifetimes);++i){
        const StoryTextLifetime *r=story_text_lifetimes+i;
        assert(!npe_world_new_game(&f,w));
        if(bound){assert(!npe_world_resources_open(&f,w));controls(&f,&r->before);}
        else{w->text_file_position=r->before.pointer;memcpy(w->text_rect,r->before.rect,8);
            w->text_offset=r->before.offset;w->text_more=r->before.more;w->text_style=r->before.style;}
        assert(!differences(w,&r->before,1));
        assert(!(r->load?npe_world_restore(&f,w,slot):npe_world_restart_game(&f,w)));
        unsigned bad=differences(w,&r->after,1);failures+=bad!=0;++lifetime;
        fprintf(stderr,"text lifetime %u binding%u: %u differences\n",i,bound,bad);
        assert(!npe_world_save(&f,w,changed));
        /* Compare cold decoding with and without session text state. */
        NpeWorld *decoded=malloc(sizeof(*decoded)),*plain=malloc(sizeof(*plain));assert(decoded&&plain);
        assert(!npe_world_load(&f,decoded,changed)&&!npe_world_load(&f,plain,slot));
        StoryTextState zero={0};assert(!differences(decoded,&zero,1)&&!differences(plain,&zero,1));
        ++cold;free(decoded);free(plain);
        if(bound)npe_rpg_runtime_close(&f);
    }
    assert(!npe_world_new_game(&f,w)&&!npe_world_save(&f,w,slot));
    const StoryTextState *seed=&story_text_lifetimes[0].before;
    w->text_file_position=seed->pointer;memcpy(w->text_rect,seed->rect,8);
    w->text_offset=seed->offset;w->text_more=seed->more;w->text_style=seed->style;
    assert(!npe_world_save(&f,w,changed));
    FILE *a=fopen(slot,"rb"),*b=fopen(changed,"rb");assert(a&&b);
    int ca,cb;do{ca=fgetc(a);cb=fgetc(b);assert(ca==cb);}while(ca!=EOF);
    assert(!fclose(a)&&!fclose(b));
    assert(!remove(slot)&&!remove(changed));
    printf("Text controls: %u original lifetime comparisons, %u own cold initializations\n",lifetime,cold);
    npe_font_free(&font);free(w);npe_free(&f);return failures?1:0;
}
