#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buff_timers_native.h"

static unsigned compared;
static void phase(const NpeWorld *w,const char *variant,unsigned step) {
    unsigned found=0;
    for(unsigned i=0;i<sizeof(native_buff_phases)/sizeof(*native_buff_phases);++i) {
        const NativeBuffPhase *p=&native_buff_phases[i];
        if(strcmp(p->variant,variant)||p->phase!=step)continue;
        unsigned n=p->member;
        uint16_t attack=(uint16_t)(w->attack[n]+w->battle_skill_attack_bonus[n]);
        uint16_t defense=(uint16_t)(w->battle_party_defenses[n]+w->battle_skill_defense_bonus[n]);
        if(attack!=p->attack||defense!=p->defense||w->battle_attack_turns[n]!=p->attack_timer||
           w->battle_defense_turns[n]!=p->defense_timer||w->qi[n]!=p->qi)
            fprintf(stderr,"%s phase%u member%u: %u/%u timers%u/%u qi%u expected %u/%u timers%u/%u qi%u\n",
                variant,step,n,attack,defense,w->battle_attack_turns[n],w->battle_defense_turns[n],w->qi[n],
                p->attack,p->defense,p->attack_timer,p->defense_timer,p->qi);
        assert(attack==p->attack&&defense==p->defense&&w->battle_attack_turns[n]==p->attack_timer&&
               w->battle_defense_turns[n]==p->defense_timer&&w->qi[n]==p->qi);
        assert(w->attack[n]==p->base_attack&&w->defense[n]==p->base_defense);
        assert((w->hp[n]==0)==p->fallen);
        assert(w->game_mode==3&&w->battle_enemy_hps[1]==500);
        ++found;++compared;
    }
    assert(found==(!strcmp(variant,"group-order")?3u:2u));
}
static NpeWorld fixture(const NpeFile *file,const char *variant) {
    NpeWorld w;
    assert(npe_world_init(file,&w,9,29,13)==0);
    w.game_mode=3;w.story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,&w)==1);
    unsigned count=!strcmp(variant,"group-order")?3:2;
    for(unsigned i=0;i<count;++i) {
        w.party_active[i]=1;w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];w.hp[i]=w.max_hp[i]=10000;
        w.qi[i]=w.max_qi[i]=999;w.other_stats[i][1]=(uint16_t)(999-i);w.level[i]=99;
        w.attack[i]=!strcmp(variant,"caps")?950:!strcmp(variant,"wrap")?32700:15;
        w.defense[i]=w.battle_party_defenses[i]=!strcmp(variant,"caps")?950:!strcmp(variant,"wrap")?32700:13;
    }
    if(!strncmp(variant,"fallen",6))w.hp[1]=0;
    memset(w.condition_words,0,sizeof(w.condition_words));
    w.condition_words[62]=w.condition_words[63]=20;npe_world_seed(&w,1);return w;
}
enum { NOOP=42, SPELL=14, ATTACK_ITEM=62, DEFENSE_ITEM=63, DEFEND=255 };
static void act(const NpeFile *file,NpeWorld *w,unsigned action) {
    unsigned actor=w->battle_actor;
    if(action==DEFEND)assert(npe_world_battle_defend(w)==NPE_BATTLE_CONTINUE);
    else if(action==ATTACK_ITEM||action==DEFENSE_ITEM)
        assert(npe_world_battle_use_item(file,w,action,actor)==NPE_BATTLE_CONTINUE);
    else {
        memset(w->skill_slots[actor],0,sizeof(w->skill_slots[actor]));w->skill_slots[actor][0]=(uint8_t)action;
        assert(npe_world_battle_cast_magic(file,w,action,actor)==NPE_BATTLE_CONTINUE);
    }
}
static void probe(const NpeFile *file) {
    static const char *names[]={"mixed","refresh","defend","expiry","group-order","caps","wrap","death","fallen","fallen-item"};
    static const unsigned actions[][7]={
        {62,63,42,42,42,42},{62,42,62,42,42,42,42},{14,255,42,42,42},
        {14,42,42,42,42},{14,42,42,42,42},{62,63,42,42,42,42},
        {62,63,42,42,42,42},{14,42,42,42,42},{14,42,42,42,42},{62,63,42,42,42,42}};
    static const unsigned rounds[]={6,7,5,5,5,6,6,5,5,6};
    for(unsigned c=0;c<sizeof(names)/sizeof(*names);++c) {
        const char *name=names[c];NpeWorld w=fixture(file,name);phase(&w,name,0);
        unsigned count=c==4?3:2;
        for(unsigned round=0;round<rounds[c];++round) {
            if(c==7&&round==1)w.hp[1]=0;
            for(unsigned actor=0;actor<count;++actor) {
                if(!w.hp[actor])continue;
                assert(w.battle_actor==actor);
                unsigned action=actor==0?actions[c][round]:c==4&&round==0?(actor==1?62:63):42;
                act(file,&w,action);
            }
            phase(&w,name,round+1);
        }
        fprintf(stderr,"Native buff sequence %s passed\n",name);
    }
    printf("Native buff timers: %u party states, independent expiry, refresh, defense, fallen and signed caps passed\n",compared);
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *input=fopen(argv[1],"rb");assert(input);
    assert(fseek(input,0,SEEK_END)==0);long size=ftell(input);assert(size>0&&fseek(input,0,SEEK_SET)==0);
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,input)==(size_t)size);
    assert(fclose(input)==0);NpeFile file;char error[128];
    assert(npe_parse(&file,bytes,(size_t)size,error,sizeof(error))==0);
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    probe(&file);npe_free(&file);return 0;
}
