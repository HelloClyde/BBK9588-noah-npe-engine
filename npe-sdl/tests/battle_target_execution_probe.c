#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../npe.h"
#include "battle_target_execution_native.h"
#include "battle_defend_targets_native.h"

static unsigned checks,execution_checks,post_execution_checks;
static void check(const NpeWorld *w,const NativeTargetCase *c,unsigned phase){
    assert(phase<c->phase_count);
    for(unsigned i=0;i<3;++i){
        if(w->battle_native_targets[i]!=c->targets[phase][i])
            fprintf(stderr,"%s phase%u actor%u target%u/%u\n",c->name,phase,i,
                w->battle_native_targets[i],c->targets[phase][i]);
        assert(w->battle_native_targets[i]==c->targets[phase][i]);
    }
    ++checks;
}
static void observe(void *context,const NpeWorld *w,unsigned action,unsigned target,unsigned phase){
    (void)action;
    const NativeTargetCase *c=context;
    if(phase!=NPE_BATTLE_ANIMATION_BEGIN)return;
    if(w->battle_actor==1 || (c->side && w->battle_actor==2)){
        /* The old post_execution sample is still actor0's state. The
           current defender has now run40:1312 and updated its target. */
        assert(w->battle_native_targets[0]==c->post_execution[0]);
        unsigned index=(unsigned)(c-native_target_cases);
        assert(!memcmp(w->battle_native_targets,native_defend_execution[index],3));
        ++post_execution_checks;return;
    }
    if(w->battle_actor!=0)return;
    assert(!memcmp(w->battle_native_targets,c->execution,3));
    assert(w->battle_commands[0].target==target);
    assert(w->battle_native_targets[0]==target+1+(c->side?0:3));
    ++execution_checks;
}
static void prepare(const NpeFile *f,NpeWorld *w,const NativeTargetCase *c){
    assert(!npe_world_init(f,w,9,29,13));
    for(unsigned m=0;m<3;++m){w->party_active[m]=1;w->party_ids[m]=1;w->party_characters[m]=w->party_characters[0];memcpy(w->party_names[m],w->party_names[0],12);memcpy(w->battle_names[m],w->party_names[0],12);w->battle_characters[m]=w->party_characters[0];}
    w->game_mode=3;w->story_event_id=1;
    assert(npe_world_prepare_scripted_battle(f,w)==1);
    for(unsigned m=0;m<3;++m){
        w->hp[m]=(c->side || m<2)?300:0;w->max_hp[m]=1000;
        w->qi[m]=500;w->max_qi[m]=999;w->level[m]=99;
        w->other_stats[m][0]=w->other_stats[m][2]=1;w->other_stats[m][1]=(uint16_t)(999-m);
        w->attack[m]=1;w->defense[m]=w->battle_party_defenses[m]=0;
        memset(w->battle_party_status[m],0,8);
        w->battle_enemy_hps[m]=m==1?30000:0;w->battle_enemy_max_hps[m]=30000;
        w->battle_enemy_qis[m]=0;w->battle_enemy_spell_stats[m]=1;
        w->battle_enemy_attacks[m]=w->battle_enemy_base_attacks[m]=1;
        w->battle_enemy_defenses[m]=w->battle_enemy_base_defenses[m]=0;
        w->battle_enemy_speeds[m]=w->battle_enemy_lucks[m]=1;
        w->battle_enemy_damage_floor_stats[m]=10;
        memset(w->battle_enemy_status[m],0,8);w->battle_enemy_status[m][2]=-36;
        memset(w->battle_enemy_skill_ids[m],0,3);
    }
    if(!c->side)w->battle_enemy_ids[0]=w->battle_enemy_ids[1];
    memset(w->skill_slots[0],0,18);w->skill_slots[0][0]=6;
    w->music_enabled=0;npe_world_seed(w,4);
    w->battle_animation_observer=observe;w->battle_animation_context=(void *)c;
}
int main(int argc,char **argv){
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in);
    assert(!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0);
    assert(!fseek(in,0,SEEK_SET));uint8_t *bytes=malloc((size_t)size);assert(bytes);
    assert(fread(bytes,1,(size_t)size,in)==(size_t)size && !fclose(in));
    NpeFile f;char error[128];assert(!npe_parse(&f,bytes,(size_t)size,error,sizeof(error)));
    for(unsigned i=0;i<sizeof(native_target_cases)/sizeof(*native_target_cases);++i){
        const NativeTargetCase *c=&native_target_cases[i];NpeWorld w;prepare(&f,&w,c);unsigned phase=0;
        check(&w,c,phase++);unsigned start=execution_checks;
        if(c->side){
            assert(npe_world_battle_select_magic(&f,&w,6)==1);
            assert(npe_world_battle_select_target(&w,1,1)==1);
            assert(npe_world_battle_cast_magic(&f,&w,6,1)==NPE_BATTLE_CONTINUE);
        }else{
            assert(npe_world_battle_select_attack(&w,1)==1);
            assert(npe_world_battle_attack(&w,1)==NPE_BATTLE_CONTINUE);
            /* Original next actor enters the retained sword selector before
               this menu sample. Use that production service in the C replay. */
            assert(npe_world_battle_select_attack(&w,1)==1);
        }
        check(&w,c,phase++);
        if(c->side){assert(npe_world_battle_defend(&w)==NPE_BATTLE_CONTINUE);check(&w,c,phase++);w.hp[1]=(uint16_t)c->hp;}
        else {w.battle_enemy_hps[0]=30000;w.battle_enemy_hps[1]=(uint16_t)c->hp;}
        check(&w,c,phase++);
        assert(npe_world_battle_defend(&w)==NPE_BATTLE_CONTINUE);
        check(&w,c,phase++);assert(phase==c->phase_count && execution_checks==start+1);
        fprintf(stderr,"Original target %s: menu and execution fields passed\n",c->name);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));npe_free(&f);
    assert(post_execution_checks==6);
    printf("6 original target paths,%u menu states,%u begin and%u post-action target checks,zero and negative HP passed\n",checks,execution_checks,post_execution_checks);
    return 0;
}
