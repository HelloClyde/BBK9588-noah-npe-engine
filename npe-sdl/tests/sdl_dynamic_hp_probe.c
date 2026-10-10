#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "dynamic_hp_native.h"
static int dynamic_new_game(const NpeFile *,NpeWorld *);
static int dynamic_attack(NpeWorld *,unsigned);
static int dynamic_defend(NpeWorld *);
static int dynamic_magic(const NpeFile *,NpeWorld *,unsigned,unsigned);
static void dynamic_seed(NpeWorld *,uint32_t);
static unsigned scenario,phase,calls;
static SDL_atomic_t dynamic_ready;
static uint8_t *patched_skill,saved_skill[42];
static int dynamic_wait(SDL_Event *e,int t){SDL_AtomicSet(&dynamic_ready,1);return SDL_WaitEventTimeout(e,t);}
#define npe_world_new_game dynamic_new_game
#define npe_world_restart_game dynamic_new_game
#define npe_world_battle_attack dynamic_attack
#define npe_world_battle_defend dynamic_defend
#define npe_world_battle_cast_magic dynamic_magic
#define npe_world_seed dynamic_seed
#define SDL_WaitEventTimeout dynamic_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef npe_world_seed
#undef npe_world_battle_cast_magic
#undef npe_world_battle_defend
#undef npe_world_battle_attack
#undef npe_world_new_game
#undef npe_world_restart_game

