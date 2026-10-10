#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned id,x,y,direction;} ScriptNpc;
typedef struct {const char *name;unsigned x,y,target_x,target_y,commits;uint32_t seed;
    unsigned mode,event,progress;uint32_t rng;ScriptNpc npc[20];
#ifdef NPE_MOVE_EVENT_RAM
    unsigned input_event;uint8_t before_cache[90],after_cache[90];
#endif
} NpcScriptCase;
#ifdef NPE_MOVE_EVENT_RAM
#include "npc_move_resident_native.h"
#define NPC_REFERENCE "tmp/native-npc-move-resident-%u-%u.idx"
#define NPC_INPUT(c) ((c)->input_event)
#else
#include "npc_script_move_native.h"
#define NPC_REFERENCE "tmp/native-npc-script-move-%u-%u.idx"
#define NPC_INPUT(c) 1
#endif
static NpeWorld *live;
static unsigned case_index,commits,total;
static Uint32 fake_clock;
static int active;
static int cancel_after_step(void *opaque,const uint8_t *pixels,unsigned wait){
    (void)pixels;(void)wait;unsigned *count=opaque;return ++*count==3;
}
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static void present(SDL_Renderer *);
static int wait_event(SDL_Event *,int);
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 ms){fake_clock+=ms;}
static int no_input(SDL_Event *e){(void)e;return 0;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 f){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,f);}
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent no_input
#define SDL_WaitEventTimeout wait_event
#define SDL_RenderPresent present
#define SDL_CreateWindow probe_window
#define npe_sound_select select_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef npe_sound_select
#ifdef NPE_MOVE_EVENT_RAM
#include "move_event_ram_helpers.h"
#endif
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){
        const NpcScriptCase *c=&npc_script_cases[case_index];live=w;
        NpeFile *file=(NpeFile *)w->battle_file;
#ifdef NPE_MOVE_EVENT_RAM
        move_file_fingerprint=npe_game_fingerprint(file);
        inject_move_ram(file,c->before_cache);
#else
        NpeMap map;NpeMapNpcMoveEvent event;
        assert(!npe_map(file,0,&map)&&npe_map_npc_move_event(&map,1,&event)==1);
        uint8_t *raw=(uint8_t *)event.raw;
        raw[2]=0;raw[3]=1;raw[5]=(uint8_t)c->target_x;raw[6]=(uint8_t)c->target_y;
        raw[7]=1;raw[8]=raw[9]=0;
        assert(!npe_world_load_npc_move_events(w,&map));
        for(unsigned i=0;i<20;++i){const uint8_t *npc;assert(!npe_map_npc(&map,i,&npc));((uint8_t *)npc)[7]=0;}
#endif
        w->npc_override_x[0]=(uint8_t)c->x;w->npc_override_y[0]=(uint8_t)c->y;
        w->direction=1;w->music_enabled=0;w->random_state=c->seed;
        NpeWorld numeric=*w;numeric.game_mode=18;numeric.story_event_id=(uint16_t)NPC_INPUT(c);
        assert(npe_world_apply_npc_move_event(file,&numeric)==1);
        assert(numeric.npc_override_x[0]==c->npc[0].x&&numeric.npc_override_y[0]==c->npc[0].y&&
            numeric.npc_override_direction[0]==c->npc[0].direction&&numeric.random_state==c->seed);
        NpeWorld zero=*w;zero.game_mode=18;zero.story_event_id=(uint16_t)NPC_INPUT(c);
#ifdef NPE_MOVE_EVENT_RAM
        uint8_t xy[]={(uint8_t)(c->x>>8),(uint8_t)c->x,(uint8_t)(c->y>>8),(uint8_t)c->y};
        assert(!npe_world_resident_write(&zero,0x527658+((NPC_INPUT(c)&255)-1)*10,xy,4));
#else
        raw[5]=(uint8_t)c->x;raw[6]=(uint8_t)c->y;
        assert(!npe_world_load_npc_move_events(&zero,&map));
#endif
        NpeWorld unchanged=zero;
        assert(npe_world_apply_npc_move_event(file,&zero)==-1&&!memcmp(&zero,&unchanged,sizeof(zero)));
#ifndef NPE_MOVE_EVENT_RAM
        raw[5]=(uint8_t)c->target_x;raw[6]=(uint8_t)c->target_y;
#endif
        NpeWorld cancelled=*w;cancelled.game_mode=18;cancelled.story_event_id=(uint16_t)NPC_INPUT(c);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];unsigned cancel_commits=0;
        assert(npe_world_npc_move_with_display(file,&cancelled,pixels,cancel_after_step,&cancel_commits)==-2);
        assert(cancelled.game_mode==18&&cancelled.story_event_id==NPC_INPUT(c)&&cancelled.story_progress==w->story_progress);
    }
    npe_sound_select(p,w);SDL_PauseAudioDevice(p->device,1);
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const NpcScriptCase *c=&npc_script_cases[case_index];assert(commits<c->commits);
    char path[160];snprintf(path,sizeof(path),NPC_REFERENCE,case_index,commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"NPC script %s commit%u C=%u SDL=%u time%u\n",c->name,commits,d,sd,fake_clock);
    if(d){FILE *out=fopen("tmp/npc-script-move-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;memset(e,0,sizeof(*e));
    if(!active){live->game_mode=18;live->story_event_id=(uint16_t)NPC_INPUT(&npc_script_cases[case_index]);active=1;
        e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F12;return 1;}
    const NpcScriptCase *c=&npc_script_cases[case_index];assert(commits==c->commits);
    assert(live->game_mode==c->mode&&live->story_event_id==c->event&&live->story_progress==c->progress&&live->random_state==c->rng);
    const NpeFile *file=(const NpeFile *)live->battle_file;NpeMap map;assert(!npe_map(file,0,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *npc;unsigned x,y;assert(!npe_map_npc(&map,i,&npc));
        npe_world_npc_position(live,npc,i,&x,&y);unsigned dir=live->npc_override_direction[i]<=3?live->npc_override_direction[i]:npc[6];
        assert(x==c->npc[i].x&&y==c->npc[i].y&&dir==c->npc[i].direction);}
    unsigned dx=c->x>c->target_x?c->x-c->target_x:c->target_x-c->x;
    unsigned dy=c->y>c->target_y?c->y-c->target_y:c->target_y-c->y;
    assert(fake_clock==(dx>dy?dx:dy)*141u);
#ifdef NPE_MOVE_EVENT_RAM
    check_move_ram(file,c->after_cache);
#endif
    active=0;e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<sizeof(npc_script_cases)/sizeof(*npc_script_cases);++case_index){
        live=NULL;commits=fake_clock=active=0;
        char *args[]={"npe-sdl","--walk-preview","1,7,9,0,5",argv[1],NULL};assert(!run(4,args));
    }
    printf("Original NPC script: %u actual main/SDL commits, %u paths, final NPC state, C RNG preservation and nominal waits passed\n",total,
        (unsigned)(sizeof(npc_script_cases)/sizeof(*npc_script_cases)));
    return 0;
}
