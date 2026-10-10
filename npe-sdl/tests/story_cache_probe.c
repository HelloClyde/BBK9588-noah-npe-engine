#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {unsigned progress,mode,next;} StoryCache;
typedef struct {const char *name;unsigned map,restart;StoryCache boot[15],before[15],after[15];} StoryCacheCase;
#include "story_cache_native.h"
static void seed(NpeWorld *w,const StoryCache c[15]){
    for(unsigned i=0;i<15;++i)w->story_events[i]=(NpeMapStoryEvent){.id=(uint16_t)(i+1),
        .story_progress=(uint16_t)c[i].progress,.mode=(uint8_t)c[i].mode,.next_id=(uint16_t)c[i].next};
}
static void check(const NpeWorld *w,const StoryCache c[15]){
    for(unsigned i=0;i<15;++i){NpeMapStoryEvent e;assert(npe_world_story_event(w,i+1,&e)==1&&!e.raw);
        assert(e.id==i+1&&e.story_progress==c[i].progress&&e.mode==c[i].mode&&e.next_id==c[i].next);}
}
static void wire_equal(const char *a,const char *b){
    FILE *fa=fopen(a,"rb"),*fb=fopen(b,"rb");assert(fa&&fb);
    int ca,cb;do{ca=fgetc(fa);cb=fgetc(fb);assert(ca==cb);}while(ca!=EOF);
    assert(!fclose(fa)&&!fclose(fb));
}
int main(int argc,char **argv){assert(argc==2);
    FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long size=ftell(in);
    assert(size>0&&!fseek(in,0,SEEK_SET));uint8_t *bytes=malloc((size_t)size);
    assert(bytes&&fread(bytes,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    NpeWorld baseline;assert(!npe_world_new_game(&file,&baseline));
    for(unsigned i=0;i<sizeof(story_cache_cases)/sizeof(*story_cache_cases);++i){const StoryCacheCase *c=&story_cache_cases[i];
        check(&baseline,c->boot);NpeWorld w=baseline,saved,cold;seed(&w,c->before);
        if(c->restart){assert(!npe_world_restart_game(&file,&w));check(&w,c->after);continue;}
        assert(!npe_world_init(&file,&saved,c->map,1,1));char path[120];snprintf(path,sizeof(path),"tmp/story-cache-%s.npes",c->name);
        assert(!npe_world_save(&file,&saved,path));
        if(!i){const char *other="tmp/story-cache-wire.npes";NpeWorld changed=saved;seed(&changed,c->before);changed.defer_story_events=1;
            assert(!npe_world_save(&file,&changed,other));wire_equal(path,other);assert(!remove(other));}
        assert(!npe_world_restore(&file,&w,path)&&w.map_number==c->map);check(&w,c->after);
        assert(!npe_world_restore(&file,&w,path));check(&w,c->after);
        assert(!npe_world_load(&file,&cold,path));NpeMap map;NpeMapStoryEvent records[15],e;unsigned count;
        assert(!npe_map(&file,c->map-1,&map)&&!npe_map_story_events(&map,records,&count));
        for(unsigned j=0;j<15;++j){assert(npe_world_story_event(&cold,j+1,&e)==1);
            assert(e.story_progress==(j<count?records[j].story_progress:0)&&e.mode==(j<count?records[j].mode:0)&&e.next_id==(j<count?records[j].next_id:0));}
        NpeWorld before=w;in=fopen(path,"wb");assert(in&&fwrite("NPESAV17",1,8,in)==8&&!fclose(in));
        assert(npe_world_restore(&file,&w,path)<0&&!memcmp(&w,&before,sizeof(w))&&!remove(path));
        NpeMap legacy=map;legacy.tail_size=592;assert(!npe_world_load_story_event_cache(&w,&legacy));check(&w,c->after);assert(!w.story_event_count);
        before=w;uint8_t tail[710]={0};tail[652+11*3+2]=16;NpeMap bad=map;bad.tail=tail;bad.tail_size=sizeof(tail);
        assert(npe_world_load_story_event_cache(&w,&bad)<0&&!memcmp(&w,&before,sizeof(w)));
        /* Plausible-looking bytes outside an actual directory are not a table. */
        memset(tail,0,sizeof(tail));const uint8_t plausible[]={0,1,0,3,1,0,1};memcpy(tail+703,plausible,sizeof(plausible));
        assert(npe_world_load_story_event_cache(&w,&bad)<0&&!memcmp(&w,&before,sizeof(w)));
        printf("Story cache %s: original warm fields, repeated restore, cold host decode and transactions passed\n",c->name);
    }
    NpeWorld w=baseline;seed(&w,story_cache_cases[0].before);w.story_event_count=0;w.game_mode=14;w.story_event_id=271;
    assert(npe_world_apply_story_event(&w,271)==1&&w.story_progress==214&&w.game_mode==1&&w.story_event_id==314);
    NpeMapStoryEvent e;assert(npe_world_story_event(&w,256,&e)==0&&npe_world_story_event(&w,16,&e)==0);
    w.game_mode=14;w.story_event_id=16;uint16_t progress=w.story_progress;
    assert(npe_world_apply_story_event(&w,16)==0&&w.game_mode==0&&w.story_progress==progress&&w.story_event_id==16);
    w.defer_story_events=1;w.game_mode=5;w.story_event_id=1;
    assert(npe_world_apply_story_event(&w,271)==1&&w.game_mode==14&&w.story_event_id==271&&w.story_progress==progress&&!w.map_init_pending);
    assert(npe_world_execute_story_event(&w,271)==1&&w.game_mode==1&&w.story_progress==214&&w.story_event_id==314&&w.defer_story_events);
    assert(npe_world_apply_story_event(&w,16)==1&&w.game_mode==14&&w.story_event_id==16&&w.story_progress==214);
    assert(npe_world_execute_story_event(&w,16)==0&&w.game_mode==0&&w.story_event_id==16&&w.story_progress==214);
    npe_free(&file);puts("Story cache:8 original UI cases, fixed15/low-byte service, file count independence, strict directory and cache-independent NPESAV19 passed");return 0;
}