static const DynamicHpCase *dynamic_case(void){return &dynamic_hp_cases[scenario];}
static const DynamicHpPhase *dynamic_reference(unsigned index){
    for(unsigned i=0;i<sizeof(dynamic_hp_phases)/sizeof(*dynamic_hp_phases);++i)
        if(dynamic_hp_phases[i].scenario==scenario&&dynamic_hp_phases[i].phase==index)return &dynamic_hp_phases[i];
    assert(0);return NULL;
}
static void dynamic_seed(NpeWorld *w,uint32_t seed){(void)seed;npe_world_seed(w,dynamic_case()->seed);}
static void dynamic_check(const NpeWorld *w){
    const DynamicHpPhase *p=dynamic_reference(phase);
    assert(w->game_mode==3&&!w->battle_lost&&w->battle_actor==p->actor);
    for(unsigned i=0;i<6;++i){unsigned e=i<3?0:i-3;NegativeHpRecord r;
        r.hp=i<3?w->hp[i]:w->battle_enemy_hps[e];r.maximum=i<3?w->max_hp[i]:w->battle_enemy_max_hps[e];
        r.qi=i<3?w->qi[i]:w->battle_enemy_qis[e];r.mental=i<3?w->other_stats[i][0]:w->battle_enemy_spell_stats[e];
        r.attack=i<3?(uint16_t)(w->attack[i]+w->battle_skill_attack_bonus[i]):w->battle_enemy_attacks[e];
        r.defense=i<3?(uint16_t)(w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]):w->battle_enemy_defenses[e];
        r.level=i<3?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        r.experience=i<3?w->experience[i]:w->battle_enemy_experience_rewards[e];
        r.attack_timer=i<3?w->battle_attack_turns[i]:w->battle_enemy_attack_turns[e];
        r.defense_timer=i<3?w->battle_defense_turns[i]:w->battle_enemy_defense_turns[e];
        memcpy(r.status,i<3?w->battle_party_status[i]:w->battle_enemy_status[e],8);
        if(memcmp(&r,&p->records[i],sizeof(r)))fprintf(stderr,"%s phase%u record%u HP %u/%u QI %u/%u DEF %u/%u\n",
            dynamic_case()->name,phase,i,r.hp,p->records[i].hp,r.qi,p->records[i].qi,r.defense,p->records[i].defense);
        assert(!memcmp(&r,&p->records[i],sizeof(r)));
    }
}
static int dynamic_new_game(const NpeFile *f,NpeWorld *w){
    assert(!patched_skill&&!npe_world_init(f,w,9,29,13));
    for(unsigned m=0;m<3;++m){w->party_active[m]=1;w->party_ids[m]=1;w->party_characters[m]=w->party_characters[0];memcpy(w->party_names[m],w->party_names[0],12);memcpy(w->battle_names[m],w->party_names[0],12);w->battle_characters[m]=w->party_characters[0];}
    w->game_mode=3;w->story_event_id=1;assert(npe_world_prepare_scripted_battle(f,w)==1);
    const NegativeHpRecord *ref=dynamic_reference(0)->records;
    for(unsigned m=0;m<3;++m){const NegativeHpRecord *r=&ref[m];
        w->hp[m]=r->hp;w->max_hp[m]=r->maximum;w->qi[m]=r->qi;w->max_qi[m]=999;
        w->other_stats[m][0]=r->mental;w->other_stats[m][1]=999-m;w->other_stats[m][2]=1;
        w->attack[m]=r->attack;w->defense[m]=w->battle_party_defenses[m]=r->defense;
        w->level[m]=(uint8_t)r->level;w->experience[m]=r->experience;
        memcpy(w->battle_party_status[m],r->status,8);
        r=&ref[m+3];w->battle_enemy_hps[m]=r->hp;w->battle_enemy_max_hps[m]=r->maximum;w->battle_enemy_qis[m]=r->qi;
        w->battle_enemy_spell_stats[m]=r->mental;w->battle_enemy_lucks[m]=1;w->battle_enemy_speeds[m]=1;
        w->battle_enemy_attacks[m]=w->battle_enemy_base_attacks[m]=r->attack;
        w->battle_enemy_defenses[m]=w->battle_enemy_base_defenses[m]=r->defense;
        w->battle_enemy_damage_floor_stats[m]=(uint8_t)r->level;w->battle_enemy_experience_rewards[m]=r->experience;
        memcpy(w->battle_enemy_status[m],r->status,8);memset(w->battle_enemy_skill_ids[m],0,3);
    }
    if(dynamic_case()->kind<2)w->battle_enemy_ids[0]=w->battle_enemy_ids[1];
    memset(w->skill_slots[0],0,18);w->skill_slots[0][0]=6;
    if(dynamic_case()->kind==2){
        const uint8_t *raw;size_t size;assert(!npe_record(f,3,5,&raw,&size)&&size>=42);
        patched_skill=(uint8_t *)raw;memcpy(saved_skill,raw,42);
        patched_skill[14]=1;patched_skill[37]=1;patched_skill[38]=0;
        for(unsigned o=15;o<=21;o+=2){patched_skill[o]=0;patched_skill[o+1]=o==15?40:0;}
        memset(w->battle_enemy_skill_ids[1],6,3);
    }
    phase=calls=0;w->battle_actor=0;dynamic_seed(w,0);dynamic_check(w);return 0;
}
static void dynamic_change(NpeWorld *w){
    if(dynamic_case()->kind==0){w->battle_enemy_hps[0]=30000;w->battle_enemy_hps[1]=(uint16_t)dynamic_case()->changed_hp;}
    else w->hp[1]=(uint16_t)dynamic_case()->changed_hp;
    ++phase;dynamic_check(w);
}
static int dynamic_attack(NpeWorld *w,unsigned target){
    assert(dynamic_case()->kind==0&&phase==0&&target==1);
    int result=npe_world_battle_attack(w,target);assert(result==NPE_BATTLE_CONTINUE);
    ++phase;dynamic_check(w);dynamic_change(w);++calls;return result;
}
static int dynamic_magic(const NpeFile *f,NpeWorld *w,unsigned skill,unsigned target){
    assert(dynamic_case()->kind==1&&phase==0&&skill==6&&target==1);
    int result=npe_world_battle_cast_magic(f,w,skill,target);assert(result==NPE_BATTLE_CONTINUE);
    ++phase;dynamic_check(w);++calls;return result;
}
static int dynamic_defend(NpeWorld *w){
    int result=npe_world_battle_defend(w);assert(result==NPE_BATTLE_CONTINUE);
    ++phase;dynamic_check(w);++calls;
    if(dynamic_case()->kind==1&&phase==2)dynamic_change(w);
    if(dynamic_case()->kind==2){
        assert(w->battle_enemy_last_skill[1]==(phase==3?6:0));
        assert(w->battle_last_order_count==2&&w->battle_last_order[0]==0&&w->battle_last_order[1]==4);
        if(phase==3){memcpy(patched_skill,saved_skill,42);patched_skill=NULL;}
    }
    return result;
}
static void dynamic_save_exit(const NpeFile *f,const NpeWorld *w){
    NpeWorld exit_copy=*w,loaded;exit_copy.story_event_id=0;npe_world_seed(&exit_copy,1);
    assert(npe_world_battle_flee(&exit_copy)==NPE_BATTLE_FLED&&exit_copy.game_mode==1);
    for(unsigned m=0;m<3;++m)exit_copy.party_ids[m]=(uint8_t)(m+1);
    const char *path="tmp/dynamic-hp-exit.npes";
    assert(!npe_world_save(f,&exit_copy,path));
    assert(!npe_world_load(f,&loaded,path));
    assert(!memcmp(exit_copy.hp,loaded.hp,sizeof(loaded.hp))&&!memcmp(exit_copy.max_hp,loaded.max_hp,sizeof(loaded.max_hp)));
    assert(!remove(path));
}
static int dynamic_keys(void *unused){
    (void)unused;unsigned waited=0;while(!SDL_AtomicGet(&dynamic_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&dynamic_ready))return -1;
    SDL_Keycode keys[40];unsigned count=0,round_ends[3]={0};
#define KEY(k) keys[count++]=(k)
    KEY(SDLK_RETURN); /* title */
    if(dynamic_case()->kind==0)KEY(SDLK_RETURN);
    else if(dynamic_case()->kind==1){KEY(SDLK_LEFT);KEY(SDLK_RETURN);KEY(SDLK_RETURN);KEY(SDLK_UP);KEY(SDLK_RETURN);}
    unsigned rounds=dynamic_case()->kind==2?3:dynamic_case()->kind==1?2:1;
    for(unsigned n=0;n<rounds;++n){
        KEY(SDLK_LEFT);KEY(SDLK_DOWN);KEY(SDLK_RETURN);
        if(n==0){KEY(SDLK_DOWN);KEY(SDLK_DOWN);}
        KEY(SDLK_RETURN);round_ends[n]=count-1;
    }
#undef KEY
    for(unsigned i=0;i<count;++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];
        if(SDL_PushEvent(&e)!=1)return -1;SDL_Delay(60);
        if(dynamic_case()->kind==2)
            for(unsigned n=0;n<rounds;++n)if(i==round_ends[n])SDL_Delay(350);
    }
    SDL_Event e={0};e.type=SDL_QUIT;return SDL_PushEvent(&e)==1?0:-1;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(scenario=0;scenario<sizeof(dynamic_hp_cases)/sizeof(*dynamic_hp_cases);++scenario){
        NpeWorld w;assert(!dynamic_new_game(&f,&w));
        if(dynamic_case()->kind==0){dynamic_attack(&w,1);dynamic_defend(&w);}
        else if(dynamic_case()->kind==1){dynamic_magic(&f,&w,6,1);dynamic_defend(&w);dynamic_defend(&w);}
        else for(unsigned n=0;n<3;++n)dynamic_defend(&w);
        assert(phase+1==dynamic_case()->phases&&!patched_skill);
        if(dynamic_case()->kind==2)dynamic_save_exit(&f,&w);
        calls=0;SDL_AtomicSet(&dynamic_ready,0);SDL_Thread *thread=SDL_CreateThread(dynamic_keys,"original dynamic HP keys",NULL);assert(thread);
        char *arguments[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,arguments));int result=-1;SDL_WaitThread(thread,&result);
        fprintf(stderr,"SDL %s result%d phase%u calls%u\n",dynamic_case()->name,result,phase,calls);
        assert(!result&&phase+1==dynamic_case()->phases&&calls==(dynamic_case()->kind==0?2u:3u)&&!patched_skill);
        fprintf(stderr,"C/SDL original dynamic HP %s passed\n",dynamic_case()->name);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));
    puts("C and SDL: 8 original dynamic HP sequences, 210 complete record phases passed");npe_free(&f);return 0;
}
