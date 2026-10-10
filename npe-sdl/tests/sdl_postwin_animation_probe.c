#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "../text.h"
#include "postwin_animation_native.h"
typedef PostwinAnimationCase AttackCase;
#define attack_animation_cases postwin_animation_cases
#define ATTACK_RECORD_PHASES POSTWIN_ANIMATION_RECORD_PHASES
#define ATTACK_ACTIONS POSTWIN_ANIMATION_ACTIONS
#define ATTACK_COMMITS POSTWIN_ANIMATION_COMMITS
static unsigned scenario,calls,frame_checked;
static int postwin_wait(SDL_Event *);
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
static int item_action(const NpeFile *,NpeWorld *,unsigned,unsigned);
static int postwin_drop(const NpeFile *,const NpeFont *,const NpeWorld *,unsigned,uint8_t *);
static void attack_seed(NpeWorld *,uint32_t);
static int attack_wait(SDL_Event *e,int t){
    if(calls==attack_animation_cases[scenario].party_actions&&calls>frame_checked){
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
#define npe_world_battle_throw_item item_action
#define npe_world_seed attack_seed
#define SDL_WaitEventTimeout attack_wait
#define npe_render_battle_drop_panel postwin_drop
#define SDL_WaitEvent postwin_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef npe_render_battle_drop_panel
#undef main
#undef SDL_CreateRenderer
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_PollEvent
#undef SDL_WaitEvent
#undef npe_world_seed
#undef npe_world_battle_attack
#undef npe_world_battle_defend
#undef npe_world_battle_throw_item
#undef npe_world_new_game
#undef npe_world_restart_game
static int postwin_wait(SDL_Event *event){
    if(calls==attack_animation_cases[scenario].party_actions&&calls>frame_checked){
        assert(animation_frames==sequence_frames()&&!animation_capture);
        attack_frame();SDL_AtomicSet(&attack_round_ready,(int)calls);
    }
    return SDL_WaitEvent(event);
}
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
typedef struct { uint8_t base[NPE_WIDTH*NPE_HEIGHT]; NpeFont font; unsigned frame,poison; } SequenceDisplay;
static void check_drop_stock(const NpeWorld *w,unsigned frame){
    assert(frame<26);
    assert(w->condition_words[48]==postwin_drop_quantities[scenario][frame][0]);
    assert(w->condition_words[50]==postwin_drop_quantities[scenario][frame][1]);
}
static int postwin_drop(const NpeFile *f,const NpeFont *font,const NpeWorld *w,unsigned target,uint8_t *pixels){
    check_drop_stock(w,animation_frames);
    return npe_render_battle_drop_panel(f,font,w,target,pixels);
}
static void sequence_c_commit(SequenceDisplay *d,const uint8_t *pixels){
    uint8_t reference[NPE_WIDTH*NPE_HEIGHT];attack_animation_reference(d->frame,reference);
    unsigned differences=0;for(unsigned i=0;i<sizeof(reference);++i)differences+=pixels[i]!=reference[i];
    fprintf(stderr,"C original %s commit%u: %u differences\n",attack_case()->name,d->frame,differences);
    if(differences){FILE *out=fopen("tmp/postwin-animation-c.idx","wb");assert(out);fwrite(pixels,1,sizeof(reference),out);fclose(out);}
    assert(!differences);++d->frame;
}
static void sequence_c_observer(void *context,const NpeWorld *w,unsigned action,unsigned target,unsigned phase){
    SequenceDisplay *d=context;uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    assert(action==NPE_BATTLE_ANIMATION_ITEM||action==NPE_BATTLE_ANIMATION_DEFEND||action==NPE_BATTLE_ANIMATION_POISON||action==NPE_BATTLE_ANIMATION_VICTORY||action==NPE_BATTLE_ANIMATION_LEVEL||action==NPE_BATTLE_ANIMATION_DROP||action==NPE_BATTLE_ANIMATION_LEARN);
    if(action==NPE_BATTLE_ANIMATION_LEVEL||action==NPE_BATTLE_ANIMATION_DROP||action==NPE_BATTLE_ANIMATION_LEARN){
        if(action==NPE_BATTLE_ANIMATION_DROP)check_drop_stock(w,d->frame);
        assert(!(action==NPE_BATTLE_ANIMATION_LEVEL?npe_render_battle_level_panel(w->battle_file,&d->font,w,target&255u,target>>8,phase,d->base):action==NPE_BATTLE_ANIMATION_DROP?npe_render_battle_drop_panel(w->battle_file,&d->font,w,target,d->base):npe_render_battle_learn_panel(w->battle_file,&d->font,w,target,d->base)));
        if(action!=NPE_BATTLE_ANIMATION_LEVEL||phase!=NPE_BATTLE_ANIMATION_BEGIN)sequence_c_commit(d,d->base);
        return;
    }
    if(action==NPE_BATTLE_ANIMATION_VICTORY){
        assert(phase==NPE_BATTLE_ANIMATION_BEGIN);
        assert(!npe_render_battle_round_restore(w->battle_file,w,0,d->base));sequence_c_commit(d,d->base);
        memcpy(pixels,d->base,sizeof(pixels));assert(!npe_render_battle_rewards(w->battle_file,w,pixels));sequence_c_commit(d,pixels);memcpy(d->base,pixels,sizeof(pixels));
        return;
    }
    if(phase==NPE_BATTLE_ANIMATION_ACTOR_RESTORE){
        assert(!npe_render_battle_round_restore(w->battle_file,w,0,d->base));sequence_c_commit(d,d->base);return;
    }
    if(phase==NPE_BATTLE_ANIMATION_ROUND_RESTORE){
        if(!d->poison){sequence_c_commit(d,d->base);}d->poison=0;
        assert(!npe_render_battle_round_restore(w->battle_file,w,0,d->base));sequence_c_commit(d,d->base);return;
    }
    if(phase==NPE_BATTLE_ANIMATION_BEGIN){
        
        if(action==NPE_BATTLE_ANIMATION_DEFEND){assert(!npe_render_battle_defend_stage(w->battle_file,w,d->base));sequence_c_commit(d,d->base);}
        else {
            static const int px[3][3]={{168,0,0},{136,168,0},{136,168,200}},py[3][3]={{51,0,0},{75,51,0},{75,51,28}};
            NpeCharacter character;NpeItem item;unsigned count=0,compact=0;
            assert(!npe_character(w->battle_file,w->party_ids[w->battle_actor]-1,&character));
            assert(!npe_battle_item(w->battle_file,w->battle_commands[w->battle_actor].resource_id,&item));
            for(unsigned i=0;i<3;++i)if(w->party_active[i]){if(i==w->battle_actor)compact=count;++count;}
            assert(!npe_render_battle_party_spell_stage(w->battle_file,w,target,0,d->base));sequence_c_commit(d,d->base);
            unsigned casting=character.raw[item.target_side==1?86:85];
            if(casting){
                for(unsigned frame=0;frame<npe_animation_frames(w->battle_file,casting-1);++frame){
                    memcpy(pixels,d->base,sizeof(pixels));assert(!npe_render_battle_animation_frame(w->battle_file,casting-1,frame,px[count-1][compact],py[count-1][compact],pixels));
                    sequence_c_commit(d,pixels);sequence_c_commit(d,d->base);
                }
                sequence_c_commit(d,d->base);
            }

        }
    }else if(phase==NPE_BATTLE_ANIMATION_DAMAGE){
        assert(action==NPE_BATTLE_ANIMATION_ITEM||action==NPE_BATTLE_ANIMATION_POISON);
        if(action==NPE_BATTLE_ANIMATION_POISON)d->poison=1;
        for(unsigned step=0;step<3;++step){
            memcpy(pixels,d->base,sizeof(pixels));assert(!(action==NPE_BATTLE_ANIMATION_POISON?npe_render_battle_poison_numbers(w,step,pixels):npe_render_battle_party_spell_numbers(w,step,pixels)));sequence_c_commit(d,pixels);
            if(!step&&action!=NPE_BATTLE_ANIMATION_POISON)assert(!npe_render_battle_party_spell_stage(w->battle_file,w,target,1,d->base));
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
    for(unsigned actor=0;actor<attack_case()->party_actions;++actor)assert((attack_case()->actions[actor]==5?npe_world_battle_defend(&after):npe_world_battle_throw_item(f,&after,attack_case()->item,attack_case()->target))==(actor+1==attack_case()->party_actions?NPE_BATTLE_WON:NPE_BATTLE_CONTINUE));
    assert(d.frame==sequence_frames()-1);
    NpeStoryRecord story;assert(after.game_mode==5&&!npe_story_record(f,after.story_event_id-1,&story));
    size_t end=0;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];assert(!npe_font_load16(&d.font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!npe_render_native_story_page(f,&d.font,&story,0,&end,pixels)&&end==story.text_size);
    sequence_c_commit(&d,pixels);assert(d.frame==sequence_frames());npe_font_free(&d.font);
}
static void attack_seed(NpeWorld *w,uint32_t ignored){
    (void)ignored;
    /* Match this captured target/variance context with one initial seed;
       original startup state remains unproven. Never reseed between actors. */
    npe_world_seed(w,attack_case()->seed);
}
static void attack_check(const NpeWorld *w,const NegativeHpRecord *reference,unsigned phase){
    assert(w->game_mode==(phase==2&&calls+1==attack_case()->party_actions?5:3)&&!w->battle_lost);
    unsigned expected_actor=phase==1?attack_case()->actors[calls]:phase==2&&calls+1<attack_case()->party_actions?attack_case()->actors[calls+1]:phase==2?attack_case()->final_actor:0;
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
    for(unsigned m=0;m<3;++m){w->party_active[m]=(uint8_t)(m<attack_case()->party);w->party_ids[m]=(uint8_t)attack_case()->character;assert(!npe_character_metadata(f,w->party_ids[m]-1,&w->party_characters[m]));{NpeCharacter c;assert(!npe_character(f,w->party_ids[m]-1,&c));memcpy(w->party_names[m],c.name,12);memcpy(w->battle_names[m],c.name,12);}}
    w->game_mode=3;w->story_event_id=1;assert(npe_world_prepare_scripted_battle(f,w)==1);
    const NegativeHpRecord *ref=attack_case()->before;
    for(unsigned m=0;m<attack_case()->party;++m){const NegativeHpRecord *r=&ref[m];
        w->hp[m]=r->hp;w->max_hp[m]=r->maximum;w->qi[m]=r->qi;w->max_qi[m]=999;
        w->other_stats[m][0]=r->mental;w->other_stats[m][1]=(uint16_t)(999-m);w->other_stats[m][2]=1;
        w->attack[m]=r->attack;w->defense[m]=w->battle_party_defenses[m]=r->defense;
        w->level[m]=(uint8_t)r->level;w->experience[m]=r->experience;
        w->experience_to_next[m]=postwin_thresholds[scenario][m];
        NpeCharacter character;assert(!npe_character(f,attack_case()->character-1,&character));
        memcpy(w->growth[m],character.growth,7);memcpy(w->skill_slots[m],character.raw+58,18);
        memcpy(w->battle_party_status[m],r->status,8);
    }
    for(unsigned m=0;m<3;++m){
        const NegativeHpRecord *r=&ref[m+attack_case()->party];w->battle_enemy_hps[m]=r->hp;w->battle_enemy_max_hps[m]=r->maximum;w->battle_enemy_qis[m]=r->qi;
        w->battle_enemy_spell_stats[m]=r->mental;w->battle_enemy_lucks[m]=1;w->battle_enemy_speeds[m]=(uint16_t)(m==attack_case()->enemy&&attack_case()->fast?1000:1);
        w->battle_enemy_attacks[m]=w->battle_enemy_base_attacks[m]=r->attack;
        w->battle_enemy_defenses[m]=w->battle_enemy_base_defenses[m]=r->defense;
        w->battle_enemy_damage_floor_stats[m]=(uint8_t)r->level;w->battle_enemy_experience_rewards[m]=r->experience;
        memcpy(w->battle_enemy_status[m],r->status,8);memset(w->battle_enemy_skill_ids[m],0,3);
    }

    memset(w->condition_words,0,sizeof(w->condition_words));w->condition_words[attack_case()->item]=1;
    if(attack_case()->group&&!attack_case()->own)w->battle_target=(uint8_t)attack_case()->target;
    w->battle_enemy_ids[0]=w->battle_enemy_ids[2]=0;w->battle_enemy_ids[1]=51;
    w->battle_enemy_gold_rewards[0]=w->battle_enemy_gold_rewards[2]=0;
    memcpy(w->battle_enemy_drop_ids[1],postwin_drops[scenario],2);
    memset(w->battle_enemy_drop_ids[0],0,2);memset(w->battle_enemy_drop_ids[2],0,2);
    calls=0;attack_seed(w,0);attack_check(w,attack_case()->before,0);return 0;
}
static int mixed_action(NpeWorld *w,unsigned target,unsigned defending){
    assert((attack_case()->actions[calls]==5)==defending);
    attack_check(w,attack_case()->before,1);
    unsigned last=calls+1==attack_case()->party_actions;
    if(last){animation_frames=0;animation_capture=w->battle_animation_observer!=NULL;}
    unsigned check_animation=last&&animation_capture;
    int result=defending?npe_world_battle_defend(w):npe_world_battle_throw_item(w->battle_file,w,attack_case()->item,target);assert(result==(last?NPE_BATTLE_WON:NPE_BATTLE_CONTINUE));
    if(check_animation){assert(animation_frames==sequence_frames()-1&&animation_capture);++animation_sequences;}
    attack_check(w,last?attack_case()->after:attack_case()->before,2);
    if(last){assert(w->battle_last_order_count==1);}
    /* The native sampler's battle inventory address is not used as a
       map-mode inventory assertion after the record copy on exit. */
    if(!last)assert(!w->condition_words[attack_case()->item]);
    ++calls;return result;
}
static int attack_action(NpeWorld *w,unsigned target){return mixed_action(w,target,0);}
static int defend_action(NpeWorld *w){return mixed_action(w,w->battle_actor,1);}
static int item_action(const NpeFile *f,NpeWorld *w,unsigned skill,unsigned target){(void)f;assert(skill==attack_case()->item&&target==attack_case()->target);return mixed_action(w,target,0);}
static void attack_frame(void){
    char path[160];snprintf(path,sizeof(path),"tmp/native-%s-%u.idx",attack_case()->name,2*attack_case()->party_actions);
    uint8_t ref[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *in=fopen(path,"rb");assert(in);
    assert(fread(ref,1,sizeof(ref),in)==sizeof(ref)&&fgetc(in)==EOF&&!fclose(in));
    assert(!SDL_RenderReadPixels(attack_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<sizeof(ref);++i){unsigned v=ref[i];differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL original postwin context %s final: %u differences\n",attack_case()->name,differences);
    if(differences){FILE *out=fopen("tmp/attack-c.rgba","wb");assert(out);fwrite(rgba,1,sizeof(rgba),out);fclose(out);}
    assert(!differences);frame_checked=calls;
}
static int attack_keys(void *ignored){
    (void)ignored;unsigned waited=0;while(!SDL_AtomicGet(&attack_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&attack_ready)){SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);return -1;}
    SDL_Keycode keys[24];unsigned key_count=0;
    keys[key_count++]=SDLK_RETURN;keys[key_count++]=SDLK_LEFT;keys[key_count++]=SDLK_DOWN;
    keys[key_count++]=SDLK_RETURN;keys[key_count++]=SDLK_DOWN;
    keys[key_count++]=SDLK_RETURN;keys[key_count++]=SDLK_DOWN;keys[key_count++]=SDLK_RETURN;
    keys[key_count++]=SDLK_RETURN;
    for(unsigned actor=1;actor<attack_case()->party_actions;++actor){

        keys[key_count++]=SDLK_RETURN;
        if(actor==1)keys[key_count++]=SDLK_DOWN;
        keys[key_count++]=SDLK_RETURN;
    }
    for(unsigned i=0;i<key_count;++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];if(SDL_PushEvent(&e)!=1)return -1;SDL_Delay(60);}
    unsigned final_wait=0;while(SDL_AtomicGet(&attack_round_ready)<(int)attack_case()->party_actions&&final_wait<5000){SDL_Delay(10);final_wait+=10;}
    if(SDL_AtomicGet(&attack_round_ready)<(int)attack_case()->party_actions){SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);return -1;}
    SDL_Delay(100);SDL_Event event={0};event.type=SDL_QUIT;return SDL_PushEvent(&event)==1?0:-1;
}
int main(int argc,char **argv){
    assert(argc==2||(argc==3&&!strcmp(argv[2],"--all-frames")));
    NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(scenario=0;scenario<sizeof(attack_animation_cases)/sizeof(*attack_animation_cases);++scenario){
        NpeWorld w;assert(!attack_new_game(&f,&w));
        attack_animation_c(&f,&w);for(unsigned actor=0;actor<attack_case()->party_actions;++actor)assert((attack_case()->actions[actor]==5?defend_action(&w):item_action(&f,&w,attack_case()->item,attack_case()->target))==(actor+1==attack_case()->party_actions?NPE_BATTLE_WON:NPE_BATTLE_CONTINUE));
        calls=frame_checked=0;SDL_AtomicSet(&attack_ready,0);SDL_AtomicSet(&attack_round_ready,0);SDL_Thread *thread=SDL_CreateThread(attack_keys,"original attack keys",NULL);assert(thread);
        char *args[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,args));int result=-1;SDL_WaitThread(thread,&result);assert(!result&&calls==attack_case()->party_actions&&frame_checked==calls);
        fprintf(stderr,"C/SDL original postwin context %s passed\n",attack_case()->name);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));npe_free(&f);
    assert(animation_sequences==sizeof(attack_animation_cases)/sizeof(*attack_animation_cases));
    printf("%u queued commands, %u original killing actions, %u full record phases, %u final SDL frames and %u C/SDL animation commits passed\n",
        POSTWIN_ANIMATION_QUEUED_COMMANDS,ATTACK_ACTIONS,ATTACK_RECORD_PHASES,
        (unsigned)(sizeof(attack_animation_cases)/sizeof(*attack_animation_cases)),ATTACK_COMMITS);return 0;
}
