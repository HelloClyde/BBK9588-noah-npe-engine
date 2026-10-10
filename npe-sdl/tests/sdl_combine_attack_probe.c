#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "battle_menu_actor_check.h"
#include "combine_native.h"
#include "battle_execution_check.h"
typedef struct {const char *name;unsigned phases;uint8_t targets[5];
#ifdef NPE_BATTLE_TYPE_WORDS
uint16_t type_words[5][3];
#endif
} CombineTarget;
#ifdef NPE_BATTLE_TYPE_WORDS
#include "battle_combine_type_words_native.h"
#else
#include "battle_combine_targets_native.h"
#endif
static unsigned scenario,calls,frame_checked;
static unsigned target_route;
static unsigned animation_capture,animation_frames,animation_sequences,animation_round;
static unsigned drawn_icon,repeat_icon_checked;
static SDL_Renderer *combine_renderer;
static const NpeWorld *execution_world;
static uint8_t c_actors[NPE_WIDTH*NPE_HEIGHT];
static void combine_frame(void);
static void combine_animation_present(SDL_Renderer *renderer);
static int combine_icon_selection(unsigned,uint8_t *);
static int combine_controls(const NpeFile *,unsigned,uint8_t *);
static SDL_Renderer *combine_create_renderer(SDL_Window *window,int index,Uint32 flags){combine_renderer=SDL_CreateRenderer(window,index,flags);return combine_renderer;}
static SDL_Window *combine_create_window(const char *title,int x,int y,int width,int height,Uint32 flags){(void)width;(void)height;return SDL_CreateWindow(title,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
static SDL_atomic_t combine_ready,combine_round_ready;
static int combine_new_game(const NpeFile *,NpeWorld *);
static int combine_action(NpeWorld *);
static void combine_seed(NpeWorld *,uint32_t);
static int combine_wait(SDL_Event *e,int t){
    if(calls>frame_checked){
        if(animation_frames)assert(animation_frames==28&&!animation_capture);
        combine_frame();
        SDL_AtomicSet(&combine_round_ready,(int)calls);
    }
    SDL_AtomicSet(&combine_ready,1);return SDL_WaitEventTimeout(e,t);
}
#define SDL_CreateWindow combine_create_window
#define SDL_CreateRenderer combine_create_renderer
#define SDL_RenderPresent combine_animation_present
#define npe_render_battle_icon_selection combine_icon_selection
#define npe_render_battle_controls combine_controls
#define npe_world_new_game combine_new_game
#define npe_world_restart_game combine_new_game
#define npe_world_battle_combine_attack combine_action
#define npe_world_seed combine_seed
#define SDL_WaitEventTimeout combine_wait
#define main npe_app_main
#include "scene_wait_input.h"
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_CreateRenderer
#undef SDL_RenderPresent
#undef npe_render_battle_icon_selection
#undef npe_render_battle_controls
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef npe_world_seed
#undef npe_world_battle_combine_attack
#undef npe_world_new_game
#undef npe_world_restart_game
static const CombineCase *combine_case(void){return &combine_cases[scenario];}
static int combine_icon_selection(unsigned icon,uint8_t *pixels){
    drawn_icon=icon;return npe_render_battle_icon_selection(icon,pixels);
}
static int combine_controls(const NpeFile *file,unsigned icon,uint8_t *pixels){
    drawn_icon=icon;return npe_render_battle_controls(file,icon,pixels);
}
static int combine_has_animation_reference(void){
    if(combine_case()->rounds==2)return 1;
    const char *name=combine_case()->name;
    return !strcmp(name,"combine-basic") || !strcmp(name,"combine-three-targets") ||
           !strcmp(name,"combine-two-party") || !strcmp(name,"combine-fallen-live-enemies") ||
           !strcmp(name,"combine-one-kill") || !strcmp(name,"combine-low-fallen");
}
static void combine_animation_reference(unsigned frame,uint8_t reference[NPE_WIDTH*NPE_HEIGHT]){
    char path[160];snprintf(path,sizeof(path),"tmp/native-%s%s-commit-%u.idx",combine_case()->name,
        combine_case()->rounds==2?"":"-animation",animation_round*28+frame);
    FILE *in=fopen(path,"rb");assert(in);
    assert(fread(reference,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT && fgetc(in)==EOF && !fclose(in));
}
static void combine_animation_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);
    if(!animation_capture && combine_case()->rounds==2 && calls==1 &&
       frame_checked==1 && drawn_icon==3 && !repeat_icon_checked){
        uint8_t reference[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
        char path[160];snprintf(path,sizeof(path),"tmp/native-%s-3.idx",combine_case()->name);
        FILE *in=fopen(path,"rb");assert(in);
        assert(fread(reference,1,sizeof(reference),in)==sizeof(reference)&&fgetc(in)==EOF&&!fclose(in));
        assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
        unsigned differences=0;for(unsigned i=0;i<sizeof(reference);++i){unsigned v=reference[i];
            differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
        fprintf(stderr,"SDL original %s second icon page: %u differences\n",combine_case()->name,differences);
        if(differences){FILE *out=fopen("tmp/combine-repeat-icon-c.rgba","wb");assert(out);fwrite(rgba,1,sizeof(rgba),out);fclose(out);}
        assert(!differences);repeat_icon_checked=1;
    }
    if(!animation_capture)return;
#ifdef NPE_BATTLE_MENU_ACTOR
    menu_actor_frame_check(execution_world,combine_case()->name,animation_round*28+animation_frames,1);
#endif
#ifdef NPE_BATTLE_EXECUTION_STATE
    execution_frame_check(execution_world,combine_case()->name,animation_frames,animation_round,0);
#endif
    uint8_t reference[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    combine_animation_reference(animation_frames,reference);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;
    for(unsigned i=0;i<sizeof(reference);++i){unsigned v=reference[i];
        differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL original %s animation commit%u: %u differences\n",combine_case()->name,animation_frames,differences);
    assert(!differences);
    if(++animation_frames==28)animation_capture=0;
}
static void combine_animation_c(const NpeFile *f,const NpeWorld *w){
    if(!combine_has_animation_reference())return;
    animation_round=calls;
    uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[NPE_WIDTH*NPE_HEIGHT],reference[NPE_WIDTH*NPE_HEIGHT];
    if(calls){memcpy(base,c_actors,sizeof(base));assert(!npe_render_battle_combine_stage(f,w,base));}
    else assert(!npe_render_battle_combine_prepare(f,w,base));
    unsigned target=0;while(!w->battle_enemy_hps[target])++target;
    const int x[3]={82,42,2},y[3]={16,38,54};
    for(unsigned commit=0;commit<15;++commit){
        memcpy(pixels,base,sizeof(pixels));
        if(commit&1)assert(!npe_render_battle_animation_frame(f,14,commit/2,x[target],y[target],pixels));
        combine_animation_reference(commit,reference);
        unsigned differences=0;for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=reference[i];
        fprintf(stderr,"C original %s animation commit%u: %u differences\n",combine_case()->name,commit,differences);
        if(differences){FILE *out=fopen("tmp/combine-animation-c.idx","wb");assert(out);fwrite(pixels,1,sizeof(pixels),out);fclose(out);}
        assert(!differences);
    }
    NpeWorld after=*w;
    assert(npe_world_battle_combine_attack(&after)==NPE_BATTLE_CONTINUE);
    NpeWorld pending=after;
    memcpy(pending.battle_enemy_hps,w->battle_enemy_hps,sizeof(pending.battle_enemy_hps));
    for(unsigned commit=15;commit<22;++commit){
        if(commit==16)assert(!npe_render_battle_combine_restore(f,&pending,base));
        memcpy(pixels,base,sizeof(pixels));
        if(commit==15||commit==17||commit==19)
            assert(!npe_render_battle_damage_numbers(&pending,(commit-15)/2,pixels));
        combine_animation_reference(commit,reference);
        unsigned differences=0;for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=reference[i];
        fprintf(stderr,"C original %s animation commit%u: %u differences\n",combine_case()->name,commit,differences);
        if(differences){FILE *out=fopen("tmp/combine-animation-c.idx","wb");assert(out);fwrite(pixels,1,sizeof(pixels),out);fclose(out);}
        assert(!differences);
    }
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    for(unsigned commit=22;commit<28;++commit){
        if(commit==22||commit==23)assert(!npe_render_battle_round_restore(f,&after,commit-22,base));
        if(commit==24){
            memcpy(c_actors,base,sizeof(c_actors));
            const uint8_t hp_label[]={0xcc,0xe5},qi_label[]={0xc6,0xf8};
            assert(!npe_draw_gbk_text(&font,hp_label,2,70,88,base));
            assert(!npe_draw_gbk_text(&font,qi_label,2,70,104,base));
            npe_render_battle_hud_values(after.hp[after.battle_actor],after.max_hp[after.battle_actor],
                after.qi[after.battle_actor],after.max_qi[after.battle_actor],base);
        }
        if(commit==25)assert(!npe_render_battle_controls(f,1,base));
        if(commit==26)assert(!npe_render_battle_cursor(f,&after,base));
        if(commit==27)npe_render_battle_status_overlay(&after,base);
        combine_animation_reference(commit,reference);
        unsigned differences=0;for(unsigned i=0;i<sizeof(base);++i)differences+=base[i]!=reference[i];
        fprintf(stderr,"C original %s animation commit%u: %u differences\n",combine_case()->name,commit,differences);
        if(differences){FILE *out=fopen("tmp/combine-animation-c.idx","wb");assert(out);fwrite(base,1,sizeof(base),out);fclose(out);}
        assert(!differences);
    }
    if(combine_case()->rounds==2&&!calls){
        memcpy(pixels,base,sizeof(pixels));assert(!npe_render_battle_controls(f,3,pixels));
        char path[160];snprintf(path,sizeof(path),"tmp/native-%s-3.idx",combine_case()->name);
        FILE *in=fopen(path,"rb");assert(in);assert(fread(reference,1,sizeof(reference),in)==sizeof(reference)&&fgetc(in)==EOF&&!fclose(in));
        unsigned differences=0;for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=reference[i];
        fprintf(stderr,"C original %s second icon page: %u differences\n",combine_case()->name,differences);assert(!differences);
    }
    npe_font_free(&font);
}
static void combine_seed(NpeWorld *w,uint32_t ignored){
    (void)ignored;
    /* One initial seed reproduces the native sampled variance remainders:
       0 for single-round cases, 0 then1 for the two uninterrupted rounds.
       Never reseed between their actions or change production RNG. */
    npe_world_seed(w,combine_case()->rounds==2?30:4);
}
static void combine_check(const NpeWorld *w,const NegativeHpRecord *reference,unsigned phase){
#ifdef NPE_BATTLE_EXECUTION_STATE
    execution_state_check(w,combine_case()->name,target_route,phase);
#endif
    assert(w->game_mode==3&&!w->battle_lost&&w->battle_actor==0);
    for(unsigned i=0;i<sizeof(combine_targets)/sizeof(*combine_targets);++i) {
        const CombineTarget *c=&combine_targets[i];if(strcmp(c->name,combine_case()->name))continue;
        assert(phase<c->phases);
        if(w->battle_display_target!=c->targets[phase])
            fprintf(stderr,"%s phase%u global target C%u original%u\n",c->name,phase,w->battle_display_target,c->targets[phase]);
        assert(w->battle_display_target==c->targets[phase]);
        char path[180];snprintf(path,sizeof(path),"tmp/battle-combine-target-c-%s-%s-phase-%u.bin",c->name,target_route?"main":"direct",phase);
        FILE *out=fopen(path,"wb");assert(out);
        assert(fwrite(&w->battle_display_target,1,1,out)==1&&!fclose(out));
#ifdef NPE_BATTLE_TYPE_WORDS
        const uint16_t values[]={w->battle_selection_item_type,w->battle_selection_magic_type,w->battle_selection_type_copy};
        uint8_t types[6];
        for(unsigned k=0;k<3;++k){
            if(values[k]!=c->type_words[phase][k])fprintf(stderr,"%s phase%u type%u C%u original%u\n",c->name,phase,k,values[k],c->type_words[phase][k]);
            assert(values[k]==c->type_words[phase][k]);types[2*k]=(uint8_t)(values[k]>>8);types[2*k+1]=(uint8_t)values[k];}
        snprintf(path,sizeof(path),"tmp/battle-combine-type-public-c-%s-%s-phase-%u.bin",c->name,target_route?"main":"direct",phase);
        out=fopen(path,"wb");assert(out&&fwrite(types,1,6,out)==6&&!fclose(out));
#endif
    }
    for(unsigned i=0;i<combine_case()->party+3;++i){unsigned e=i<combine_case()->party?0:i-combine_case()->party;NegativeHpRecord r;
        r.hp=i<combine_case()->party?w->hp[i]:w->battle_enemy_hps[e];r.maximum=i<combine_case()->party?w->max_hp[i]:w->battle_enemy_max_hps[e];
        r.qi=i<combine_case()->party?w->qi[i]:w->battle_enemy_qis[e];r.mental=i<combine_case()->party?w->other_stats[i][0]:w->battle_enemy_spell_stats[e];
        r.attack=i<combine_case()->party?(uint16_t)(w->attack[i]+w->battle_skill_attack_bonus[i]):w->battle_enemy_attacks[e];
        r.defense=i<combine_case()->party?(uint16_t)(w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]):w->battle_enemy_defenses[e];
        r.level=i<combine_case()->party?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        r.experience=i<combine_case()->party?w->experience[i]:w->battle_enemy_experience_rewards[e];
        r.attack_timer=i<combine_case()->party?w->battle_attack_turns[i]:w->battle_enemy_attack_turns[e];
        r.defense_timer=i<combine_case()->party?w->battle_defense_turns[i]:w->battle_enemy_defense_turns[e];
        memcpy(r.status,i<combine_case()->party?w->battle_party_status[i]:w->battle_enemy_status[e],8);
        if(memcmp(&r,&reference[i],sizeof(r)))fprintf(stderr,"%s phase%u record%u HP %u/%u QI %u/%u DEF %u/%u\n",
            combine_case()->name,phase,i,r.hp,reference[i].hp,r.qi,reference[i].qi,r.defense,reference[i].defense);
        assert(!memcmp(&r,&reference[i],sizeof(r)));
    }
}
static int combine_new_game(const NpeFile *f,NpeWorld *w){
    assert(!npe_world_init(f,w,9,29,13));
    for(unsigned m=0;m<3;++m){w->party_active[m]=(uint8_t)(m<combine_case()->party);w->party_ids[m]=1;w->party_characters[m]=w->party_characters[0];memcpy(w->party_names[m],w->party_names[0],12);memcpy(w->battle_names[m],w->party_names[0],12);w->battle_characters[m]=w->party_characters[0];}
    /* Original public animation inputs set the outer formation bytes
       before entry. Updating only host IDs later leaves physical graphic
       words from the empty slots and cannot reproduce those sprites. */
    uint8_t outer[2]={51,51};
    if(!strcmp(combine_case()->name,"combine-basic"))outer[0]=outer[1]=0;
    assert(!npe_world_resident_write(w,0x527626,outer,2));
    w->game_mode=3;w->story_event_id=1;assert(npe_world_prepare_scripted_battle(f,w)==1);
    const NegativeHpRecord *ref=combine_case()->before;
    for(unsigned m=0;m<combine_case()->party;++m){const NegativeHpRecord *r=&ref[m];
        w->hp[m]=r->hp;w->max_hp[m]=r->maximum;w->qi[m]=r->qi;w->max_qi[m]=60;
        w->other_stats[m][0]=r->mental;w->other_stats[m][1]=combine_case()->second_fast?(unsigned[]){1,999,2}[m]:999-m;w->other_stats[m][2]=1;
        w->attack[m]=r->attack;w->defense[m]=w->battle_party_defenses[m]=r->defense;
        w->level[m]=(uint8_t)r->level;w->experience[m]=r->experience;
        memcpy(w->battle_party_status[m],r->status,8);
    }
    for(unsigned m=0;m<3;++m){
        const NegativeHpRecord *r=&ref[m+combine_case()->party];w->battle_enemy_hps[m]=r->hp;w->battle_enemy_max_hps[m]=r->maximum;w->battle_enemy_qis[m]=r->qi;
        w->battle_enemy_spell_stats[m]=r->mental;w->battle_enemy_lucks[m]=1;w->battle_enemy_speeds[m]=1;
        w->battle_enemy_attacks[m]=w->battle_enemy_base_attacks[m]=r->attack;
        w->battle_enemy_defenses[m]=w->battle_enemy_base_defenses[m]=r->defense;
        w->battle_enemy_damage_floor_stats[m]=(uint8_t)r->level;w->battle_enemy_experience_rewards[m]=r->experience;
        memcpy(w->battle_enemy_status[m],r->status,8);memset(w->battle_enemy_skill_ids[m],0,3);
    }

    memset(w->battle_enemy_ids,51,3);
    calls=drawn_icon=repeat_icon_checked=0;combine_seed(w,0);combine_check(w,combine_case()->before,0);return 0;
}
static int combine_action(NpeWorld *w){
    execution_world=w;
    combine_check(w,calls?combine_case()->after:combine_case()->before,1+2*calls);
    animation_round=calls; animation_frames=0;
    animation_capture=w->battle_animation_observer && combine_has_animation_reference();
    unsigned check_animation=animation_capture;
    int result=npe_world_battle_combine_attack(w);assert(result==NPE_BATTLE_CONTINUE);
    if(check_animation){assert(animation_frames==28&&!animation_capture);++animation_sequences;}
    combine_check(w,calls?combine_case()->after_second:combine_case()->after,2+2*calls);
    assert(w->battle_last_order_count==1&&w->battle_last_order[0]==(combine_case()->second_fast?1:0));
    ++calls;return result;
}
static void combine_frame(void){
    char path[160];snprintf(path,sizeof(path),"tmp/native-%s-%u.idx",combine_case()->name,2*calls);
    uint8_t ref[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *in=fopen(path,"rb");assert(in);
    assert(fread(ref,1,sizeof(ref),in)==sizeof(ref)&&fgetc(in)==EOF&&!fclose(in));
    assert(!SDL_RenderReadPixels(combine_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<sizeof(ref);++i){unsigned v=ref[i];differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL original combined attack %s final: %u differences\n",combine_case()->name,differences);
    if(differences){FILE *out=fopen("tmp/combine-c.rgba","wb");assert(out);fwrite(rgba,1,sizeof(rgba),out);fclose(out);}
    assert(!differences);frame_checked=calls;
}
static int combine_keys_end(int result){
    SDL_Event event={0};event.type=SDL_QUIT;
    return SDL_PushEvent(&event)==1?result:-1;
}
static int combine_keys(void *ignored){
    (void)ignored;unsigned waited=0;while(!SDL_AtomicGet(&combine_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&combine_ready))return combine_keys_end(-1);
    const SDL_Keycode first[]={SDLK_RETURN,SDLK_DOWN,SDLK_RETURN},next[]={SDLK_DOWN,SDLK_RETURN};
    for(unsigned round=0;round<combine_case()->rounds;++round){
        if(round){unsigned waited=0;while(SDL_AtomicGet(&combine_round_ready)<(int)round&&waited<5000){SDL_Delay(10);waited+=10;}
            if(SDL_AtomicGet(&combine_round_ready)<(int)round)return combine_keys_end(-1);}
        const SDL_Keycode *keys=round?next:first;
        unsigned n=round?2:3;
        for(unsigned i=0;i<n;++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];if(SDL_PushEvent(&e)!=1)return combine_keys_end(-1);SDL_Delay(60);}
    }
    unsigned final_wait=0;while(SDL_AtomicGet(&combine_round_ready)<(int)combine_case()->rounds&&final_wait<5000){SDL_Delay(10);final_wait+=10;}
    if(SDL_AtomicGet(&combine_round_ready)<(int)combine_case()->rounds)return combine_keys_end(-1);
    SDL_Delay(100);return combine_keys_end(0);
}
int main(int argc,char **argv){
    assert(argc==2||(argc==3&&!strcmp(argv[2],"--all-frames")));
    NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(scenario=0;scenario<sizeof(combine_cases)/sizeof(*combine_cases);++scenario){
        target_route=0;
        NpeWorld w;assert(!combine_new_game(&f,&w));
        for(unsigned round=0;round<combine_case()->rounds;++round){combine_animation_c(&f,&w);assert(combine_action(&w)==NPE_BATTLE_CONTINUE);}
        target_route=1;calls=frame_checked=0;SDL_AtomicSet(&combine_ready,0);SDL_AtomicSet(&combine_round_ready,0);SDL_Thread *thread=SDL_CreateThread(combine_keys,"original combine keys",NULL);assert(thread);
        char *args[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,args));int result=-1;SDL_WaitThread(thread,&result);assert(!result&&calls==combine_case()->rounds&&frame_checked==calls);
        if(combine_case()->rounds==2)assert(repeat_icon_checked);
        fprintf(stderr,"C/SDL original combined attack %s passed\n",combine_case()->name);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));npe_free(&f);
    assert(animation_sequences==10);
    printf("%u original combined attacks, %u full record phases, %u final SDL frames and %u C/SDL animation commits passed\n",
        COMBINE_ACTIONS,COMBINE_RECORD_PHASES,
        COMBINE_ACTIONS,animation_sequences*28);return 0;
}
