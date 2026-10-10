#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned mode,event,map,progress,direction,sx,sy,vx,vy,choice,slot,steps;uint32_t rng;} ExitState;
typedef struct {unsigned id,graphic,first,last,direction,x,y,moving;} ExitNpc;
typedef struct {unsigned key,commits;ExitState after;} ExitPhase;
typedef struct {const char *name;unsigned commits;ExitState before;uint8_t raw[21];ExitNpc npc[20];unsigned phases;ExitPhase phase[32];
#ifdef NPE_EXIT_RESIDENT_RAM
    uint8_t before_bytes[288],after_bytes[288],npc_bytes[280],monster_bytes[56],chase_bytes[28];
#endif
} ExitCase;
#ifdef NPE_EXIT_DIALOGUE_MATRIX
#ifdef NPE_SHOP_CLOSE_MATRIX
#include "shop_close_native.h"
#else
#include "exit_dialogue_native.h"
#endif
static int dialogue_patch_applied;
#else
#ifdef NPE_EXIT_RESIDENT_RAM
#include "exit_resident_native.h"
#else
#include "exit_choice_native.h"
#endif
#endif
static unsigned case_index,phase_index,main_commits,main_stage;
static int active,direct_notice,idle_phase;
static Uint32 fake_clock;
static NpeWorld *live;
#ifdef NPE_EXIT_RESIDENT_RAM
static uint64_t resident_fingerprint;
#endif
static int wait_outer(SDL_Event *,int);
#ifdef NPE_EXIT_DIALOGUE_MATRIX
static int popup_wait(SDL_Event *);
#define SDL_WaitEvent popup_wait
#endif
static void present_probe(SDL_Renderer *);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static Uint32 ticks(void){return fake_clock;}
static SDL_Window *create_window(const char *s,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetTicks ticks
#define SDL_WaitEventTimeout wait_outer
#define SDL_CreateWindow create_window
#define SDL_RenderPresent present_probe
#define npe_sound_select select_sound
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_GetTicks
#undef SDL_WaitEventTimeout
#undef SDL_CreateWindow
#undef SDL_RenderPresent
#undef npe_sound_select
#undef SDL_WaitEvent
#include "battle_input_keys.h"
#ifndef NPE_EXIT_RESIDENT_RAM
static void word(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
#endif
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;if(main_stage)w->music_enabled=0;npe_sound_select(p,w);}
static void read_frame(unsigned commit,int before,uint8_t *pixels){char name[128];
#ifdef NPE_EXIT_DIALOGUE_MATRIX
#ifdef NPE_SHOP_CLOSE_MATRIX
    const char *prefix="native-shop-close";
#else
    const char *prefix="native-exit-dialogue";
#endif
#else
#ifdef NPE_EXIT_RESIDENT_RAM
    const char *prefix="native-exit-resident";
#else
    const char *prefix="native-exit-choice";
#endif
#endif
    if(before)snprintf(name,sizeof(name),"tmp/%s-%u-before.idx",prefix,case_index);
    else snprintf(name,sizeof(name),"tmp/%s-%u-commit-%u.idx",prefix,case_index,commit);
    FILE *f=fopen(name,"rb");assert(f&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT);
    assert(fgetc(f)==EOF&&!fclose(f));}
static void configure(NpeFile *file,NpeWorld *w){NpeMap map;const ExitCase *c=&exit_cases[case_index];
    assert(!npe_map(file,0,&map));
#ifdef NPE_EXIT_RESIDENT_RAM
    uint64_t fingerprint=npe_game_fingerprint(file);resident_fingerprint=fingerprint;
    assert(!npe_world_new_game(file,w));
    int attached=npe_rpg_runtime_owner(file)==w;
    assert(!(attached?npe_rpg_runtime_write(file,0x526b82,c->before_bytes,288):npe_world_resident_write(w,0x526b82,c->before_bytes,288)));
    assert(!(attached?npe_rpg_runtime_write(file,0x526ca2,c->npc_bytes,280):npe_world_resident_write(w,0x526ca2,c->npc_bytes,280)));
    assert(!(attached?npe_rpg_runtime_write(file,0x52761a,c->monster_bytes,56):npe_world_resident_write(w,0x52761a,c->monster_bytes,56)));
    assert(!(attached?npe_rpg_runtime_write(file,0x52783a,c->chase_bytes,28):npe_world_resident_write(w,0x52783a,c->chase_bytes,28)));
    assert(npe_game_fingerprint(file)==fingerprint);
#else
    for(unsigned i=0;i<20;++i){const uint8_t *raw;assert(!npe_map_npc(&map,i,&raw));
        uint8_t *p=file->bytes+(raw-file->bytes);const ExitNpc *n=&c->npc[i];
        word(p,n->id);p[2]=(uint8_t)n->x;p[3]=(uint8_t)n->y;word(p+4,n->graphic);
        p[6]=(uint8_t)n->direction;p[7]=(uint8_t)n->moving;word(p+8,n->first);word(p+10,n->last);}
    memset(file->bytes+(map.tail-file->bytes)+644,0,8);
    for(unsigned i=0;i<4;++i){uint8_t *p=file->bytes+(map.tail-file->bytes)+592+i*13;p[0]=0;word(p+9,0);}
    memcpy(file->bytes+(map.tail-file->bytes)+340+10*21,c->raw,21);
    assert(!npe_world_new_game(file,w));
#endif
    const ExitState *s=&c->before;
    w->music_enabled=0;w->story_event_id=(uint16_t)s->event;w->direction=(uint8_t)s->direction;
    w->view.offset_x=s->vx;w->view.offset_y=s->vy;w->view.screen_x=s->sx;w->view.screen_y=s->sy;
    w->x=(uint16_t)(s->vx+s->sx);w->y=(uint16_t)(s->vy+s->sy);w->encounter_steps=(uint16_t)s->steps;
    npe_world_seed(w,s->rng);
#ifdef NPE_SHOP_CLOSE_MATRIX
    /* The diagnostic viewport was moved after construction. Supply the
       original105-word cache as well; a cache from the new-game spawn
       paints the wrong background underneath the first walking pose. */
    assert(npe_rpg_runtime_owner(file)==w);uint8_t cache[210];
    for(unsigned i=0;i<105;++i)word(cache+2*i,shop_close_map_cache[case_index][i]);
    assert(!npe_rpg_runtime_write(file,0x5270b4,cache,sizeof(cache)));
#endif
}
static void resident_end(const NpeFile *file,const NpeWorld *w){
#ifdef NPE_EXIT_RESIDENT_RAM
    uint8_t actual[288];assert(!npe_world_resident_read(w,0x526b82,actual,288));
    assert(!memcmp(actual,exit_cases[case_index].after_bytes,288)&&!npe_rpg_runtime_failures(file));
    assert(npe_rpg_runtime_owner(file)==w&&npe_game_fingerprint(file)==resident_fingerprint);
#else
    (void)file;(void)w;
#endif
}
static void state(const NpeWorld *w,const ExitState *s){
    fprintf(stderr,"%s phase%u map=%u choice=%u event=%u steps=%u RNG=%u\n",exit_cases[case_index].name,
        phase_index,w->map_number,w->exit_choice,w->story_event_id,w->encounter_steps,w->random_state);
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->map_number==s->map&&
        w->story_progress==s->progress&&w->direction==s->direction&&w->view.screen_x==s->sx&&
        w->view.screen_y==s->sy&&w->view.offset_x==s->vx&&w->view.offset_y==s->vy&&
        w->exit_choice==s->choice&&w->exit_slot==s->slot&&w->encounter_steps==s->steps&&w->random_state==s->rng);
#ifdef NPE_EXIT_DIALOGUE_MATRIX
    assert(w->mode2_context==dialogue_context[case_index][phase_index][0]&&
           w->event_context_id==dialogue_context[case_index][phase_index][1]);
#endif
}
static void compare(SDL_Renderer *renderer,const uint8_t *pixels,unsigned commit,const char *route){
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(commit,0,native);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned diff=0,sdl_diff=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        diff+=pixels[i]!=v;sdl_diff+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s %s commit%u C=%u SDL=%u\n",route,exit_cases[case_index].name,commit,diff,sdl_diff);
    if(diff){FILE *f=fopen("tmp/exit-choice-actual.idx","wb");assert(f&&fwrite(pixels,1,sizeof(native),f)==sizeof(native)&&!fclose(f));}
    assert(!diff&&!sdl_diff);}
