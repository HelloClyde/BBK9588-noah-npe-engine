#include "compound_native.h"

static unsigned compound_compared;
static const CompoundPhase *compound_phase(unsigned scenario, unsigned phase) {
    for (unsigned i=0;i<sizeof(compound_phases)/sizeof(*compound_phases);++i)
        if (compound_phases[i].scenario==scenario && compound_phases[i].phase==phase)
            return &compound_phases[i];
    assert(0);return NULL;
}
static void compound_check(const NpeWorld *w,unsigned scenario,unsigned phase) {
    const CompoundPhase *p=compound_phase(scenario,phase);
    const CompoundCase *c=&compound_cases[scenario];
    assert(w->game_mode==3 && w->battle_enemy_count==c->enemies && !w->battle_pending_mask);
    if(c->item)assert(w->condition_words[c->id]==p->inventory);
    for(unsigned i=0;i<p->count;++i) {
        CompoundRecord r;
        if(i<2)r=(CompoundRecord){w->hp[i],w->qi[i],w->attack[i],w->defense[i],
            (uint16_t)(w->attack[i]+w->battle_skill_attack_bonus[i]),
            (uint16_t)(w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]),
            w->battle_attack_turns[i],w->battle_defense_turns[i]};
        else {unsigned n=i-2;r=(CompoundRecord){w->battle_enemy_hps[n],w->battle_enemy_qis[n],
            w->battle_enemy_base_attacks[n],w->battle_enemy_base_defenses[n],
            w->battle_enemy_attacks[n],w->battle_enemy_defenses[n],
            w->battle_enemy_attack_turns[n],w->battle_enemy_defense_turns[n]};}
        if(memcmp(&r,&p->records[i],sizeof(r))) {
            const uint16_t *actual=(const uint16_t *)&r,*expected=(const uint16_t *)&p->records[i];
            fprintf(stderr,"%s phase%u record%u actual/expected:",c->name,phase,i);
            for(unsigned k=0;k<8;++k)fprintf(stderr," %u/%u",actual[k],expected[k]);
            fputc('\n',stderr);
        }
        assert(!memcmp(&r,&p->records[i],sizeof(r)));++compound_compared;
    }
}
static NpeWorld compound_fixture(const NpeFile *file,unsigned scenario) {
    const CompoundCase *c=&compound_cases[scenario];const CompoundPhase *p=compound_phase(scenario,0);
    NpeWorld w;assert(!npe_world_init(file,&w,9,29,13));
    w.game_mode=3;w.story_event_id=1;assert(npe_world_prepare_scripted_battle(file,&w)==1);
    for(unsigned i=0;i<2;++i){
        const CompoundRecord *r=&p->records[i];
        w.party_active[i]=1;w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];w.level[i]=99;
        w.hp[i]=r->hp;w.qi[i]=r->qi;w.max_hp[i]=1000;w.max_qi[i]=999;
        if(!strcmp(c->name,"friend-wrap")&&i==0)w.max_hp[i]=w.max_qi[i]=32767;
        if(!strcmp(c->name,"friend-qi-wrap")&&i==0)w.max_qi[i]=32767;
        w.attack[i]=r->base_attack;w.defense[i]=w.battle_party_defenses[i]=r->base_defense;
        w.other_stats[i][1]=(uint16_t)(999-i);
    }
    memset(w.condition_words,0,sizeof(w.condition_words));
    if(c->item)w.condition_words[c->id]=(uint16_t)p->inventory;
    memset(w.battle_enemy_status,0,sizeof(w.battle_enemy_status));
    for(unsigned i=0;i<c->enemies;++i){
        const CompoundRecord *r=&p->records[2+i];
        w.battle_enemy_ids[i]=51;w.battle_enemy_hps[i]=r->hp;w.battle_enemy_max_hps[i]=10000;
        w.battle_enemy_qis[i]=r->qi;w.battle_enemy_max_qis[i]=300;
        w.battle_enemy_attacks[i]=w.battle_enemy_base_attacks[i]=r->base_attack;
        w.battle_enemy_defenses[i]=w.battle_enemy_base_defenses[i]=r->base_defense;
        w.battle_enemy_speeds[i]=0;w.battle_enemy_status[i][2]=-36;
    }
    for(unsigned i=c->enemies;i<3;++i)w.battle_enemy_hps[i]=0;
    w.battle_enemy_count=(uint8_t)c->enemies;w.battle_target=0;w.battle_actor=0;
    w.battle_enemy_hp=w.battle_enemy_hps[0];npe_world_seed(&w,1);return w;
}
static void compound_put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static uint8_t *compound_patch(const NpeFile *file,const CompoundCase *c,unsigned refresh,uint8_t original[42]) {
    const uint8_t *raw;size_t size;
    assert(!npe_record(file,c->item?2:3,c->id-1,&raw,&size) && size>=42);
    uint8_t *bytes=(uint8_t *)raw;memcpy(original,bytes,42);
    unsigned offset=c->item?13:15;
    for(unsigned i=0;i<4;++i)compound_put16(bytes+offset+2*i,
        refresh&&c->refresh_changes?c->refresh_effects[i]:c->effects[i]);
    bytes[c->item?12:14]=(uint8_t)c->type;
    bytes[c->item?36:37]=(uint8_t)c->group;bytes[c->item?37:38]=(uint8_t)c->side;
    return bytes;
}
static void compound_numeric_action(const NpeFile *file,NpeWorld *w,const CompoundCase *c,unsigned use) {
    unsigned actor=w->battle_actor,target=c->side?actor:0;int result;
    if(use&&c->item)result=c->side?npe_world_battle_use_item(file,w,c->id,target):
        npe_world_battle_throw_item(file,w,c->id,target);
    else {unsigned spell=use?c->id:42;
        memset(w->skill_slots[actor],0,sizeof(w->skill_slots[actor]));w->skill_slots[actor][0]=(uint8_t)spell;
        result=npe_world_battle_cast_magic(file,w,spell,use?target:actor);
    }
    assert(result==NPE_BATTLE_CONTINUE);
}
