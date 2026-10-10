#include "../npe.h"
#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "npc_text_probe_helpers.h"
typedef struct {NpeFile *file;NpeWorld *world;uint8_t bitmap[32];unsigned calls;} LoadContext;
static int load(void *context){
    LoadContext *c=context;uint8_t bitmap[32];
    assert(!npe_rpg_runtime_read(c->file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32));
    /* The reader's allocation must already be live when the NPC loader runs. */
    assert(memcmp(bitmap,c->bitmap,32));++c->calls;
    return npe_world_prepare_mode2(c->file,c->world);
}
int main(int argc,char **argv){assert(argc==2||(argc==3&&!strcmp(argv[2],"--legacy")));
    FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long length=ftell(in);
    assert(length>0&&!fseek(in,0,SEEK_SET));uint8_t *bytes=malloc((size_t)length);assert(bytes);
    assert(fread(bytes,1,(size_t)length,in)==(size_t)length&&!fclose(in));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)length,error,sizeof(error)));
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    NpeWorld *w=malloc(sizeof(*w));assert(w);unsigned total=0,failures=0,loads=0;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[sizeof(pixels)];char path[200];
    for(unsigned i=0;i<sizeof(npc_text_cases)/sizeof(*npc_text_cases);++i){const NpcTextCase *r=npc_text_cases+i;
        assert(!npe_world_init(&file,w,1,24,9)&&!npe_world_resources_open(&file,w));
        npc_patch(&file,w,&r->before,r->npc);LoadContext loader={&file,w,{0},0};
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,loader.bitmap,32));
        snprintf(path,sizeof(path),NPC_TEXT_FRAME_PREFIX "%s-before.idx",r->name);npc_frame(path,pixels);
        size_t legacy_start=0;NpeDialoguePage dialogue={0};
        if(argc==3){assert(npe_world_prepare_mode2(&file,w)==1);
            assert(!npe_dialogue_page(&file,w->mode2_dialogue_offset,&dialogue));w->map_init_pending=0;}
        for(unsigned j=0;j<r->count;++j){
            int result;
            if(argc==3){NpeStoryRecord typed={0};typed.text=dialogue.bytes;typed.text_size=dialogue.size;
                size_t next;result=npe_render_native_window_story_page(&font,&typed,legacy_start,&next,pixels);
                legacy_start=next;}
            else result=npe_render_world_npc_page(&file,&font,w,pixels,j==0,load,&loader);
            snprintf(path,sizeof(path),NPC_TEXT_FRAME_PREFIX "%s-page-%u.idx",r->name,j);npc_frame(path,native);
            unsigned bad=result!=0;bad+=npc_state_differences(w,r->pages+j,r->npc);
            if(!result)for(unsigned k=0;k<sizeof(pixels);++k)bad+=pixels[k]!=native[k];
            uint8_t bitmap[32];assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32));
            bad+=memcmp(bitmap,loader.bitmap,32)!=0;
            fprintf(stderr,"%s NPC %s page%u: %u differences\n",argc==3?"legacy typed":"resident",r->name,j,bad);
            failures+=bad!=0;++total;
        }
        if(r->patch&&!r->closes&&!r->shortcut&&!r->ignored){
            npc_patch(&file,w,&r->patch_before,r->npc);
            int result=argc==3?-1:npe_render_world_npc_page(&file,&font,w,pixels,0,NULL,NULL);
            snprintf(path,sizeof(path),NPC_TEXT_FRAME_PREFIX "%s-continue-0.idx",r->name);npc_frame(path,native);
            unsigned bad=result!=0;bad+=npc_state_differences(w,&r->after,r->npc);
            if(!result)for(unsigned k=0;k<sizeof(pixels);++k)bad+=pixels[k]!=native[k];
            fprintf(stderr,"%s NPC %s continuation: %u differences\n",argc==3?"legacy typed":"resident",r->name,bad);
            failures+=bad!=0;++total;
        }
        if(argc==2){assert(loader.calls==1);loads+=loader.calls;}
        npe_rpg_runtime_close(&file);
    }
    printf("NPC reader: %u original pages/continuations, %u failed, %u allocation-before-loader checks\n",total,failures,loads);
    npe_font_free(&font);free(w);npe_free(&file);return failures?1:0;
}
