#include "character_names_native.h"
static void name_defaults(const NpeFile *f,const NpeWorld *w){
    for(unsigned i=0;i<3;++i){NpeCharacter c;assert(!npe_character(f,w->party_ids[i]-1,&c));
        assert(!memcmp(w->party_names[i],c.name,12));}
}
static void save_names(const NpeFile *f,NpeWorld *w){
    NpeWorld loaded;uint8_t bytes[4096],original[4096];
    assert(!npe_world_save(f,w,path));size_t n=read_bytes(bytes);memcpy(original,bytes,n);
    assert(!memcmp(bytes,"NPESAV22",8)&&!memcmp(bytes+n-36-NPE_CHARACTER_RECORD_SAVE_BYTES,w->party_names,36));
    assert(!npe_world_load(f,&loaded,path)&&!memcmp(loaded.party_names,w->party_names,36));
    memset(loaded.battle_names,254,36);
    assert(!npe_world_save(f,&loaded,path)&&read_bytes(bytes)==n&&!memcmp(bytes,original,n));
    memset(loaded.party_names,253,36);
    assert(!npe_world_restore(f,&loaded,path)&&!memcmp(loaded.party_names,w->party_names,36));
    /* V20 carries metadata but no names. Missing names are loaded by saved ID. */
    memcpy(bytes,original,n);memcpy(bytes,"NPESAV20",8);write_bytes(bytes,n-36-NPE_CHARACTER_RECORD_SAVE_BYTES);
    assert(!npe_world_load(f,&loaded,path));name_defaults(f,&loaded);
    equal(loaded.party_characters,w->party_characters,3);
    memset(loaded.party_names,252,36);assert(!npe_world_restore(f,&loaded,path));name_defaults(f,&loaded);
    assert(!npe_world_restart_game(f,&loaded));name_defaults(f,&loaded);
    assert(!remove(path));
}
static void alias_names(const NpeFile *f,NpeWorld *w,const NameCase *c){
    assert(npe_world_battle_name(w,0)==w->battle_names[0]);
    if(!c->alias)return;
    memset(w->condition_words,0,sizeof(w->condition_words));
    unsigned target=0;while(target<w->battle_enemy_count&&!w->battle_enemy_hps[target])++target;
    assert(target<w->battle_enemy_count);
    assert(w->battle_actor==0&&npe_world_battle_throw_item(f,w,255,target)==NPE_BATTLE_CONTINUE&&w->battle_actor==1);
    assert(!memcmp(w->party_names,c->reserved,36)&&!memcmp(w->battle_names,c->mutated,36));
    assert(npe_world_battle_previous_actor(w)==1&&w->battle_actor==0);
    assert(!memcmp(w->party_names,c->canceled,36)&&!memcmp(w->battle_names,c->mutated,36));
    /* Word writes always address map slot0, even when that slot is inactive. */
    uint8_t flags=w->party_active[0];w->party_active[0]=0;
    assert(!npe_world_set_inventory_word(w,255,65535)&&npe_world_inventory_word(w,255)==65535);
    assert(w->party_names[0][0]==255&&w->party_names[0][1]==255);
    assert(!memcmp(w->party_names,c->wrap_before,36));
    w->party_active[0]=flags;
    assert(npe_world_battle_throw_item(f,w,255,target)==NPE_BATTLE_CONTINUE&&w->battle_actor==1);
    assert(npe_world_inventory_word(w,255)==65534);
    assert(!memcmp(w->party_names,c->wrap_reserved,36));
    assert(!npe_world_set_inventory_word(w,255,65535));
    assert(npe_world_battle_previous_actor(w)==1&&w->battle_actor==0&&npe_world_inventory_word(w,255)==0);
    assert(!memcmp(w->party_names,c->wrap_canceled,36));
    assert(!memcmp(w->battle_names,c->mutated,36));
    assert(npe_world_set_inventory_word(w,256,7)<0&&!npe_world_battle_name(w,3));
}
