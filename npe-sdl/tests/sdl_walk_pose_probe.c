#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#undef SDL_PollEvent
#undef SDL_GetTicks
#undef SDL_WaitEventTimeout
#undef SDL_CreateWindow
#undef npe_sound_select
#undef SDL_RenderPresent
#endif
typedef struct { unsigned id,graphic,first,last,direction,x,y,moving; } WalkNpc;
typedef struct { unsigned enemy,npc,graphic,first,last,x,y,left,right; } WalkMonster;
typedef struct { unsigned graphic,first,last,enemy,x,y,cooldown; } WalkChase;
typedef struct {
    const char *name,*reference;unsigned mask,commits,map,progress,sx,sy,vx,vy,direction;
    unsigned after_map,after_progress,after_mode,after_sx,after_sy,after_vx,after_vy;
    WalkNpc npc[20];
#ifdef NPE_WALK_ACTOR_MATRIX
    WalkMonster monsters[4];
#endif
#ifdef NPE_WALK_CONTINUATION_MATRIX
    unsigned encounters,steps_before,steps_after;uint32_t seed,rng_after;
    WalkChase chase_before[2],chase_after[2];
#endif
} WalkCase;
#ifdef NPE_WALK_CONTINUATION_MATRIX
#include "walk_continuation_native.h"
static void word(uint8_t *,unsigned);
#include "chase_fixture_map.h"
#define WALK_FIRST_MAIN_CASE 0
#elif defined(NPE_WALK_ACTOR_MATRIX)
#include "walk_actor_native.h"
#define WALK_FIRST_MAIN_CASE 0
#elif defined(NPE_WALK_VIEWPORT_MATRIX)
#include "walk_viewport_native.h"
#define WALK_FIRST_MAIN_CASE 0
#elif defined(NPE_WALK_EXIT_MATRIX)
#include "walk_exit_native.h"
#define WALK_FIRST_MAIN_CASE 0
#else
#include "walk_native.h"
#define WALK_FIRST_MAIN_CASE 1
#endif
static unsigned main_case,main_stage,main_commits,main_checked;
static unsigned ignored_index,ignored_checked;
static NpeWorld ignored_before;
static int main_active;
static Uint32 fake_clock;
static NpeWorld *live;
static int guarded_save(const NpeFile *,const NpeWorld *,const char *);
static int guarded_restore(const NpeFile *,NpeWorld *,const char *);
#ifdef NPE_WALK_EXIT_MATRIX
static unsigned notice_keys,quit_notice;
#endif
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static int wait_outer(SDL_Event *,int);
static void present_probe(SDL_Renderer *);
static Uint32 ticks(void) { return fake_clock; }
static SDL_Window *create_probe_window(const char *s,int x,int y,int w,int h,Uint32 flags) {
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
#define SDL_GetTicks ticks
#define npe_world_save guarded_save
#define npe_world_restore guarded_restore
/* Observe the logical wait, not the audio bridge's bounded10ms slice. */
#define SDL_WaitEventTimeout(event,slice) wait_outer(event,timeout)
#define SDL_CreateWindow create_probe_window
#define npe_sound_select select_sound
#define SDL_RenderPresent present_probe
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef npe_world_save
#undef npe_world_restore
#undef main
#undef SDL_PollEvent
#undef SDL_GetTicks
#undef SDL_WaitEventTimeout
#undef SDL_CreateWindow
#undef npe_sound_select
#undef SDL_RenderPresent
#include "battle_input_keys.h"
static int guarded_save(const NpeFile *file,const NpeWorld *world,const char *path){
    if(getenv("NPE_PROBE_HOST_SAVE_KEYS")){
        fprintf(stderr,"Non-original F5 reached the save service\n");assert(0);
    }
    return npe_world_save(file,world,path);
}
static int guarded_restore(const NpeFile *file,NpeWorld *world,const char *path){
    if(getenv("NPE_PROBE_HOST_SAVE_KEYS")){
        fprintf(stderr,"Non-original F9 reached the restore service\n");assert(0);
    }
    return npe_world_restore(file,world,path);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w) { live=w;npe_sound_select(p,w); }
static void word(uint8_t *p,unsigned v) { p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v; }
static void frame(unsigned case_id,int commit,uint8_t *pixels) {
    char name[128];
    if(commit<0)snprintf(name,sizeof(name),"%s-before.idx",walk_cases[case_id].reference);
    else snprintf(name,sizeof(name),"%s-commit-%u.idx",walk_cases[case_id].reference,(unsigned)commit);
    FILE *f=fopen(name,"rb");assert(f);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT);
    assert(fgetc(f)==EOF&&!fclose(f));
}
static void configure(NpeFile *file,NpeWorld *world,const WalkCase *c) {
    NpeMap map;
        assert(!npe_map(file,c->map-1,&map));
#ifndef NPE_MONSTER_RESIDENT_RAM
        for(unsigned j=0;j<20;++j){const uint8_t *raw;assert(!npe_map_npc(&map,j,&raw));
            uint8_t *p=file->bytes+(raw-file->bytes);const WalkNpc *n=&c->npc[j];
            word(p,n->id);p[2]=(uint8_t)n->x;p[3]=(uint8_t)n->y;word(p+4,n->graphic);
            p[6]=(uint8_t)n->direction;p[7]=(uint8_t)n->moving;word(p+8,n->first);word(p+10,n->last);
        }
#ifdef NPE_WALK_ACTOR_MATRIX
        for(unsigned j=0;j<4;++j){NpeMapMonster m;assert(!npe_map_monster(&map,j,&m));
            uint8_t *p=file->bytes+(m.raw-file->bytes);const WalkMonster *n=&c->monsters[j];
            p[0]=(uint8_t)n->enemy;p[1]=(uint8_t)n->x;p[2]=(uint8_t)n->y;
            word(p+3,n->first);word(p+5,n->last);word(p+7,n->graphic);word(p+9,n->npc);
            p[11]=(uint8_t)n->left;p[12]=(uint8_t)n->right;
        }
#endif
#ifdef NPE_WALK_CONTINUATION_MATRIX
        memset(file->bytes+(map.tail-file->bytes)+644,0,8);
        file->bytes[(map.tail-file->bytes)+644]=(uint8_t)(c->encounters?89:0);
        for(unsigned j=0;j<4;++j){uint8_t *p=file->bytes+(map.tail-file->bytes)+592+j*13;p[0]=0;word(p+9,0);}
        for(unsigned j=0;j<2;++j){NpeMapChase row;assert(npe_map_chase(&map,j,&row)==1);
            uint8_t *p=file->bytes+(row.raw-file->bytes);const WalkChase *n=&c->chase_before[j];
            word(p,n->graphic);word(p+2,n->first);word(p+4,n->last);word(p+6,n->enemy);
            p[8]=(uint8_t)n->x;p[9]=(uint8_t)n->y;}
#endif
#endif
        assert(!npe_world_new_game(file,world));
#ifdef NPE_MONSTER_RESIDENT_RAM
        uint64_t fingerprint=npe_game_fingerprint(file);uint8_t npc_bytes[280],monster_bytes[56];
        assert(!npe_world_resident_read(world,0x526ca2,npc_bytes,280));
        for(unsigned j=0;j<20;++j){uint8_t *r=npc_bytes+14*j;const WalkNpc *n=&c->npc[j];
            word(r,n->id);word(r+2,n->graphic);word(r+4,n->first);word(r+6,n->last);
            r[8]=(uint8_t)n->direction;r[9]=(uint8_t)n->x;r[10]=(uint8_t)n->y;r[11]=(uint8_t)n->moving;
        }
        for(unsigned j=0;j<4;++j){uint8_t *r=monster_bytes+14*j;const WalkMonster *n=&c->monsters[j];
            word(r,n->graphic);word(r+2,n->first);word(r+4,n->last);word(r+6,n->npc);
            r[8]=0;r[9]=(uint8_t)n->x;r[10]=(uint8_t)n->y;r[11]=(uint8_t)n->enemy;
            r[12]=(uint8_t)n->left;r[13]=(uint8_t)n->right;
        }
        int attached=npe_rpg_runtime_owner(file)==world;
        assert(!(attached?npe_rpg_runtime_write(file,0x526ca2,npc_bytes,280):
                          npe_world_resident_write(world,0x526ca2,npc_bytes,280)));
        assert(!(attached?npe_rpg_runtime_write(file,0x52761a,monster_bytes,56):
                          npe_world_resident_write(world,0x52761a,monster_bytes,56)));
        assert(npe_game_fingerprint(file)==fingerprint);
#endif
        world->map_number=c->map;world->story_progress=c->progress;
        world->view.offset_x=c->vx;world->view.offset_y=c->vy;world->view.screen_x=c->sx;world->view.screen_y=c->sy;
        world->x=c->sx+c->vx;world->y=c->sy+c->vy;world->music_enabled=0;
#ifdef NPE_WALK_CONTINUATION_MATRIX
        world->encounter_steps=(uint16_t)c->steps_before;npe_world_seed(world,c->seed);
        for(unsigned j=0;j<2;++j){const WalkChase *n=&c->chase_before[j];
            world->chase_x[j]=(uint16_t)n->x;world->chase_y[j]=(uint16_t)n->y;
            world->chase_cooldown[j]=(uint8_t)n->cooldown;}
#endif
}
static void compare_state(const NpeWorld *world,const WalkCase *c) {
    assert(world->map_number==c->after_map && world->story_progress==c->after_progress &&
           world->game_mode==c->after_mode && world->direction==c->direction &&
           world->view.screen_x==c->after_sx && world->view.screen_y==c->after_sy &&
           world->view.offset_x==c->after_vx && world->view.offset_y==c->after_vy);
#ifdef NPE_WALK_VIEWPORT_MATRIX
    assert(world->x==c->after_vx+c->after_sx && world->y==c->after_vy+c->after_sy);
#endif
#ifdef NPE_WALK_CONTINUATION_MATRIX
    fprintf(stderr,"%s continuation: counter=%u RNG=%u chase=%u,%u / %u,%u\n",c->name,
        world->encounter_steps,world->random_state,world->chase_x[0],world->chase_y[0],world->chase_x[1],world->chase_y[1]);
    assert(world->encounter_steps==c->steps_after && world->random_state==c->rng_after);
    for(unsigned j=0;j<2;++j){const WalkChase *n=&c->chase_after[j];
        assert(world->chase_x[j]==n->x&&world->chase_y[j]==n->y&&world->chase_cooldown[j]==n->cooldown);}
#endif
}
typedef struct {
    SDL_Renderer *renderer;SDL_Texture *texture;
    unsigned index,step,checked;
} WalkComparison;
static int compare_commit(void *opaque,const uint8_t *pixels,unsigned wait_parameter) {
    WalkComparison *c=opaque;uint8_t native[NPE_WIDTH*NPE_HEIGHT];
    (void)wait_parameter;
    assert(c->step<walk_cases[c->index].commits);frame(c->index,(int)c->step,native);
    unsigned diff=0;for(unsigned k=0;k<sizeof(native);++k)diff+=pixels[k]!=native[k];
#ifdef NPE_WALK_CONTINUATION_MATRIX
    if(diff){FILE *f=fopen("tmp/walk-continuation-mismatch.idx","wb");assert(f);
        assert(fwrite(pixels,1,sizeof(native),f)==sizeof(native)&&!fclose(f));}
#endif
    fprintf(stderr,"%s commit%u C differences=%u\n",walk_cases[c->index].name,c->step,diff);assert(!diff);
    draw_pixels(c->renderer,c->texture,pixels);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(c->renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));diff=0;
    for(unsigned k=0;k<sizeof(native);++k){unsigned v=native[k];
        diff+=rgba[k]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s commit%u SDL differences=%u\n",walk_cases[c->index].name,c->step,diff);assert(!diff);
    ++c->step;++c->checked;return 0;
}
static void present_probe(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);
    if(!main_active)return;
    const WalkCase *c=&walk_cases[main_case];uint8_t native[NPE_WIDTH*NPE_HEIGHT];
