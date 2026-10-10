#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
static int attribute_new_game(const NpeFile *,NpeWorld *);
static int attribute_attack(NpeWorld *,unsigned);
static void attribute_seed(NpeWorld *,uint32_t);
static SDL_atomic_t attribute_ready;
static int attribute_wait(SDL_Event *event,int timeout){SDL_AtomicSet(&attribute_ready,1);return SDL_WaitEventTimeout(event,timeout);}
#define npe_world_new_game attribute_new_game
#define npe_world_battle_attack attribute_attack
#define npe_world_seed attribute_seed
#define SDL_WaitEventTimeout attribute_wait
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEventTimeout
#undef npe_world_battle_attack
#undef npe_world_new_game
#undef npe_world_seed
#include "attribute_multiplier_native.h"
static const AttributeCase *attribute_case;
static unsigned attribute_calls;
static void attribute_seed(NpeWorld *w,uint32_t seed){(void)seed;npe_world_seed(w,70768);}
static void attribute_check(const NpeWorld *w,const AttributeRecord *reference){
    assert(w->game_mode==3&&w->battle_actor==0&&!w->battle_pending_mask);
    for(unsigned i=0;i<4;++i){unsigned enemy=i?i-1:0;AttributeRecord r;
        r.attribute=i?w->battle_enemy_lucks[enemy]:w->other_stats[0][2];
        r.hp=i?w->battle_enemy_hps[enemy]:w->hp[0];r.qi=i?w->battle_enemy_qis[enemy]:w->qi[0];
        r.attack=i?w->battle_enemy_attacks[enemy]:(uint16_t)(w->attack[0]+w->battle_skill_attack_bonus[0]);
        r.defense=i?w->battle_enemy_defenses[enemy]:(uint16_t)(w->battle_party_defenses[0]+w->battle_skill_defense_bonus[0]);
        r.attack_timer=i?w->battle_enemy_attack_turns[enemy]:w->battle_attack_turns[0];
        r.defense_timer=i?w->battle_enemy_defense_turns[enemy]:w->battle_defense_turns[0];
        memcpy(r.status,i?w->battle_enemy_status[enemy]:w->battle_party_status[0],8);
        if(memcmp(&r,&reference[i],sizeof(r))){fprintf(stderr,"%s record%u HP %u/%u QI %u/%u ATK %u/%u DEF %u/%u\n",attribute_case->name,i,
            r.hp,reference[i].hp,r.qi,reference[i].qi,r.attack,reference[i].attack,r.defense,reference[i].defense);}
        assert(!memcmp(&r,&reference[i],sizeof(r)));
    }
}
static int attribute_new_game(const NpeFile *file,NpeWorld *w){
    assert(!npe_world_init(file,w,9,29,13));w->game_mode=3;w->story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,w)==1);
    for(unsigned i=0;i<3;++i){if(w->battle_enemy_ids[i]){NpeEnemy enemy;
        assert(!npe_enemy(file,w->battle_enemy_ids[i]-1,&enemy));
        assert(w->battle_enemy_lucks[i]==enemy.luck);
    }}
    const AttributeRecord *r=&attribute_case->before[0];
    w->hp[0]=w->max_hp[0]=r->hp;w->qi[0]=r->qi;w->level[0]=10;
    w->attack[0]=r->attack;w->defense[0]=w->battle_party_defenses[0]=r->defense;
    w->other_stats[0][1]=999;w->other_stats[0][2]=r->attribute;memcpy(w->battle_party_status[0],r->status,8);
    for(unsigned i=0;i<3;++i){r=&attribute_case->before[i+1];
        w->battle_enemy_hps[i]=w->battle_enemy_max_hps[i]=r->hp;w->battle_enemy_qis[i]=r->qi;
        w->battle_enemy_attacks[i]=w->battle_enemy_base_attacks[i]=r->attack;
        w->battle_enemy_defenses[i]=w->battle_enemy_base_defenses[i]=r->defense;
        memcpy(w->battle_enemy_status[i],r->status,8);w->battle_enemy_speeds[i]=1;w->battle_enemy_lucks[i]=r->attribute;
        w->battle_enemy_damage_floor_stats[i]=10;memset(w->battle_enemy_skill_ids[i],0,3);
    }
    // Controlled rolls: party variance0 / attribute1; enemy variance1 / attribute44.
    // This seed reproduces those rolls, not the original startup RNG state.
    npe_world_seed(w,70768);attribute_check(w,attribute_case->before);return 0;
}
static int attribute_attack(NpeWorld *w,unsigned target){
    assert(target==1);int result=npe_world_battle_attack(w,target);
    assert(result==NPE_BATTLE_CONTINUE);attribute_check(w,attribute_case->after);++attribute_calls;return result;
}
static int attribute_keys(void *unused){
    (void)unused;unsigned waited=0;while(!SDL_AtomicGet(&attribute_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&attribute_ready))return -1;
    const SDL_Keycode attack[]={SDLK_RETURN,SDLK_RETURN};
    const SDL_Keycode *keys=attack;unsigned count=2;
    for(unsigned i=0;i<count;++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];if(SDL_PushEvent(&e)!=1)return -1;SDL_Delay(60);}
    SDL_Event e={0};e.type=SDL_QUIT;return SDL_PushEvent(&e)==1?0:-1;
}
int main(int argc,char **argv){
    assert(argc==2);
    for(unsigned i=0;i<sizeof(attribute_cpu_cases)/sizeof(*attribute_cpu_cases);++i){
        assert(npe_battle_attribute_multiplier((unsigned)attribute_cpu_cases[i].actor,
            (unsigned)attribute_cpu_cases[i].target,attribute_cpu_cases[i].roll)==attribute_cpu_cases[i].multiplier);
    }
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(unsigned i=0;i<sizeof(attribute_cases)/sizeof(*attribute_cases);++i){
        attribute_case=&attribute_cases[i];NpeWorld w;assert(!attribute_new_game(&file,&w));
        attribute_attack(&w,1);
        attribute_calls=0;SDL_AtomicSet(&attribute_ready,0);SDL_Thread *thread=SDL_CreateThread(attribute_keys,"original attack keys",NULL);assert(thread);
        char *arguments[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,arguments));int result=-1;SDL_WaitThread(thread,&result);
        assert(!result&&attribute_calls==1);fprintf(stderr,"C/SDL original attribute multiplier %s passed\n",attribute_case->name);
    }
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    puts("C and SDL: 14 original attribute multiplier sequences, 112 record phases; 27 PRG CPU slices passed");npe_free(&file);return 0;
}
