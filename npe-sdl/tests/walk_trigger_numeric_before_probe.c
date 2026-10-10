/* Run against the recorded preceding production world.c, before the fix. */
#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {const char *entry;unsigned map;uint8_t before[100],after[100];} TriggerLoad;
typedef struct {
    unsigned mode,event,context,map,progress,init,sx,sy,vx,vy,direction,enabled,slot;
    unsigned menuPartyCount,menuStage,menuRow,menuMember;
    uint8_t cache[100],npc[280],monster[56],chase[28];uint16_t viewCache[105];
} TriggerState;
typedef struct {const char *name;unsigned numeric_mode,numeric_event,frames;TriggerState before,after;} TriggerCase;
#include "walk_trigger_native.h"
int main(int argc,char **argv){
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long n=ftell(in);assert(n>0&&!fseek(in,0,SEEK_SET));
    uint8_t *bytes=malloc(n);assert(bytes&&fread(bytes,1,n,in)==(size_t)n&&!fclose(in));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,n,error,sizeof(error)));
    unsigned differences=0;
    for(unsigned i=0;i<10;++i){const TriggerCase *c=&trigger_services[i];NpeWorld w;
        assert(!npe_world_new_game(&file,&w)&&!npe_world_resident_write(&w,0x527490,c->before.cache,100));
        w.game_mode=20;w.story_event_id=c->before.event;w.map_init_pending=0;
        int result=npe_world_apply_move_event(&file,&w);assert(result>=0);
        unsigned d=w.game_mode!=c->numeric_mode||w.story_event_id!=c->numeric_event||w.map_init_pending!=1;
        fprintf(stderr,"before %s C%u/%u/init%u original%u/%u/init1 differs%u\n",c->name,w.game_mode,w.story_event_id,w.map_init_pending,c->numeric_mode,c->numeric_event,d);differences+=d;
    }
    npe_free(&file);printf("Before fix:%u of10 original mode20 services differ\n",differences);return differences==10?0:1;
}