static unsigned direct_commits;
static void present_probe(SDL_Renderer *renderer){SDL_RenderPresent(renderer);if(!active)return;
    unsigned *count=active==1?&direct_commits:&main_commits;
    assert(*count<exit_cases[case_index].commits);compare(renderer,last_pixel_frame,(*count)++,active==1?"direct":"main");}
static unsigned phase_end(unsigned phase){unsigned count=0;for(unsigned i=0;i<=phase;++i)count+=exit_cases[case_index].phase[i].commits;return count;}
static SDL_Keycode keycode(unsigned mask){switch(mask){case 1:return SDLK_UP;case 2:return SDLK_DOWN;case 4:return SDLK_LEFT;
    case 8:return SDLK_RIGHT;case 16:return SDLK_SPACE;case 32:return SDLK_ESCAPE;case 64:return SDLK_F4;case 128:case 256:return SDLK_RETURN;default:assert(0);return 0;}}
static void native_event(SDL_Event *event,unsigned mask){
    if(mask==16||mask==32||mask==256){
        memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;
        event->key.keysym.sym=keycode(mask);
    }else probe_battle_input(event,mask==128?16:mask,SDLK_RETURN);
}
#ifdef NPE_EXIT_DIALOGUE_MATRIX
static void patch_talk_npc(void){
    NpeFile *file=(NpeFile *)live->battle_file;uint8_t p[14];
    const ExitNpc *n=&dialogue_patch[case_index];
    uint64_t fingerprint=npe_game_fingerprint(file);
    assert(!npe_world_resident_read(live,0x526ca2,p,14));
    word(p,n->id);word(p+2,n->graphic);word(p+4,n->first);word(p+6,n->last);
    p[8]=(uint8_t)n->direction;p[9]=(uint8_t)n->x;p[10]=(uint8_t)n->y;p[11]=(uint8_t)n->moving;
    assert(!npe_rpg_runtime_write(file,0x526ca2,p,14)&&npe_game_fingerprint(file)==fingerprint);
    unsigned id;assert(npe_world_facing_npc(file,live,&id)==1&&id==1);dialogue_patch_applied=1;
}
static int popup_wait(SDL_Event *event){
    assert(active==2&&main_commits==phase_end(phase_index));
    state(live,&exit_cases[case_index].phase[phase_index].after);
    assert(++phase_index<exit_cases[case_index].phases);
    unsigned key=exit_cases[case_index].phase[phase_index].key;assert(key==16||key==256);
    memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;event->key.keysym.sym=keycode(key);return 1;
}
#endif
static int wait_outer(SDL_Event *event,int timeout){memset(event,0,sizeof(*event));
    if(direct_notice){fake_clock+=(Uint32)timeout;return 0;}
    if(!main_stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;main_stage=1;return 1;}
    if(main_stage==1){NpeWorld saved=*live;NpeFile *file=(NpeFile *)live->battle_file;configure(file,live);
        live->sound_observer=saved.sound_observer;live->sound_context=saved.sound_context;
        live->sound_range_observer=saved.sound_range_observer;
        live->battle_animation_observer=saved.battle_animation_observer;live->battle_animation_context=saved.battle_animation_context;
#ifndef NPE_SHOP_CLOSE_MATRIX
        /* The original diagnostic moved the viewport after construction.
           Keep its105-word map cache aligned with that input as well. */
        uint16_t cached[105];
        assert(npe_rpg_runtime_map_cache(file,live->map_number,live->view.offset_x,
                                        live->view.offset_y,cached)==1);
#endif
        read_frame(0,1,world_map_frame);retain_world_frame(file,live,last_pixel_renderer,world_map_frame,0);
        main_stage=2;active=2;fake_clock=100;phase_index=0;native_event(event,1);return 1;}
    assert(main_stage==2);
    if(main_commits<phase_end(phase_index)){assert(timeout>0);fake_clock+=(Uint32)timeout;return 0;}
    state(live,&exit_cases[case_index].phase[phase_index].after);
    idle_phase=0;++phase_index;
    if(phase_index==exit_cases[case_index].phases){assert(main_commits==exit_cases[case_index].commits);
        resident_end((const NpeFile *)live->battle_file,live);
        active=0;main_stage=3;event->type=SDL_QUIT;return 1;}
    const ExitPhase *p=&exit_cases[case_index].phase[phase_index];
#ifdef NPE_EXIT_DIALOGUE_MATRIX
    if(p->key==16&&!dialogue_patch_applied)patch_talk_npc();
#endif
    if(!p->key){fake_clock+=5000;idle_phase=1;return 0;}
    native_event(event,p->key);return 1;
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("exit choice",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
#ifndef NPE_EXIT_DIALOGUE_MATRIX
    unsigned total=0;for(case_index=0;case_index<sizeof(exit_cases)/sizeof(*exit_cases);++case_index){
        NpeWorld world,numeric;configure(&file,&world);numeric=world;read_frame(0,1,world_map_frame);
        assert(!npe_world_resources_open(&file,&world));
        retain_world_frame(&file,&world,renderer,world_map_frame,0);direct_commits=0;active=1;fake_clock=100;
        world_map_saved_edges_valid=0;
        for(phase_index=0;phase_index<exit_cases[case_index].phases;++phase_index){const ExitPhase *p=&exit_cases[case_index].phase[phase_index];
            if(!phase_index){assert(npe_world_step(&file,&numeric,2)>=0);direct_notice=1;
                assert(game_world_step_display(&file,&world,2,renderer,texture,&font)>=0);direct_notice=0;}
            else if(p->key==64){if(world.game_mode==1){assert(npe_world_open_system_menu(&numeric)==1);
                    assert(npe_world_open_system_menu(&world)==1);}
                else {assert(npe_world_close_system_menu(&numeric)==1);assert(npe_world_close_system_menu(&world)==1);}}
            else if(p->key){if(p->key==1||p->key==2)assert(!npe_world_select_exit(&numeric,p->key));
                else if(p->key==128)assert(npe_world_confirm_exit(&file,&numeric)==NPE_MOVE_EXIT);
                direct_notice=1;assert(!game_exit_choice_input(&file,&world,keycode(p->key),renderer,texture,&font));direct_notice=0;}
            else {Uint32 deadline=fake_clock;fake_clock+=5000;assert(!game_world_timer_display(&file,&world,fake_clock,&deadline,0,renderer,texture));}
            assert(!memcmp(&world,&numeric,sizeof(world)));unsigned levels[3]={0};
            assert(!draw_world(renderer,texture,&file,&world,0,0,0,0,0,0,levels,0,&font));
            assert(direct_commits==phase_end(phase_index));state(&world,&p->after);}
        resident_end(&file,&world);assert(!npe_rpg_runtime_failures(&file));
        npe_rpg_runtime_close(&file);total+=direct_commits;active=0;}
    fprintf(stderr,"Direct exit commits=%u zero differences\n",total);
#endif
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_font_free(&font);npe_free(&file);
    unsigned actual=0;for(case_index=0;case_index<sizeof(exit_cases)/sizeof(*exit_cases);++case_index){
#ifdef NPE_EXIT_DIALOGUE_MATRIX
        dialogue_patch_applied=0;
#endif
        main_stage=main_commits=idle_phase=0;active=0;char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&main_stage==3);actual+=main_commits;
        fprintf(stderr,"Main case %s complete %u/%u\n",exit_cases[case_index].name,main_commits,exit_cases[case_index].commits);}
    fprintf(stderr,"Main exit commits=%u zero differences\n",actual);return 0;
}
