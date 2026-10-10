#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "attack_animation_native.h"
static unsigned scenario,calls,frame_checked;
static unsigned animation_capture,animation_frames,animation_sequences;
static SDL_Renderer *attack_renderer;
static void attack_frame(void);
static void attack_animation_present(SDL_Renderer *renderer);
static SDL_Renderer *attack_create_renderer(SDL_Window *window,int index,Uint32 flags){attack_renderer=SDL_CreateRenderer(window,index,flags);return attack_renderer;}
static SDL_Window *attack_create_window(const char *title,int x,int y,int width,int height,Uint32 flags){(void)width;(void)height;return SDL_CreateWindow(title,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
static SDL_atomic_t attack_ready,attack_round_ready;
static int attack_new_game(const NpeFile *,NpeWorld *);
static int attack_action(NpeWorld *,unsigned);
static void attack_seed(NpeWorld *,uint32_t);
static int attack_wait(SDL_Event *e,int t){
    if(calls>frame_checked){
        if(animation_frames)assert(animation_frames==15&&!animation_capture);
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
#define npe_world_seed attack_seed
#define SDL_WaitEventTimeout attack_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_CreateRenderer
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef npe_world_seed
#undef npe_world_battle_attack
#undef npe_world_new_game
#undef npe_world_restart_game
static const AttackCase *attack_case(void){return &attack_animation_cases[scenario];}
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
    if(++animation_frames==15)animation_capture=0;
}
static void attack_animation_c(const NpeFile *f,const NpeWorld *w){
    uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[NPE_WIDTH*NPE_HEIGHT],reference[NPE_WIDTH*NPE_HEIGHT];
    assert(!npe_render_battle_actors_scene(f,w,base));
    unsigned target=0;while(!w->battle_enemy_hps[target])++target;
    NpeWorld after=*w;assert(npe_world_battle_attack(&after,target)==NPE_BATTLE_CONTINUE);
    NpeWorld pending=after;memcpy(pending.battle_enemy_hps,w->battle_enemy_hps,sizeof(pending.battle_enemy_hps));
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    for(unsigned commit=0;commit<15;++commit){
        if(commit<2)assert(!npe_render_battle_attack_stage(f,w,target,commit,base));
        if(commit==3)assert(!npe_render_battle_attack_stage(f,&pending,target,2,base));
        if(commit==9||commit==10)assert(!npe_render_battle_round_restore(f,&after,commit-9,base));
        if(commit==11){const uint8_t hp[]={0xcc,0xe5},qi[]={0xc6,0xf8};
            assert(!npe_draw_gbk_text(&font,hp,2,70,88,base)&&!npe_draw_gbk_text(&font,qi,2,70,104,base));
            npe_render_battle_hud_values(after.hp[0],after.max_hp[0],after.qi[0],after.max_qi[0],base);}
        if(commit==12)assert(!npe_render_battle_controls(f,1,base));
        if(commit==13)assert(!npe_render_battle_cursor(f,&after,base));
        if(commit==14)npe_render_battle_status_overlay(&after,base);
        memcpy(pixels,base,sizeof(pixels));
        if(commit==2||commit==4||commit==6)assert(!npe_render_battle_damage_numbers(&pending,(commit-2)/2,pixels));
        attack_animation_reference(commit,reference);
        unsigned differences=0;for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=reference[i];
        fprintf(stderr,"C original %s commit%u: %u differences\n",attack_case()->name,commit,differences);
        if(differences){FILE *out=fopen("tmp/attack-animation-c.idx","wb");assert(out);fwrite(pixels,1,sizeof(pixels),out);fclose(out);}
        assert(!differences);
    }
    npe_font_free(&font);
}
static void attack_seed(NpeWorld *w,uint32_t ignored){
    (void)ignored;
    npe_world_seed(w,4);
}
static void attack_check(const NpeWorld *w,const NegativeHpRecord *reference,unsigned phase){
    assert(w->game_mode==3&&!w->battle_lost&&w->battle_actor==0);
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
    /* Match the original fixture's entry-time outer enemy bytes, so the
       loader installs all three physical poses and coordinate pairs. */
    uint8_t outer[2]={(uint8_t)(scenario?51:0),(uint8_t)(scenario?51:0)};
    assert(!npe_world_resident_write(w,0x527626,outer,sizeof(outer)));
    w->game_mode=3;w->story_event_id=1;assert(npe_world_prepare_scripted_battle(f,w)==1);
    fprintf(stderr,"entry %s count=%u ids=%u,%u,%u coordinates=%d,%d %d,%d %d,%d\n",attack_case()->name,w->battle_enemy_count,w->battle_selected_ids[0],w->battle_selected_ids[1],w->battle_selected_ids[2],npe_world_battle_coordinate(w,1,0,0),npe_world_battle_coordinate(w,1,0,1),npe_world_battle_coordinate(w,1,1,0),npe_world_battle_coordinate(w,1,1,1),npe_world_battle_coordinate(w,1,2,0),npe_world_battle_coordinate(w,1,2,1));
    const NegativeHpRecord *ref=attack_case()->before;
    for(unsigned m=0;m<attack_case()->party;++m){const NegativeHpRecord *r=&ref[m];
        w->hp[m]=r->hp;w->max_hp[m]=r->maximum;w->qi[m]=r->qi;w->max_qi[m]=60;
        w->other_stats[m][0]=r->mental;w->other_stats[m][1]=999-m;w->other_stats[m][2]=1;
        w->attack[m]=r->attack;w->defense[m]=w->battle_party_defenses[m]=r->defense;
        w->level[m]=(uint8_t)r->level;w->experience[m]=r->experience;
        memcpy(w->battle_party_status[m],r->status,8);
    }
    for(unsigned m=0;m<3;++m){
        const NegativeHpRecord *r=&ref[m+attack_case()->party];w->battle_enemy_hps[m]=r->hp;w->battle_enemy_max_hps[m]=r->maximum;w->battle_enemy_qis[m]=r->qi;
        w->battle_enemy_spell_stats[m]=r->mental;w->battle_enemy_lucks[m]=1;w->battle_enemy_speeds[m]=1;
        w->battle_enemy_attacks[m]=w->battle_enemy_base_attacks[m]=r->attack;
        w->battle_enemy_defenses[m]=w->battle_enemy_base_defenses[m]=r->defense;
        w->battle_enemy_damage_floor_stats[m]=(uint8_t)r->level;w->battle_enemy_experience_rewards[m]=r->experience;
        memcpy(w->battle_enemy_status[m],r->status,8);memset(w->battle_enemy_skill_ids[m],0,3);
    }

    memset(w->battle_enemy_ids,51,3);
    calls=0;attack_seed(w,0);attack_check(w,attack_case()->before,0);return 0;
}
static int attack_action(NpeWorld *w,unsigned target){
    attack_check(w,attack_case()->before,1);
    animation_frames=0;
    animation_capture=w->battle_animation_observer!=NULL;
    unsigned check_animation=animation_capture;
    int result=npe_world_battle_attack(w,target);assert(result==NPE_BATTLE_CONTINUE);
    if(check_animation){assert(animation_frames==15&&!animation_capture);++animation_sequences;}
    attack_check(w,attack_case()->after,2);
    assert(w->battle_last_order_count==1&&w->battle_last_order[0]==0);
    ++calls;return result;
}
static void attack_frame(void){
    char path[160];snprintf(path,sizeof(path),"tmp/native-%s-%u.idx",attack_case()->name,2*calls);
    uint8_t ref[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *in=fopen(path,"rb");assert(in);
    assert(fread(ref,1,sizeof(ref),in)==sizeof(ref)&&fgetc(in)==EOF&&!fclose(in));
    assert(!SDL_RenderReadPixels(attack_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<sizeof(ref);++i){unsigned v=ref[i];differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL original ordinary attack %s final: %u differences\n",attack_case()->name,differences);
    if(differences){FILE *out=fopen("tmp/attack-c.rgba","wb");assert(out);fwrite(rgba,1,sizeof(rgba),out);fclose(out);}
    assert(!differences);frame_checked=calls;
}
static int attack_keys(void *ignored){
    (void)ignored;unsigned waited=0;while(!SDL_AtomicGet(&attack_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&attack_ready)){SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);return -1;}
    const SDL_Keycode keys[]={SDLK_RETURN,SDLK_RETURN,SDLK_RETURN};
    unsigned live=0;for(unsigned i=0;i<3;++i)live+=(int16_t)attack_case()->before[i+attack_case()->party].hp>0;
    for(unsigned i=0;i<(live>1?3u:2u);++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];if(SDL_PushEvent(&e)!=1)return -1;SDL_Delay(60);}
    unsigned final_wait=0;while(SDL_AtomicGet(&attack_round_ready)<1&&final_wait<5000){SDL_Delay(10);final_wait+=10;}
    if(SDL_AtomicGet(&attack_round_ready)<1){SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);return -1;}
    SDL_Delay(100);SDL_Event event={0};event.type=SDL_QUIT;return SDL_PushEvent(&event)==1?0:-1;
}
int main(int argc,char **argv){
    assert(argc==2||(argc==3&&!strcmp(argv[2],"--all-frames")));
    NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(scenario=0;scenario<sizeof(attack_animation_cases)/sizeof(*attack_animation_cases);++scenario){
        NpeWorld w;assert(!attack_new_game(&f,&w));
        attack_animation_c(&f,&w);unsigned target=0;while(!w.battle_enemy_hps[target])++target;assert(attack_action(&w,target)==NPE_BATTLE_CONTINUE);
        calls=frame_checked=0;SDL_AtomicSet(&attack_ready,0);SDL_AtomicSet(&attack_round_ready,0);SDL_Thread *thread=SDL_CreateThread(attack_keys,"original attack keys",NULL);assert(thread);
        char *args[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,args));int result=-1;SDL_WaitThread(thread,&result);assert(!result&&calls==1&&frame_checked==calls);
        fprintf(stderr,"C/SDL original ordinary attack %s passed\n",attack_case()->name);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));npe_free(&f);
    assert(animation_sequences==ATTACK_ACTIONS);
    printf("%u original ordinary attacks, %u full record phases, %u final SDL frames and %u C/SDL animation commits passed\n",
        ATTACK_ACTIONS,ATTACK_RECORD_PHASES,
        ATTACK_ACTIONS,animation_sequences*15);return 0;
}
