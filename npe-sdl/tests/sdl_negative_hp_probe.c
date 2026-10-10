#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "negative_hp_native.h"
static int hp_new_game(const NpeFile *,NpeWorld *);
static int hp_attack(NpeWorld *,unsigned);
static void hp_seed(NpeWorld *,uint32_t);
static const NegativeHpCase *hp_case;
static unsigned hp_calls;
static SDL_atomic_t hp_ready;
static int hp_wait(SDL_Event *e,int timeout){SDL_AtomicSet(&hp_ready,1);return SDL_WaitEventTimeout(e,timeout);}
#define npe_world_new_game hp_new_game
#define npe_world_battle_attack hp_attack
#define npe_world_seed hp_seed
#define SDL_WaitEventTimeout hp_wait
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEventTimeout
#undef npe_world_seed
#undef npe_world_battle_attack
#undef npe_world_new_game

static void hp_seed(NpeWorld *w,uint32_t seed){
    (void)seed;
    /* Controlled party variance0, enemy variance1; second enemy0.
       These seeds reproduce the controls, not original startup RNG. */
    npe_world_seed(w,hp_case->formation?381:4);
}
static void hp_check(const NpeWorld *w,const NegativeHpRecord *ref,unsigned mode){
    assert(w->game_mode==mode&&!w->battle_lost&&!w->battle_pending_mask);
    for(unsigned i=0;i<6;++i){unsigned enemy=i<3?0:i-3;NegativeHpRecord r;
        r.hp=i<3?w->hp[i]:w->battle_enemy_hps[enemy];
        r.maximum=i<3?w->max_hp[i]:w->battle_enemy_max_hps[enemy];
        r.qi=i<3?w->qi[i]:w->battle_enemy_qis[enemy];
        r.mental=i<3?w->other_stats[i][0]:w->battle_enemy_spell_stats[enemy];
        r.attack=i<3?(uint16_t)(w->attack[i]+w->battle_skill_attack_bonus[i]):w->battle_enemy_attacks[enemy];
        r.defense=i<3?(uint16_t)(w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]):w->battle_enemy_defenses[enemy];
        r.level=i<3?w->level[i]:w->battle_enemy_damage_floor_stats[enemy];
        r.experience=i<3?w->experience[i]:w->battle_enemy_experience_rewards[enemy];
        r.attack_timer=i<3?w->battle_attack_turns[i]:w->battle_enemy_attack_turns[enemy];
        r.defense_timer=i<3?w->battle_defense_turns[i]:w->battle_enemy_defense_turns[enemy];
        memcpy(r.status,i<3?w->battle_party_status[i]:w->battle_enemy_status[enemy],8);
        if(memcmp(&r,&ref[i],sizeof(r)))fprintf(stderr,"%s record%u HP %u/%u XP %u/%u level %u/%u\n",
            hp_case->name,i,r.hp,ref[i].hp,r.experience,ref[i].experience,r.level,ref[i].level);
        assert(!memcmp(&r,&ref[i],sizeof(r)));
    }
}
static int hp_new_game(const NpeFile *f,NpeWorld *w){
    assert(!npe_world_init(f,w,9,29,13));
    for(unsigned m=0;m<3;++m){w->party_active[m]=1;w->party_ids[m]=1;w->party_characters[m]=w->party_characters[0];memcpy(w->party_names[m],w->party_names[0],12);memcpy(w->battle_names[m],w->party_names[0],12);w->battle_characters[m]=w->party_characters[0];}
    w->game_mode=3;w->story_event_id=1;assert(npe_world_prepare_scripted_battle(f,w)==1);
    for(unsigned m=0;m<3;++m){const NegativeHpRecord *r=&hp_case->before[m];
        w->hp[m]=r->hp;w->max_hp[m]=r->maximum;w->qi[m]=r->qi;
        w->other_stats[m][0]=r->mental;w->other_stats[m][1]=999-m;w->other_stats[m][2]=1;
        w->attack[m]=r->attack;w->defense[m]=w->battle_party_defenses[m]=r->defense;
        w->level[m]=(uint8_t)r->level;w->experience[m]=r->experience;w->experience_to_next[m]=32767;
        memcpy(w->battle_party_status[m],r->status,8);
        r=&hp_case->before[m+3];
        if(hp_case->formation)w->battle_enemy_ids[m]=w->battle_enemy_ids[1];
        w->battle_enemy_hps[m]=r->hp;w->battle_enemy_max_hps[m]=r->maximum;w->battle_enemy_qis[m]=r->qi;
        w->battle_enemy_spell_stats[m]=r->mental;w->battle_enemy_lucks[m]=1;w->battle_enemy_speeds[m]=1;
        w->battle_enemy_attacks[m]=w->battle_enemy_base_attacks[m]=r->attack;
        w->battle_enemy_defenses[m]=w->battle_enemy_base_defenses[m]=r->defense;
        w->battle_enemy_damage_floor_stats[m]=(uint8_t)r->level;
        w->battle_enemy_experience_rewards[m]=r->experience;
        memcpy(w->battle_enemy_status[m],r->status,8);memset(w->battle_enemy_skill_ids[m],0,3);
    }
    w->battle_actor=(uint8_t)hp_case->actor;hp_seed(w,0);hp_check(w,hp_case->before,3);return 0;
}
static int hp_attack(NpeWorld *w,unsigned target){
    assert(target==(hp_case->formation?2u:1u));
    int result=npe_world_battle_attack(w,target);
    assert(result==(hp_case->mode==5?NPE_BATTLE_WON:NPE_BATTLE_CONTINUE));
    hp_check(w,hp_case->after,hp_case->mode);++hp_calls;return result;
}
static int hp_keys(void *unused){
    (void)unused;unsigned waited=0;
    while(!SDL_AtomicGet(&hp_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&hp_ready))return -1;
    const SDL_Keycode single[]={SDLK_RETURN,SDLK_RETURN};
    const SDL_Keycode multiple[]={SDLK_RETURN,SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_UP,SDLK_RETURN};
    const SDL_Keycode *keys=hp_case->formation?multiple:single;unsigned count=hp_case->formation?6:2;
    for(unsigned i=0;i<count;++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];
        if(SDL_PushEvent(&e)!=1)return -1;SDL_Delay(60);}
    SDL_Event e={0};e.type=SDL_QUIT;return SDL_PushEvent(&e)==1?0:-1;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(unsigned i=0;i<sizeof(negative_hp_cases)/sizeof(*negative_hp_cases);++i){
        hp_case=&negative_hp_cases[i];NpeWorld w;assert(!hp_new_game(&f,&w));
        if(hp_case->formation){w.battle_target=2;assert(npe_world_battle_cycle_target(&w,1)==1);
            assert(npe_world_battle_cycle_target(&w,1)==2);assert(npe_world_battle_cycle_target(&w,1)==1);
            assert(npe_world_battle_cycle_target(&w,-1)==2);}
        hp_attack(&w,hp_case->formation?2:1);
        if(hp_case->mode==5){
            /* Independent already-settled map copy; mode5 is not a
               user save point and no continuous story checkpoint changes. */
            NpeWorld map_copy=w,loaded;map_copy.game_mode=1;
            /* Native RAM deliberately clones hero records; the host
               save format requires its three distinct roster IDs. */
            for(unsigned m=0;m<3;++m)map_copy.party_ids[m]=(uint8_t)(m+1);
            const char *path="tmp/negative-hp-map-copy.npes";
            assert(!npe_world_save(&f,&map_copy,path)&&!npe_world_load(&f,&loaded,path));
            assert(!memcmp(map_copy.hp,loaded.hp,sizeof(loaded.hp)));
            assert(!memcmp(map_copy.max_hp,loaded.max_hp,sizeof(loaded.max_hp)));
            assert(!memcmp(map_copy.experience,loaded.experience,sizeof(loaded.experience)));
            assert(!memcmp(map_copy.other_stats,loaded.other_stats,sizeof(loaded.other_stats)));
            assert(!remove(path));
        }
        hp_calls=0;SDL_AtomicSet(&hp_ready,0);SDL_Thread *thread=SDL_CreateThread(hp_keys,"original HP keys",NULL);assert(thread);
        char *arguments[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,arguments));int result=-1;SDL_WaitThread(thread,&result);
        assert(!result&&hp_calls==1);fprintf(stderr,"C/SDL original HP sequence %s passed\n",hp_case->name);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));
    puts("C and SDL: 14 original HP/settlement sequences, 168 full record phases passed");npe_free(&f);return 0;
}
