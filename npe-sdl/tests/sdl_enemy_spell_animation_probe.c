#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "battle_animation_entry_fixture.h"
#include "enemy_spell_animation_native.h"
typedef EnemyAnimationCase AttackCase;
#define attack_animation_cases enemy_animation_cases
#define ATTACK_RECORD_PHASES ENEMY_ANIMATION_RECORD_PHASES
#define ATTACK_ACTIONS ENEMY_ANIMATION_ACTIONS
#define ATTACK_COMMITS ENEMY_ANIMATION_COMMITS
static unsigned scenario,calls,frame_checked;
static unsigned sequence_frames(void);
static unsigned animation_capture,animation_frames,animation_sequences;
static SDL_Renderer *attack_renderer;
static void attack_frame(void);
static void attack_animation_present(SDL_Renderer *renderer);
static SDL_Renderer *attack_create_renderer(SDL_Window *window,int index,Uint32 flags){attack_renderer=SDL_CreateRenderer(window,index,flags);return attack_renderer;}
static SDL_Window *attack_create_window(const char *title,int x,int y,int width,int height,Uint32 flags){(void)width;(void)height;return SDL_CreateWindow(title,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
static SDL_atomic_t attack_ready,attack_round_ready;
static int attack_new_game(const NpeFile *,NpeWorld *);
static int attack_action(NpeWorld *,unsigned);
static int defend_action(NpeWorld *);
static void attack_seed(NpeWorld *,uint32_t);
static int attack_wait(SDL_Event *e,int t){
    if(calls==attack_animation_cases[scenario].party&&calls>frame_checked){
        if(animation_frames)assert(animation_frames==sequence_frames()&&!animation_capture);
        attack_frame();
        SDL_AtomicSet(&attack_round_ready,(int)calls);
    }
    SDL_AtomicSet(&attack_ready,1);return SDL_WaitEventTimeout(e,t);
}
#define SDL_CreateWindow attack_create_window
#define SDL_CreateRenderer attack_create_renderer
#define SDL_RenderPresent attack_animation_present
#define npe_world_new_game attack_new_game
#define npe_world_restart_game attack_new_game
#define npe_world_battle_attack attack_action
#define npe_world_battle_defend defend_action
#define npe_world_seed attack_seed
#define SDL_WaitEventTimeout attack_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_CreateRenderer
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_PollEvent
#undef npe_world_seed
#undef npe_world_battle_attack
#undef npe_world_battle_defend
#undef npe_world_new_game
#undef npe_world_restart_game
static const AttackCase *attack_case(void){return &attack_animation_cases[scenario];}
static unsigned sequence_frames(void){return attack_case()->frames;}
static void attack_animation_reference(unsigned frame,uint8_t reference[NPE_WIDTH*NPE_HEIGHT]){
    char path[160];snprintf(path,sizeof(path),"tmp/native-%s-commit-%u.idx",attack_case()->name,frame);
    FILE *in=fopen(path,"rb");assert(in);
    assert(fread(reference,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT && fgetc(in)==EOF && !fclose(in));
}
static void attack_animation_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);
    if(!animation_capture)return;
    uint8_t reference[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    attack_animation_reference(animation_frames,reference);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;
    for(unsigned i=0;i<sizeof(reference);++i){unsigned v=reference[i];
        differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL original %s animation commit%u: %u differences\n",attack_case()->name,animation_frames,differences);
    assert(!differences);
    if(++animation_frames==sequence_frames())animation_capture=0;
}
typedef struct { uint8_t base[NPE_WIDTH*NPE_HEIGHT]; NpeFont font; unsigned frame; } SequenceDisplay;
static void sequence_c_commit(SequenceDisplay *d,const uint8_t *pixels){
    uint8_t reference[NPE_WIDTH*NPE_HEIGHT];attack_animation_reference(d->frame,reference);
    unsigned differences=0;for(unsigned i=0;i<sizeof(reference);++i)differences+=pixels[i]!=reference[i];
    fprintf(stderr,"C original %s commit%u: %u differences\n",attack_case()->name,d->frame,differences);
    if(differences){FILE *out=fopen("tmp/enemy-spell-animation-c.idx","wb");assert(out);fwrite(pixels,1,sizeof(reference),out);fclose(out);}
    assert(!differences);++d->frame;
}
static void sequence_c_observer(void *context,const NpeWorld *w,unsigned action,unsigned target,unsigned phase){
    SequenceDisplay *d=context;uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];assert(action==NPE_BATTLE_ANIMATION_ATTACK||action==NPE_BATTLE_ANIMATION_ENEMY_ATTACK||action==NPE_BATTLE_ANIMATION_DEFEND||action==NPE_BATTLE_ANIMATION_ENEMY_RECOVERY);
    if(phase==NPE_BATTLE_ANIMATION_ACTOR_RESTORE){
        assert(!npe_render_battle_round_restore(w->battle_file,w,0,d->base));sequence_c_commit(d,d->base);return;
    }
    if(phase==NPE_BATTLE_ANIMATION_ROUND_RESTORE){
        sequence_c_commit(d,d->base);
        assert(!npe_render_battle_round_restore(w->battle_file,w,0,d->base));sequence_c_commit(d,d->base);return;
    }
    if(phase==NPE_BATTLE_ANIMATION_BEGIN){
        
        if(action==NPE_BATTLE_ANIMATION_DEFEND){assert(!npe_render_battle_defend_stage(w->battle_file,w,d->base));sequence_c_commit(d,d->base);}
        else if(action==NPE_BATTLE_ANIMATION_ENEMY_RECOVERY){
            static const int x[3][3]={{42,0,0},{82,42,0},{82,42,2}},y[3][3]={{38,0,0},{16,38,0},{16,38,54}};
            NpeSkill skill;assert(!npe_skill(w->battle_file,w->battle_enemy_last_skill[w->battle_animation_enemy]-1,&skill));
            assert(!npe_render_battle_enemy_recovery_stage(w->battle_file,w,target,0,d->base));sequence_c_commit(d,d->base);
            for(unsigned frame=0;frame<npe_animation_frames(w->battle_file,skill.effect_type-1);++frame){
                memcpy(pixels,d->base,sizeof(pixels));assert(!npe_render_battle_animation_frame(w->battle_file,skill.effect_type-1,frame,skill.raw[37]==1?20:x[w->battle_enemy_count-1][target],skill.raw[37]==1?20:y[w->battle_enemy_count-1][target],pixels));
                sequence_c_commit(d,pixels);sequence_c_commit(d,d->base);
            }
        }
        else if(action==NPE_BATTLE_ANIMATION_ENEMY_ATTACK){assert(!npe_render_battle_enemy_attack_stage(w->battle_file,w,target,0,d->base));sequence_c_commit(d,d->base);}
        else for(unsigned stage=0;stage<2;++stage){assert(!npe_render_battle_attack_stage(w->battle_file,w,target,stage,d->base));sequence_c_commit(d,d->base);}
    }else if(phase==NPE_BATTLE_ANIMATION_DAMAGE){
        for(unsigned step=0;step<3;++step){
            memcpy(pixels,d->base,sizeof(pixels));assert(!(action==NPE_BATTLE_ANIMATION_ENEMY_RECOVERY?npe_render_battle_enemy_recovery_numbers(w,step,pixels):action==NPE_BATTLE_ANIMATION_ENEMY_ATTACK?npe_render_battle_party_damage_numbers(w,step,pixels):npe_render_battle_damage_numbers(w,step,pixels)));sequence_c_commit(d,pixels);
            if(!step)assert(!(action==NPE_BATTLE_ANIMATION_ENEMY_RECOVERY?npe_render_battle_enemy_recovery_stage(w->battle_file,w,target,1,d->base):action==NPE_BATTLE_ANIMATION_ENEMY_ATTACK?npe_render_battle_enemy_attack_stage(w->battle_file,w,target,1,d->base):npe_render_battle_attack_stage(w->battle_file,w,target,2,d->base)));
            sequence_c_commit(d,d->base);
        }
    }else{
        assert(phase==NPE_BATTLE_ANIMATION_ROUND_END);
        for(unsigned stage=1;stage<2;++stage){assert(!npe_render_battle_round_restore(w->battle_file,w,stage,d->base));sequence_c_commit(d,d->base);}
        const uint8_t hp[]={0xcc,0xe5},qi[]={0xc6,0xf8};
        assert(!npe_draw_gbk_text(&d->font,hp,2,70,88,d->base)&&!npe_draw_gbk_text(&d->font,qi,2,70,104,d->base));
        npe_render_battle_hud_values(w->hp[w->battle_actor],w->max_hp[w->battle_actor],w->qi[w->battle_actor],w->max_qi[w->battle_actor],d->base);sequence_c_commit(d,d->base);
        assert(!npe_render_battle_controls(w->battle_file,attack_case()->icon,d->base));sequence_c_commit(d,d->base);
        assert(!npe_render_battle_cursor(w->battle_file,w,d->base));sequence_c_commit(d,d->base);
        npe_render_battle_status_overlay(w,d->base);sequence_c_commit(d,d->base);
    }
}
static void attack_animation_c(const NpeFile *f,const NpeWorld *w){
    SequenceDisplay d={0};NpeWorld after=*w;
    assert(!npe_render_battle_actors_scene(f,w,d.base));assert(!npe_font_load(&d.font,"npe-sdl/assets/gbk12-native.bin"));
    after.battle_animation_observer=sequence_c_observer;after.battle_animation_context=&d;
    for(unsigned actor=0;actor<attack_case()->party;++actor)assert((attack_case()->actions[actor]==5?npe_world_battle_defend(&after):npe_world_battle_attack(&after,attack_case()->direct?1:0))==NPE_BATTLE_CONTINUE);
    assert(d.frame==sequence_frames());npe_font_free(&d.font);
}
static void attack_seed(NpeWorld *w,uint32_t ignored){
    (void)ignored;
    /* Match this captured target/variance context with one initial seed;
       original startup state remains unproven. Never reseed between actors. */
    npe_world_seed(w,attack_case()->seed);
}
static void attack_check(const NpeWorld *w,const NegativeHpRecord *reference,unsigned phase){
    assert(w->game_mode==3&&!w->battle_lost);
    unsigned expected_actor=phase==1?calls:phase==2&&calls+1<attack_case()->party?calls+1:phase==2?attack_case()->final_actor:0;
    assert(w->battle_actor==expected_actor);
    for(unsigned i=0;i<attack_case()->party+3;++i){unsigned e=i<attack_case()->party?0:i-attack_case()->party;NegativeHpRecord r;
        r.hp=i<attack_case()->party?w->hp[i]:w->battle_enemy_hps[e];r.maximum=i<attack_case()->party?w->max_hp[i]:w->battle_enemy_max_hps[e];
        r.qi=i<attack_case()->party?w->qi[i]:w->battle_enemy_qis[e];r.mental=i<attack_case()->party?w->other_stats[i][0]:w->battle_enemy_spell_stats[e];
        r.attack=i<attack_case()->party?(uint16_t)(w->attack[i]+w->battle_skill_attack_bonus[i]):w->battle_enemy_attacks[e];
        r.defense=i<attack_case()->party?(uint16_t)(w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]):w->battle_enemy_defenses[e];
        r.level=i<attack_case()->party?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        r.experience=i<attack_case()->party?w->experience[i]:w->battle_enemy_experience_rewards[e];
        r.attack_timer=i<attack_case()->party?w->battle_attack_turns[i]:w->battle_enemy_attack_turns[e];
        r.defense_timer=i<attack_case()->party?w->battle_defense_turns[i]:w->battle_enemy_defense_turns[e];
        memcpy(r.status,i<attack_case()->party?w->battle_party_status[i]:w->battle_enemy_status[e],8);
        if(memcmp(&r,&reference[i],sizeof(r)))fprintf(stderr,"%s phase%u record%u HP %u/%u QI %u/%u DEF %u/%u\n",
            attack_case()->name,phase,i,r.hp,reference[i].hp,r.qi,reference[i].qi,r.defense,reference[i].defense);
        assert(!memcmp(&r,&reference[i],sizeof(r)));
    }
}
static int attack_new_game(const NpeFile *f,NpeWorld *w){
    assert(!npe_world_init(f,w,9,29,13));
    for(unsigned m=0;m<3;++m){w->party_active[m]=(uint8_t)(m<attack_case()->party);w->party_ids[m]=1;w->party_characters[m]=w->party_characters[0];memcpy(w->party_names[m],w->party_names[0],12);memcpy(w->battle_names[m],w->party_names[0],12);w->battle_characters[m]=w->party_characters[0];}
    battle_animation_entry_fixture(w,attack_case()->name);
    w->game_mode=3;w->story_event_id=1;assert(npe_world_prepare_scripted_battle(f,w)==1);
    const NegativeHpRecord *ref=attack_case()->before;
    for(unsigned m=0;m<attack_case()->party;++m){const NegativeHpRecord *r=&ref[m];
        w->hp[m]=r->hp;w->max_hp[m]=r->maximum;w->qi[m]=r->qi;w->max_qi[m]=60;
        w->other_stats[m][0]=r->mental;w->other_stats[m][1]=(uint16_t)(999-m);w->other_stats[m][2]=1;
        w->attack[m]=r->attack;w->defense[m]=w->battle_party_defenses[m]=r->defense;
        w->level[m]=(uint8_t)r->level;w->experience[m]=r->experience;
        memcpy(w->battle_party_status[m],r->status,8);
    }
    for(unsigned m=0;m<3;++m){
        const NegativeHpRecord *r=&ref[m+attack_case()->party];w->battle_enemy_hps[m]=r->hp;w->battle_enemy_max_hps[m]=r->maximum;w->battle_enemy_qis[m]=r->qi;
        w->battle_enemy_spell_stats[m]=r->mental;w->battle_enemy_lucks[m]=1;w->battle_enemy_speeds[m]=(uint16_t)(m==attack_case()->enemy&&attack_case()->fast?1000:1);
        w->battle_enemy_attacks[m]=w->battle_enemy_base_attacks[m]=r->attack;
        w->battle_enemy_defenses[m]=w->battle_enemy_base_defenses[m]=r->defense;
        w->battle_enemy_damage_floor_stats[m]=(uint8_t)r->level;w->battle_enemy_experience_rewards[m]=r->experience;
        memcpy(w->battle_enemy_status[m],r->status,8);memset(w->battle_enemy_skill_ids[m],m==attack_case()->enemy?attack_case()->skill:0,3);
    }

    memset(w->battle_enemy_ids,51,3);
    calls=0;attack_seed(w,0);attack_check(w,attack_case()->before,0);return 0;
}
static int mixed_action(NpeWorld *w,unsigned target,unsigned defending){
    assert((attack_case()->actions[calls]==5)==defending);
    attack_check(w,attack_case()->before,1);
    unsigned last=calls+1==attack_case()->party;
    if(last){animation_frames=0;animation_capture=w->battle_animation_observer!=NULL;}
    unsigned check_animation=last&&animation_capture;
    int result=defending?npe_world_battle_defend(w):npe_world_battle_attack(w,target);assert(result==NPE_BATTLE_CONTINUE);
    if(check_animation){assert(animation_frames==sequence_frames()&&!animation_capture);++animation_sequences;}
    attack_check(w,last?attack_case()->after:attack_case()->before,2);
    if(last){assert(w->battle_last_order_count==attack_case()->party_actions+1);assert(w->battle_last_order[attack_case()->fast?0:attack_case()->party_actions]==3+attack_case()->enemy);}
    ++calls;return result;
}
static int attack_action(NpeWorld *w,unsigned target){return mixed_action(w,target,0);}
static int defend_action(NpeWorld *w){return mixed_action(w,w->battle_actor,1);}
static void attack_frame(void){
    char path[160];snprintf(path,sizeof(path),"tmp/native-%s-%u.idx",attack_case()->name,2*attack_case()->party);
    uint8_t ref[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *in=fopen(path,"rb");assert(in);
    assert(fread(ref,1,sizeof(ref),in)==sizeof(ref)&&fgetc(in)==EOF&&!fclose(in));
    assert(!SDL_RenderReadPixels(attack_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<sizeof(ref);++i){unsigned v=ref[i];differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL original enemy recovery context %s final: %u differences\n",attack_case()->name,differences);
    if(differences){FILE *out=fopen("tmp/attack-c.rgba","wb");assert(out);fwrite(rgba,1,sizeof(rgba),out);fclose(out);}
    assert(!differences);frame_checked=calls;
}
static int attack_keys(void *ignored){
    (void)ignored;unsigned waited=0;while(!SDL_AtomicGet(&attack_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&attack_ready)){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return -1;}
    SDL_Keycode keys[24];unsigned key_count=0,flag=0,defend_selected=0;
    keys[key_count++]=SDLK_RETURN;
    for(unsigned actor=0;actor<attack_case()->party;++actor){
        if(attack_case()->actions[actor]==5){
            if(!flag){keys[key_count++]=SDLK_LEFT;keys[key_count++]=SDLK_DOWN;}flag=1;
            keys[key_count++]=SDLK_RETURN;
            if(!defend_selected){keys[key_count++]=SDLK_DOWN;keys[key_count++]=SDLK_DOWN;}defend_selected=1;
            keys[key_count++]=SDLK_RETURN;
        }else{
            if(flag){keys[key_count++]=SDLK_UP;keys[key_count++]=SDLK_RIGHT;}flag=0;
            keys[key_count++]=SDLK_RETURN;
            if(!attack_case()->direct)keys[key_count++]=SDLK_RETURN;
        }
    }
    for(unsigned i=0;i<key_count;++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];if(SDL_PushEvent(&e)!=1)return -1;SDL_Delay(60);}
    unsigned final_wait=0;while(SDL_AtomicGet(&attack_round_ready)<(int)attack_case()->party&&final_wait<5000){SDL_Delay(10);final_wait+=10;}
    if(SDL_AtomicGet(&attack_round_ready)<(int)attack_case()->party){SDL_Event e={0};e.type=SDL_QUIT;SDL_PushEvent(&e);return -1;}
    SDL_Delay(100);SDL_Event event={0};event.type=SDL_QUIT;return SDL_PushEvent(&event)==1?0:-1;
}
int main(int argc,char **argv){
    assert(argc==2||(argc==3&&!strcmp(argv[2],"--all-frames")));
    NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(scenario=0;scenario<sizeof(attack_animation_cases)/sizeof(*attack_animation_cases);++scenario){
        NpeWorld w;assert(!attack_new_game(&f,&w));
        attack_animation_c(&f,&w);for(unsigned actor=0;actor<attack_case()->party;++actor)assert((attack_case()->actions[actor]==5?defend_action(&w):attack_action(&w,attack_case()->direct?1:0))==NPE_BATTLE_CONTINUE);
        calls=frame_checked=0;SDL_AtomicSet(&attack_ready,0);SDL_AtomicSet(&attack_round_ready,0);SDL_Thread *thread=SDL_CreateThread(attack_keys,"original attack keys",NULL);assert(thread);
        char *args[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,args));int result=-1;SDL_WaitThread(thread,&result);assert(!result&&calls==attack_case()->party&&frame_checked==calls);
        fprintf(stderr,"C/SDL original enemy recovery context %s passed\n",attack_case()->name);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));npe_free(&f);
    assert(animation_sequences==sizeof(attack_animation_cases)/sizeof(*attack_animation_cases));
    printf("%u original defend/enemy recovery actions, %u full record phases, %u final SDL frames and %u C/SDL animation commits passed\n",
        ATTACK_ACTIONS,ATTACK_RECORD_PHASES,
        (unsigned)(sizeof(attack_animation_cases)/sizeof(*attack_animation_cases)),ATTACK_COMMITS);return 0;
}
