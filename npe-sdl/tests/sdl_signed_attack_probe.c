#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
static int signed_new_game(const NpeFile *,NpeWorld *);
static int signed_attack(NpeWorld *,unsigned);
static int signed_defend(NpeWorld *);
static void signed_seed(NpeWorld *,uint32_t);
static SDL_atomic_t signed_ready;
static int signed_wait(SDL_Event *event,int timeout){SDL_AtomicSet(&signed_ready,1);return SDL_WaitEventTimeout(event,timeout);}
#define npe_world_new_game signed_new_game
#define npe_world_battle_attack signed_attack
#define npe_world_battle_defend signed_defend
#define npe_world_seed signed_seed
#define SDL_WaitEventTimeout signed_wait
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEventTimeout
#undef npe_world_battle_defend
#undef npe_world_battle_attack
#undef npe_world_new_game
#undef npe_world_seed
#include "signed_attack_native.h"
static const SignedAttackCase *signed_case;
static unsigned signed_calls;
static void signed_seed(NpeWorld *w,uint32_t seed){(void)seed;npe_world_seed(w,4);}
static void signed_check(const NpeWorld *w,const SignedAttackRecord *reference){
    assert(w->game_mode==3&&w->battle_actor==0&&!w->battle_pending_mask);
    for(unsigned i=0;i<4;++i){unsigned enemy=i?i-1:0;SignedAttackRecord r;
        r.hp=i?w->battle_enemy_hps[enemy]:w->hp[0];r.qi=i?w->battle_enemy_qis[enemy]:w->qi[0];
        r.attack=i?w->battle_enemy_attacks[enemy]:(uint16_t)(w->attack[0]+w->battle_skill_attack_bonus[0]);
        r.defense=i?w->battle_enemy_defenses[enemy]:(uint16_t)(w->battle_party_defenses[0]+w->battle_skill_defense_bonus[0]);
        r.attack_timer=i?w->battle_enemy_attack_turns[enemy]:w->battle_attack_turns[0];
        r.defense_timer=i?w->battle_enemy_defense_turns[enemy]:w->battle_defense_turns[0];
        memcpy(r.status,i?w->battle_enemy_status[enemy]:w->battle_party_status[0],8);
        if(memcmp(&r,&reference[i],sizeof(r))){fprintf(stderr,"%s record%u HP %u/%u QI %u/%u ATK %u/%u DEF %u/%u\n",signed_case->name,i,
            r.hp,reference[i].hp,r.qi,reference[i].qi,r.attack,reference[i].attack,r.defense,reference[i].defense);}
        assert(!memcmp(&r,&reference[i],sizeof(r)));
    }
}
static int signed_new_game(const NpeFile *file,NpeWorld *w){
    assert(!npe_world_init(file,w,9,29,13));w->game_mode=3;w->story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,w)==1);
    const SignedAttackRecord *r=&signed_case->before[0];
    w->hp[0]=w->max_hp[0]=r->hp;w->qi[0]=r->qi;w->level[0]=10;
    w->attack[0]=r->attack;w->defense[0]=w->battle_party_defenses[0]=r->defense;
    w->other_stats[0][1]=999;w->other_stats[0][2]=1;memcpy(w->battle_party_status[0],r->status,8);
    for(unsigned i=0;i<3;++i){r=&signed_case->before[i+1];
        w->battle_enemy_hps[i]=w->battle_enemy_max_hps[i]=r->hp;w->battle_enemy_qis[i]=r->qi;
        w->battle_enemy_attacks[i]=w->battle_enemy_base_attacks[i]=r->attack;
        w->battle_enemy_defenses[i]=w->battle_enemy_base_defenses[i]=r->defense;
        memcpy(w->battle_enemy_status[i],r->status,8);w->battle_enemy_speeds[i]=1;
        w->battle_enemy_damage_floor_stats[i]=10;memset(w->battle_enemy_skill_ids[i],0,3);
    }
    // Original control samples isolate rolls0 (party) and1 (enemy).
    // This seed reproduces those rolls, not the original startup RNG state.
    npe_world_seed(w,4);signed_check(w,signed_case->before);return 0;
}
static int signed_attack(NpeWorld *w,unsigned target){
    assert(!signed_case->defending&&target==1);int result=npe_world_battle_attack(w,target);
    assert(result==NPE_BATTLE_CONTINUE);signed_check(w,signed_case->after);++signed_calls;return result;
}
static int signed_defend(NpeWorld *w){
    assert(signed_case->defending);int result=npe_world_battle_defend(w);
    assert(result==NPE_BATTLE_CONTINUE);signed_check(w,signed_case->after);++signed_calls;return result;
}
static int signed_keys(void *unused){
    (void)unused;unsigned waited=0;while(!SDL_AtomicGet(&signed_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&signed_ready))return -1;
    const SDL_Keycode attack[]={SDLK_RETURN,SDLK_RETURN};
    const SDL_Keycode defend[]={SDLK_RETURN,SDLK_LEFT,SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN};
    const SDL_Keycode *keys=signed_case->defending?defend:attack;unsigned count=signed_case->defending?7:2;
    for(unsigned i=0;i<count;++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];if(SDL_PushEvent(&e)!=1)return -1;SDL_Delay(60);}
    SDL_Event e={0};e.type=SDL_QUIT;return SDL_PushEvent(&e)==1?0:-1;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(unsigned i=0;i<sizeof(signed_attack_cases)/sizeof(*signed_attack_cases);++i){
        signed_case=&signed_attack_cases[i];NpeWorld w;assert(!signed_new_game(&file,&w));
        if(signed_case->defending)signed_defend(&w);else signed_attack(&w,1);
        unsigned party_damage=signed_case->before[2].hp-signed_case->after[2].hp;
        assert(signed_case->defending||npe_battle_normal_damage(signed_case->before[0].attack,signed_case->before[2].defense,10,0)==party_damage);
        if(!strncmp(signed_case->name,"enemy-",6)){
            unsigned damage=signed_case->before[0].hp-signed_case->after[0].hp;
            assert(npe_battle_enemy_normal_damage(signed_case->before[2].attack,signed_case->after[0].defense,10,1,signed_case->defending)==damage);
        }
        signed_calls=0;SDL_AtomicSet(&signed_ready,0);SDL_Thread *thread=SDL_CreateThread(signed_keys,"original attack keys",NULL);assert(thread);
        char *arguments[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,arguments));int result=-1;SDL_WaitThread(thread,&result);
        assert(!result&&signed_calls==1);fprintf(stderr,"C/SDL original signed attack %s passed\n",signed_case->name);
    }
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    puts("C and SDL: 17 original ordinary attack/defense sequences, 136 record phases passed");npe_free(&file);return 0;
}
