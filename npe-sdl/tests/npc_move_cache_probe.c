#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {unsigned npc,mode,next,x,y;} Cache;
typedef struct {const char *name;unsigned map,restart;Cache boot[5],before[5],after[5];} CacheCase;
#include "npc_move_cache_native.h"
static void check_event(const NpeMapNpcMoveEvent *e,const Cache *c){
    assert(e->npc_id==c->npc&&e->next_mode==c->mode&&e->next_id==c->next&&e->x==c->x&&e->y==c->y);
}
static void check(const NpeWorld *w,const Cache *c){
    for(unsigned i=0;i<5;++i){NpeMapNpcMoveEvent e;
        assert(npe_world_npc_move_event(w,i+1,&e)==1&&!e.raw);check_event(&e,&c[i]);}
}
static void seed(NpeWorld *w,const Cache *c){
    for(unsigned i=0;i<5;++i)w->npc_move_events[i]=(NpeNpcMoveCache){.npc_id=(uint16_t)c[i].npc,
        .next_mode=(uint8_t)c[i].mode,.next_id=(uint16_t)c[i].next,.x=(uint16_t)c[i].x,.y=(uint16_t)c[i].y};
}
int main(int argc,char **argv){
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));
    long size=ftell(in);assert(size>0&&!fseek(in,0,SEEK_SET));uint8_t *bytes=malloc((size_t)size);
    assert(bytes&&fread(bytes,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    NpeWorld baseline;assert(!npe_world_new_game(&file,&baseline));
    for(unsigned i=0;i<sizeof(cache_cases)/sizeof(*cache_cases);++i){const CacheCase *c=&cache_cases[i];
        NpeWorld saved,w,cold;char path[128];check(&baseline,c->boot);w=baseline;seed(&w,c->before);
        if(c->restart){assert(!npe_world_restart_game(&file,&w));check(&w,c->after);
            printf("NPC move cache %s: original warm constructor fields passed\n",c->name);continue;}
        assert(!npe_world_init(&file,&saved,c->map,1,1));
        snprintf(path,sizeof(path),"tmp/npc-move-cache-%s.npes",c->name);
        assert(!npe_world_save(&file,&saved,path));
        if(!i){const char *other="tmp/npc-move-cache-wire.npes";NpeWorld changed=saved;seed(&changed,c->before);
            assert(!npe_world_save(&file,&changed,other));FILE *left=fopen(path,"rb"),*right=fopen(other,"rb");
            assert(left&&right);int a,b;do{a=fgetc(left);b=fgetc(right);assert(a==b);}while(a!=EOF);
            assert(!fclose(left)&&!fclose(right)&&!remove(other));}
        assert(!npe_world_restore(&file,&w,path)&&w.map_number==c->map);check(&w,c->after);
        NpeMapNpcMoveEvent e;assert(npe_world_npc_move_event(&w,0x105,&e)==1);check_event(&e,&c->after[4]);
        assert(npe_world_npc_move_event(&w,0x100,&e)==0&&npe_world_npc_move_event(&w,6,&e)==0);
        assert(!npe_world_restore(&file,&w,path));check(&w,c->after);
        assert(!npe_world_load(&file,&cold,path));NpeMap map;assert(!npe_map(&file,c->map-1,&map));
        for(unsigned j=0;j<5;++j){int found=npe_map_npc_move_event(&map,j+1,&e);assert(found>=0);
            Cache expected=found?(Cache){e.npc_id,e.next_mode,e.next_id,e.x,e.y}:(Cache){0};
            NpeMapNpcMoveEvent loaded;assert(npe_world_npc_move_event(&cold,j+1,&loaded)==1);check_event(&loaded,&expected);}
        NpeWorld before=w;in=fopen(path,"wb");assert(in&&fwrite("NPESAV17",1,8,in)==8&&!fclose(in));
        assert(npe_world_restore(&file,&w,path)==-1&&!memcmp(&w,&before,sizeof(w))&&!remove(path));
        NpeMap legacy=map;legacy.tail_size=592;
        assert(!npe_world_load_npc_move_events(&w,&legacy)&&!memcmp(&w,&before,sizeof(w)));
        uint8_t bad_tail[703]={0};bad_tail[660]=6;NpeMap bad=map;bad.tail=bad_tail;bad.tail_size=sizeof(bad_tail);
        assert(npe_world_load_npc_move_events(&w,&bad)==-1&&!memcmp(&w,&before,sizeof(w)));
        printf("NPC move cache %s: original warm fields, cold decode, reentry and transactions passed\n",c->name);
    }
    /* The original fixed slot bounds include slot5 although this map has
       only two resource records. Changing the resource after loading must
       not alter the cached service. The pointer belongs to this test file. */
    NpeWorld moved=baseline;NpeMap map;NpeMapNpcMoveEvent source;
    assert(!npe_map(&file,0,&map)&&npe_map_npc_move_event(&map,1,&source)==1);
    moved.npc_move_events[4]=(NpeNpcMoveCache){.npc_id=source.npc_id,.next_mode=1,.next_id=0,.x=4,.y=9};
    moved.game_mode=18;moved.story_event_id=0x105;
    assert(npe_world_apply_npc_move_event(&file,&moved)==1&&moved.game_mode==1&&moved.story_event_id==0);
    assert(moved.npc_override_x[2]==4&&moved.npc_override_y[2]==9);
    NpeWorld stable=baseline;stable.game_mode=18;stable.story_event_id=1;
    uint8_t *raw=(uint8_t *)source.raw;uint8_t old_x=raw[5],old_y=raw[6];raw[5]=raw[6]=255;
    assert(npe_world_apply_npc_move_event(&file,&stable)==1&&stable.npc_override_x[2]==4&&stable.npc_override_y[2]==9);
    raw[5]=old_x;raw[6]=old_y;
    /* Teleport uses the same map loader as map exits. */
    moved=baseline;seed(&moved,cache_cases[0].before);moved.map_number=8;moved.game_mode=16;moved.story_event_id=1;
    assert(!npe_map(&file,7,&map)&&!npe_world_load_teleport_events(&moved,&map));
    assert(npe_world_apply_teleport_event(&file,&moved)==1&&!npe_map(&file,moved.map_number-1,&map));
    for(unsigned j=0;j<5;++j){int found=npe_map_npc_move_event(&map,j+1,&source);assert(found>=0);
        Cache expected=found?(Cache){source.npc_id,source.next_mode,source.next_id,source.x,source.y}:cache_cases[0].before[j];
        NpeMapNpcMoveEvent e;assert(npe_world_npc_move_event(&moved,j+1,&e)==1);check_event(&e,&expected);}
    npe_free(&file);puts("NPC move cache:12 original UI cases,retained service,source isolation,teleport and codec passed");return 0;
}
