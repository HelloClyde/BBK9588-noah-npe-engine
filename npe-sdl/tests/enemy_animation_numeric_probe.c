#include "../npe.h"
#include "enemy_animation_native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(const NpeWorld *w,const EnemyAnimationCase *c,const NegativeHpRecord *ref){
    assert(w->game_mode==3&&!w->battle_lost);
    for(unsigned i=0;i<c->party+3;++i){
        unsigned e=i<c->party?0:i-c->party;NegativeHpRecord r={0};
        r.hp=i<c->party?w->hp[i]:w->battle_enemy_hps[e];
        r.maximum=i<c->party?w->max_hp[i]:w->battle_enemy_max_hps[e];
        r.qi=i<c->party?w->qi[i]:w->battle_enemy_qis[e];
        r.mental=i<c->party?w->other_stats[i][0]:w->battle_enemy_spell_stats[e];
        r.attack=i<c->party?(uint16_t)(w->attack[i]+w->battle_skill_attack_bonus[i]):w->battle_enemy_attacks[e];
        r.defense=i<c->party?(uint16_t)(w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]):w->battle_enemy_defenses[e];
        r.level=i<c->party?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        r.experience=i<c->party?w->experience[i]:w->battle_enemy_experience_rewards[e];
        r.attack_timer=i<c->party?w->battle_attack_turns[i]:w->battle_enemy_attack_turns[e];
        r.defense_timer=i<c->party?w->battle_defense_turns[i]:w->battle_enemy_defense_turns[e];
        memcpy(r.status,i<c->party?w->battle_party_status[i]:w->battle_enemy_status[e],sizeof(r.status));
        if(memcmp(&r,&ref[i],sizeof(r)))fprintf(stderr,"%s record%u HP %u/%u DEF %u/%u\n",c->name,i,r.hp,ref[i].hp,r.defense,ref[i].defense);
        assert(!memcmp(&r,&ref[i],sizeof(r)));
    }
}
int main(int argc,char **argv){
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in);
    assert(!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0);rewind(in);
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile f;char error[160];assert(!npe_parse(&f,bytes,(size_t)size,error,sizeof(error)));
    for(unsigned case_index=0;case_index<sizeof(enemy_animation_cases)/sizeof(*enemy_animation_cases);++case_index){
        const EnemyAnimationCase *c=&enemy_animation_cases[case_index];NpeWorld w;
        assert(!npe_world_init(&f,&w,9,29,13));
        for(unsigned i=0;i<3;++i){w.party_active[i]=(uint8_t)(i<c->party);w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];}
        w.game_mode=3;w.story_event_id=1;assert(npe_world_prepare_scripted_battle(&f,&w)==1);
        for(unsigned i=0;i<c->party;++i){const NegativeHpRecord *r=&c->before[i];
            w.hp[i]=r->hp;w.max_hp[i]=r->maximum;w.qi[i]=r->qi;w.max_qi[i]=60;
            w.other_stats[i][0]=r->mental;w.other_stats[i][1]=(uint16_t)(999-i);w.other_stats[i][2]=1;
            w.attack[i]=r->attack;w.defense[i]=w.battle_party_defenses[i]=r->defense;
            w.level[i]=(uint8_t)r->level;w.experience[i]=r->experience;memcpy(w.battle_party_status[i],r->status,sizeof(r->status));
        }
        for(unsigned i=0;i<3;++i){const NegativeHpRecord *r=&c->before[c->party+i];
            w.battle_enemy_ids[i]=r->hp?51:0;w.battle_enemy_hps[i]=r->hp;w.battle_enemy_max_hps[i]=r->maximum;
            w.battle_enemy_qis[i]=r->qi;w.battle_enemy_spell_stats[i]=r->mental;
            w.battle_enemy_attacks[i]=w.battle_enemy_base_attacks[i]=r->attack;
            w.battle_enemy_defenses[i]=w.battle_enemy_base_defenses[i]=r->defense;
            w.battle_enemy_lucks[i]=1;w.battle_enemy_speeds[i]=(uint16_t)(i==c->enemy&&c->fast?1000:1);
            w.battle_enemy_damage_floor_stats[i]=(uint8_t)r->level;w.battle_enemy_experience_rewards[i]=r->experience;
            memcpy(w.battle_enemy_status[i],r->status,sizeof(r->status));memset(w.battle_enemy_skill_ids[i],0,3);
        }
        /* One initial seed reproduces both native target/variance paths;
           no reseeding between actors or enemy response. */
        npe_world_seed(&w,c->seed);check(&w,c,c->before);
        for(unsigned i=0;i<c->party;++i){
            check(&w,c,c->before);assert(w.battle_actor==i);
            assert((c->actions[i]==5?npe_world_battle_defend(&w):npe_world_battle_attack(&w,c->direct?1:0))==NPE_BATTLE_CONTINUE);
            check(&w,c,i+1<c->party?c->before:c->after);
        }
        assert(w.battle_last_order_count==c->party_actions+1);
        assert(w.battle_last_order[c->fast?0:c->party_actions]==3+c->enemy);
        assert(w.battle_enemy_last_skill[c->enemy]==0&&w.battle_enemy_last_target[c->enemy]==0);
        fprintf(stderr,"Original enemy animation numeric context %s passed\n",c->name);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));npe_free(&f);
    printf("%u original enemy contexts, %u complete record phases passed\n",(unsigned)(sizeof(enemy_animation_cases)/sizeof(*enemy_animation_cases)),ENEMY_ANIMATION_RECORD_PHASES);
    return 0;
}
