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
typedef struct {const char *name;unsigned input,x,y,target_x,target_y;int blocked_npc;
    unsigned commits,mode,event,progress,vx,vy,sx,sy,direction,background;ScriptNpc npc[20];
#ifdef NPE_MOVE_EVENT_RAM
    uint8_t before_cache[90],after_cache[90];
#endif
} ScriptCase;
#ifdef NPE_MOVE_EVENT_RAM
#include "script_walk_resident_native.h"
#define SCRIPT_REFERENCE "tmp/native-script-walk-resident-%u-%u.idx"
#else
#include "script_walk_native.h"
#define SCRIPT_REFERENCE "tmp/native-script-walk-%u-%u.idx"
#endif
static NpeWorld *live;
static unsigned case_index,commits,total,started,active;
static Uint32 fake_clock;
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
static void check_end(const ScriptCase *c,const NpeWorld *w){
    assert(w->game_mode==c->mode&&w->story_event_id==c->event&&w->story_progress==c->progress);
    assert(w->view.offset_x==c->vx&&w->view.offset_y==c->vy&&w->view.screen_x==c->sx&&w->view.screen_y==c->sy);
    assert(w->x==c->vx+c->sx&&w->y==c->vy+c->sy&&w->direction==c->direction&&w->random_state==11);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){const ScriptCase *c=&script_cases[case_index];live=w;
        const NpeFile *file=(const NpeFile *)w->battle_file;NpeMap map;assert(!npe_map(file,0,&map));
#ifdef NPE_MOVE_EVENT_RAM
        move_file_fingerprint=npe_game_fingerprint(file);
        inject_move_ram(file,c->before_cache);
#else
        unsigned slot=(c->input&255)==4?3:0;
        w->script_walk_events[slot]=(NpeScriptWalkCache){.marker=1,.x=(uint16_t)c->target_x,
            .y=(uint16_t)c->target_y,.next_mode=1,.next_id=0};
        for(unsigned i=0;i<20;++i){const uint8_t *npc;assert(!npe_map_npc(&map,i,&npc));((uint8_t *)npc)[7]=0;}
#endif
        if(c->blocked_npc>=0){w->npc_override_x[0]=44;w->npc_override_y[0]=2;}
        w->direction=1;w->random_state=11;w->music_enabled=0;w->sound_section=0;w->lcd_background=(uint8_t)c->background;
        NpeWorld numeric=*w;numeric.game_mode=19;numeric.story_event_id=(uint16_t)c->input;
        assert(npe_world_apply_script_walk_event(file,&numeric)==(c->input?1:0));check_end(c,&numeric);
    }
    npe_sound_select(p,w);SDL_PauseAudioDevice(p->device,1);
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const ScriptCase *c=&script_cases[case_index];assert(commits<c->commits);
    char path[160];snprintf(path,sizeof(path),SCRIPT_REFERENCE,case_index,commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Script walk %s commit%u C=%u SDL=%u clock%u\n",c->name,commits,d,sd,fake_clock);
    if(d){FILE *out=fopen("tmp/script-walk-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){memset(e,0,sizeof(*e));
    if(active&&timeout<=16&&commits<script_cases[case_index].commits){fake_clock+=(Uint32)timeout;return 0;}
    if(!started){started=active=1;live->game_mode=19;live->story_event_id=(uint16_t)script_cases[case_index].input;
        e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F12;return 1;}
    const ScriptCase *c=&script_cases[case_index];assert(commits==c->commits);check_end(c,live);assert(!live->map_init_pending);
    unsigned dx=c->target_x>c->x?c->target_x-c->x:c->x-c->target_x;
    int dy=(int)c->target_y-(int)c->y+1;unsigned ady=(unsigned)(dy<0?-dy:dy);
    assert(fake_clock==(c->input?(dx>ady?dx:ady)*205u:0));
    NpeMap map;assert(!npe_map((const NpeFile *)live->battle_file,0,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *npc;unsigned x,y;assert(!npe_map_npc(&map,i,&npc));
        npe_world_npc_position(live,npc,i,&x,&y);assert(x==c->npc[i].x&&y==c->npc[i].y);}
#ifdef NPE_MOVE_EVENT_RAM
    check_move_ram((const NpeFile *)live->battle_file,c->after_cache);
#endif
    active=0;e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<sizeof(script_cases)/sizeof(*script_cases);++case_index){
        live=NULL;commits=fake_clock=active=started=0;char preview[80];const ScriptCase *c=&script_cases[case_index];
        snprintf(preview,sizeof(preview),"1,%u,%u,36,0",c->x,c->y);
        char *args[]={"npe-sdl","--walk-preview",preview,argv[1],NULL};assert(!run(4,args));
    }
    printf("Original script walking: %u actual C/SDL commits, collision, viewport, numeric state, zero/invalid/retained slots and waits matched\n",total);
    return 0;
}