#ifdef NPE_WALK_CONTINUATION_MATRIX
    assert(main_commits<c->commits); /* Native chase does not flush LCD here. */
#endif
    unsigned commit=main_commits<c->commits ? main_commits : c->commits-1;
    frame(main_case,(int)commit,native);
    unsigned diff=0;for(unsigned k=0;k<sizeof(native);++k)diff+=last_pixel_frame[k]!=native[k];
    uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned sdl_diff=0;for(unsigned k=0;k<sizeof(native);++k){unsigned v=native[k];
        sdl_diff+=rgba[k]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"main %s commit%u C=%u SDL=%u\n",c->name,main_commits,diff,sdl_diff);
    assert(!diff&&!sdl_diff);if(main_commits<c->commits)++main_commits;
}
static int wait_outer(SDL_Event *event,int timeout) {
    memset(event,0,sizeof(*event));
    if(!main_stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;++main_stage;return 1;}
    if(main_stage==1){
        assert(live->game_mode==1);NpeWorld saved=*live;NpeFile *file=(NpeFile *)live->battle_file;
#ifdef NPE_WALK_CONTINUATION_MATRIX
        add_chase_directory(file);
#endif
        configure(file,live,&walk_cases[main_case]);
        live->sound_observer=saved.sound_observer;live->sound_context=saved.sound_context;
        live->sound_range_observer=saved.sound_range_observer;
        live->battle_animation_observer=saved.battle_animation_observer;
        live->battle_animation_context=saved.battle_animation_context;
        live->music_enabled=saved.music_enabled;live->map_music_slot=saved.map_music_slot;
        live->sound_section=saved.sound_section;live->sound_loop=saved.sound_loop;
        /* The original diagnostic sampler injects the viewport and its
           105 cache words together while retaining the prior LCD. */
        uint16_t cached[105];
        assert(npe_rpg_runtime_map_cache(file,live->map_number,live->view.offset_x,
                                         live->view.offset_y,cached)==1);
        frame(main_case,-1,world_map_frame);world_map_frame_state=*live;
        world_map_frame_file=file;world_map_frame_renderer=last_pixel_renderer;world_map_frame_valid=1;
        fake_clock=1234;main_active=0;
        if(getenv("NPE_PROBE_IGNORED_SHORTCUTS")||getenv("NPE_PROBE_HOST_SAVE_KEYS")){
            /* Supply the diagnostic's retained LCD as well as its map
               cache before measuring ignored input. */
            SDL_Texture *retained=SDL_CreateTexture(last_pixel_renderer,SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(retained);
            draw_pixels(last_pixel_renderer,retained,world_map_frame);SDL_DestroyTexture(retained);
            retain_world_frame(file,live,last_pixel_renderer,last_pixel_frame,0);
            ignored_before=*live;ignored_index=1;main_stage=4;
            event->type=SDL_KEYDOWN;event->key.keysym.sym=getenv("NPE_PROBE_HOST_SAVE_KEYS")?SDLK_F5:SDLK_q;
        }else{
            main_stage=2;
            probe_battle_input(event,(unsigned[]){2,8,1,4}[walk_cases[main_case].direction],SDLK_RETURN);
        }
        main_active=1;return 1;
    }
    if(main_stage==4){
        assert(!main_commits && fake_clock==1234 && live->game_mode==1);
        assert(live->x==ignored_before.x && live->y==ignored_before.y &&
               live->direction==ignored_before.direction && live->exit_choice==ignored_before.exit_choice &&
               live->story_event_id==ignored_before.story_event_id && live->story_progress==ignored_before.story_progress &&
               live->random_state==ignored_before.random_state && live->encounter_steps==ignored_before.encounter_steps);
        assert(!memcmp(live->condition_words,ignored_before.condition_words,sizeof(live->condition_words)));
        ++ignored_checked;
        unsigned count=getenv("NPE_PROBE_HOST_SAVE_KEYS")?2u:3u;
        if(ignored_index<count){event->type=SDL_KEYDOWN;
            event->key.keysym.sym=getenv("NPE_PROBE_HOST_SAVE_KEYS")?
                (SDL_Keycode[]){SDLK_F5,SDLK_F9}[ignored_index++]:
                (SDL_Keycode[]){SDLK_q,SDLK_u,SDLK_e}[ignored_index++];return 1;}
        main_stage=2;
        probe_battle_input(event,(unsigned[]){2,8,1,4}[walk_cases[main_case].direction],SDLK_RETURN);return 1;
    }
    assert(main_stage==2);
#ifdef NPE_WALK_EXIT_MATRIX
    if(main_commits==6){
        assert(timeout==(notice_keys ? 1 : 5000));
        if(quit_notice){main_active=0;++main_stage;event->type=SDL_QUIT;return 1;}
        event->type=SDL_KEYDOWN;
        if(!notice_keys){fake_clock+=4999;event->key.keysym.sym=SDLK_RETURN;++notice_keys;return 1;}
        if(notice_keys==1){event->key.keysym.sym=SDLK_F4;++notice_keys;return 1;}
        assert(notice_keys==2);fake_clock+=1;return 0;
    }
#endif
    if(timeout>0 && timeout<=16){fake_clock+=(Uint32)timeout;return 0;}
    assert(main_commits==walk_cases[main_case].commits);
    compare_state(live,&walk_cases[main_case]);
#ifdef NPE_WALK_CONTINUATION_MATRIX
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];frame(main_case,(int)main_commits-1,native);
    assert(!memcmp(last_pixel_frame,native,sizeof(native)));
#endif
#ifdef NPE_WALK_EXIT_MATRIX
    assert(notice_keys==2 && fake_clock==1234+64+5000 && timeout==4000);
#else
    assert(fake_clock==1234+(walk_cases[main_case].commits==1?0:64));
    assert(timeout==4000-(int)(fake_clock-1234));
#endif
    main_active=0;++main_stage;event->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv) {
    assert(argc==2);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
#ifdef NPE_WALK_CONTINUATION_MATRIX
    add_chase_directory(&file);
#endif
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("walk poses",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    unsigned checked=0,pending=0;
#ifdef NPE_WALK_EXIT_MATRIX
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
#endif
    for(unsigned i=0;i<sizeof(walk_cases)/sizeof(*walk_cases);++i){
        const WalkCase *c=&walk_cases[i];NpeWorld world,after;
        configure(&file,&world,c);
        assert(!npe_world_resources_open(&file,&world));
        after=world;int moved=npe_world_step(&file,&after,c->direction);assert(moved>=0);
        if(moved!=NPE_MOVE_OK && moved!=NPE_MOVE_BLOCKED
#ifdef NPE_WALK_EXIT_MATRIX
           && moved!=NPE_MOVE_EXIT
#endif
           ){
            pending+=c->commits;fprintf(stderr,"%s: exit/event sequence pending (%u commits)\n",c->name,c->commits);
            npe_rpg_runtime_close(&file);continue;
        }
        compare_state(&after,c);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];frame(i,-1,pixels);
        WalkComparison comparison={renderer,texture,i,0,0};
        int displayed=npe_world_step_with_display(&file,&world,c->direction,pixels,compare_commit,&comparison);
        assert(displayed==moved && !memcmp(&world,&after,sizeof(world)));
#ifdef NPE_WALK_EXIT_MATRIX
        assert(moved==NPE_MOVE_EXIT && comparison.step==5);
        uint8_t base[sizeof(pixels)];memcpy(base,pixels,sizeof(base));
        assert(!npe_render_map_name_notice(&file,&font,&world,pixels));
        assert(!compare_commit(&comparison,pixels,0));
        assert(!compare_commit(&comparison,base,0));
#endif
        assert(comparison.step==c->commits);checked+=comparison.checked;
        npe_rpg_runtime_close(&file);
#ifdef NPE_WALK_VIEWPORT_MATRIX
        NpeWorld restored;
        assert(!npe_world_save(&file,&world,"tmp/walk-viewport-state.sav") &&
               !npe_world_load(&file,&restored,"tmp/walk-viewport-state.sav"));
        compare_state(&restored,c);
#endif

    }
    fprintf(stderr,"Walking map commits checked=%u; exit/event commits pending=%u\n",checked,pending);
#ifdef NPE_WALK_EXIT_MATRIX
    npe_font_free(&font);
#endif
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    for(main_case=WALK_FIRST_MAIN_CASE;main_case<sizeof(walk_cases)/sizeof(*walk_cases);++main_case){
        fake_clock=main_stage=main_commits=0;main_active=0;
        char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&main_stage==3);
        assert(main_commits==walk_cases[main_case].commits);main_checked+=main_commits;
    }
    fprintf(stderr,"Actual main key dispatch: %u attempts / %u native commits zero differences\n",
            (unsigned)(sizeof(walk_cases)/sizeof(*walk_cases))-WALK_FIRST_MAIN_CASE,main_checked);
    if(getenv("NPE_PROBE_IGNORED_SHORTCUTS"))fprintf(stderr,"Map Q/U/E ignored: %u actual inputs without LCD/state/timer changes\n",ignored_checked);
    if(getenv("NPE_PROBE_HOST_SAVE_KEYS"))fprintf(stderr,"Map F5/F9 ignored: %u actual inputs without save/restore calls or LCD/state/timer changes\n",ignored_checked);
#ifdef NPE_WALK_EXIT_MATRIX
    main_case=0;fake_clock=main_stage=main_commits=notice_keys=0;main_active=0;quit_notice=1;
    char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&main_stage==3&&main_commits==6);
    fprintf(stderr,"Map-name notice: ordinary keys ignored through5000ms; quit aborts before restore\n");
#endif
    return 0;
}
