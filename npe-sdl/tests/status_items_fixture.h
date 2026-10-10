#include "status_items_native.h"
static unsigned status_items_compared;
static const StatusItemPhase *status_item_phase(unsigned scenario,unsigned phase){
    for(unsigned i=0;i<sizeof(status_item_phases)/sizeof(*status_item_phases);++i)
        if(status_item_phases[i].scenario==scenario&&status_item_phases[i].phase==phase)return &status_item_phases[i];
    assert(0);return NULL;
}
static void status_item_check(const NpeWorld *w,unsigned scenario,unsigned phase){
    const StatusItemCase *c=&status_item_cases[scenario];const StatusItemPhase *p=status_item_phase(scenario,phase);
    assert(w->game_mode==3&&w->battle_enemy_count==c->enemies&&!w->battle_pending_mask);
    assert(w->condition_words[69]==p->quantity);
    assert(w->battle_actor==p->actor);
    for(unsigned i=0;i<p->count;++i){
        StatusItemRecord actual;unsigned enemy=i>=c->party?i-c->party:0;
        actual.hp=i<c->party?w->hp[i]:w->battle_enemy_hps[enemy];
        actual.qi=i<c->party?w->qi[i]:w->battle_enemy_qis[enemy];
        for(unsigned s=0;s<4;++s)actual.status[s]=(uint16_t)(i<c->party?w->battle_party_status[i][s]:w->battle_enemy_status[enemy][s]);
        if(memcmp(&actual,&p->records[i],sizeof(actual))){
            fprintf(stderr,"%s phase%u record%u HP %u/%u QI %u/%u statuses:",c->name,phase,i,
                actual.hp,p->records[i].hp,actual.qi,p->records[i].qi);
            for(unsigned s=0;s<4;++s)fprintf(stderr," %u/%u",actual.status[s],p->records[i].status[s]);
            fputc('\n',stderr);
        }
        assert(!memcmp(&actual,&p->records[i],sizeof(actual)));++status_items_compared;
    }
}
static NpeWorld status_item_fixture(const NpeFile *file,unsigned scenario){
    const StatusItemCase *c=&status_item_cases[scenario];const StatusItemPhase *p=status_item_phase(scenario,0);
    NpeWorld w;assert(!npe_world_init(file,&w,9,29,13));w.game_mode=3;w.story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,&w)==1);
    memset(w.party_active,0,sizeof(w.party_active));memset(w.battle_party_status,0,sizeof(w.battle_party_status));
    memset(w.battle_enemy_status,0,sizeof(w.battle_enemy_status));
    memset(w.battle_enemy_hps,0,sizeof(w.battle_enemy_hps));memset(w.battle_enemy_qis,0,sizeof(w.battle_enemy_qis));
    memset(w.condition_words,0,sizeof(w.condition_words));w.condition_words[69]=20;
    for(unsigned i=0;i<c->party;++i){
        w.party_active[i]=1;w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];w.level[i]=99;w.hp[i]=p->records[i].hp;w.max_hp[i]=1000;
        w.qi[i]=p->records[i].qi;w.max_qi[i]=999;w.attack[i]=15;w.defense[i]=w.battle_party_defenses[i]=13;
        w.other_stats[i][1]=(uint16_t)(999-i);
        for(unsigned s=0;s<4;++s)w.battle_party_status[i][s]=(int16_t)p->records[i].status[s];
    }
    for(unsigned i=0;i<3;++i){const StatusItemRecord *r=&p->records[c->party+i];
        w.battle_enemy_hps[i]=r->hp;w.battle_enemy_qis[i]=r->qi;
        for(unsigned s=0;s<4;++s)w.battle_enemy_status[i][s]=(int16_t)r->status[s];
        w.battle_enemy_ids[i]=i<c->enemies?51:0;w.battle_enemy_max_hps[i]=i<c->enemies?10000:0;
        w.battle_enemy_max_qis[i]=i<c->enemies?300:0;
        w.battle_enemy_attacks[i]=w.battle_enemy_base_attacks[i]=i<c->enemies?60:0;
        w.battle_enemy_defenses[i]=w.battle_enemy_base_defenses[i]=i<c->enemies?45:0;
        w.battle_enemy_speeds[i]=0;
    }
    if(c->poison_level)w.battle_enemy_damage_floor_stats[0]=(uint16_t)c->poison_level;
    w.battle_enemy_count=(uint8_t)c->enemies;w.battle_target=w.battle_actor=0;w.battle_enemy_hp=w.battle_enemy_hps[0];
    npe_world_seed(&w,1);return w;
}
static uint8_t *status_item_patch(const NpeFile *file,const StatusItemCase *c,uint8_t saved[42]){
    const uint8_t *raw;size_t size;assert(!npe_record(file,2,68,&raw,&size)&&size>=42);
    uint8_t *bytes=(uint8_t *)raw;memcpy(saved,bytes,42);bytes[12]=2;bytes[36]=(uint8_t)c->group;bytes[37]=(uint8_t)c->side;
    static const unsigned offsets[]={29,31,27,33};
    for(unsigned s=0;s<4;++s){bytes[offsets[s]]=(uint8_t)(c->effects[s]>>8);bytes[offsets[s]+1]=(uint8_t)c->effects[s];}
    return bytes;
}
static void status_item_numeric_action(const NpeFile *file,NpeWorld *w,const StatusItemCase *c,unsigned use){
    int result;unsigned actor=w->battle_actor;
    if(use)result=c->side?npe_world_battle_use_item(file,w,69,c->target):npe_world_battle_throw_item(file,w,69,c->target);
    else if(c->defend)result=npe_world_battle_defend(w);
    else {memset(w->skill_slots[actor],0,sizeof(w->skill_slots[actor]));w->skill_slots[actor][0]=42;
        result=npe_world_battle_cast_magic(file,w,42,actor);}
    assert(result==NPE_BATTLE_CONTINUE);
}
