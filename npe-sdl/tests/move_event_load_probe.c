#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[90],after[90];} MoveLoadCase;
#include "move_event_load_native.h"
static void inject(NpeWorld *w,const uint8_t *p){
    assert(!npe_world_resident_write(w,0x527652,p,50));
    assert(!npe_world_resident_write(w,0x527728,p+50,40));
}
static void compare(const NpeWorld *w,const uint8_t *expected){
    uint8_t actual[90];assert(!npe_world_resident_read(w,0x527652,actual,50));
    assert(!npe_world_resident_read(w,0x527728,actual+50,40));
    for(unsigned i=0;i<90;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"move cache byte%u:C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<sizeof(move_load_cases)/sizeof(*move_load_cases);++i){
        const MoveLoadCase *c=&move_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));inject(&w,c->before);
        assert(!npe_world_load_npc_move_events(&w,&map));
        assert(!npe_world_load_script_walk_events(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        /* The last monster byte and first NPC word now share one span. */
        uint8_t cross[]={0x12,0x34,0x56,0x78},actual[4];
        assert(!npe_rpg_runtime_write(&f,0x527651,cross,4));
        assert(!npe_rpg_runtime_read(&f,0x527651,actual,4));assert(!memcmp(cross,actual,4));
        assert(w.npc_move_events[0].npc_id==0x3456&&w.npc_move_events[0].next_mode==0x78);
        assert(!npe_rpg_runtime_write(&f,0x52765b,cross,4));
        assert(!npe_rpg_runtime_read(&f,0x52765b,actual,4));assert(!memcmp(cross,actual,4));
        assert(!npe_rpg_runtime_write(&f,0x527731,cross,4));
        assert(!npe_rpg_runtime_read(&f,0x527731,actual,4));assert(!memcmp(cross,actual,4));
        NpeWorld before=w;
        assert(npe_world_resident_write(&w,0x527683,cross,2));
        assert(npe_world_resident_write(&w,0x52774f,cross,2));
        assert(!memcmp(&w,&before,sizeof(w)));
        /* Partial writes merge with the current C field, not stale RAM. */
        uint8_t high=0x81;w.npc_move_events[4].x=0x55aa;
        assert(!npe_rpg_runtime_write(&f,0x527680,&high,1));assert(w.npc_move_events[4].x==0x81aa);
        w.script_walk_events[3].y=0x23cd;
        assert(!npe_rpg_runtime_write(&f,0x52774e,&high,1));assert(w.script_walk_events[3].y==0x81cd);
        NpeMapNpcMoveEvent npc;NpeScriptWalkCache hero;
        assert(npe_world_npc_move_event(&w,0x105,&npc)==1&&npc.x==0x81aa);
        assert(npe_world_script_walk_event(&w,0x104,&hero)==1&&hero.y==0x81cd);
        assert(!npe_world_npc_move_event(&w,0x100,&npc)&&!npe_world_npc_move_event(&w,6,&npc));
        assert(!npe_world_script_walk_event(&w,0x100,&hero)&&!npe_world_script_walk_event(&w,5,&hero));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Move load%u %s map%u:90 original bytes/shared writes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/move-event-self-save.dat",*changed_save="tmp/move-event-cache-save.dat";
    assert(!npe_world_save(&f,&w,save));inject(&w,move_load_cases[0].before);
    assert(!npe_world_save(&f,&w,changed_save));
    /* These transient caches keep the existing own-save wire image. */
    uint8_t *a,*b;size_t as,bs;assert(!read_file(save,&a,&as)&&!read_file(changed_save,&b,&bs));
    assert(as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,move_load_cases[4].after);
    inject(&w,move_load_cases[0].before);assert(!npe_world_restart_game(&f,&w));
    compare(&w,move_load_cases[14].after);
    assert(!remove(save)&&!remove(changed_save));
    assert(npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Move events:16 original loads/1440 bytes, shared writes, warm restore/restart and own save passed");return 0;
}
