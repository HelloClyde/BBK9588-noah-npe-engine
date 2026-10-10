#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {unsigned map;uint8_t before[84],after[84];} MapEncounterLoadCase;
#include "map_encounter_load_native.h"
static void inject(NpeWorld *w,const uint8_t *p){
    assert(!npe_world_resident_write(w,0x52761a,p,56));
    assert(!npe_world_resident_write(w,0x52783a,p+56,28));
}
static void compare(const NpeWorld *w,const uint8_t *expected){
    uint8_t actual[84];assert(!npe_world_resident_read(w,0x52761a,actual,56));
    assert(!npe_world_resident_read(w,0x52783a,actual+56,28));
    for(unsigned i=0;i<84;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"encounter byte%u:C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<sizeof(encounter_load_cases)/sizeof(*encounter_load_cases);++i){
        const MapEncounterLoadCase *c=&encounter_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));inject(&w,c->before);
        assert(!npe_world_load_map_encounter_records(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        uint8_t cross[]={0x12,0x34,0x56,0x78},actual[4];
        assert(!npe_rpg_runtime_write(&f,0x527627,cross,4));
        assert(!npe_rpg_runtime_read(&f,0x527627,actual,4));assert(!memcmp(cross,actual,4));
        assert(npe_world_resident_write(&w,0x527683,cross,2));
        uint8_t tail[2];assert(!npe_rpg_runtime_read(&f,0x527650,tail,2));
        assert(!memcmp(tail,c->after+54,2));
        /* High-byte writes preserve the live low byte after C movement. */
        w.chase_x[0]=0x55aa;uint8_t high=0x81;
        assert(!npe_rpg_runtime_write(&f,0x527842,&high,1));assert(w.chase_x[0]==0x81aa);
        w.chase_y[0]=0x23cd;assert(!npe_rpg_runtime_write(&f,0x527844,&high,1));assert(w.chase_y[0]==0x81cd);
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Map encounter load%u map%u:84 original bytes/shared writes match\n",i,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));const char *save="tmp/map-encounter-self-save.dat";
    assert(!npe_world_save(&f,&w,save));inject(&w,encounter_load_cases[0].before);
    assert(!npe_world_restore(&f,&w,save));compare(&w,encounter_load_cases[0].after);
    inject(&w,encounter_load_cases[0].before);assert(!npe_world_restart_game(&f,&w));
    compare(&w,encounter_load_cases[12].after);
    /* The original cooldown is a complete byte, not a0..5 enum. */
    uint8_t cooldown=255;assert(!npe_world_resident_write(&w,0x527846,&cooldown,1));
    assert(!npe_world_save(&f,&w,save));NpeWorld loaded;
    assert(!npe_world_load(&f,&loaded,save));assert(loaded.chase_cooldown[0]==255);
    assert(loaded.chase_x[0]==w.chase_x[0]&&loaded.chase_y[0]==w.chase_y[0]);
    assert(!remove(save));
    /* A live formation key and its two companions must reach battle entry. */
    assert(!npe_world_init(&f,&w,9,29,13));w.game_mode=3;w.story_event_id=1;
    uint8_t outer[]={1,2};NpeMap map;NpeMapMonster original;
    assert(!npe_map(&f,8,&map)&&!npe_map_monster(&map,0,&original));
    assert(original.left_enemy_id!=outer[0]&&original.right_enemy_id!=outer[1]);
    assert(!npe_world_resident_write(&w,0x527626,outer,2));
    assert(npe_world_prepare_scripted_battle(&f,&w)==1);
    assert(w.battle_enemy_count==3&&w.battle_enemy_ids[0]==1&&w.battle_enemy_ids[2]==2);
    assert(npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Map encounters:14 original loads, shared writes, UI restore/restart and live formation passed");return 0;
}
