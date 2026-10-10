#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#include "farmer_native.h"
static NpeWorld *live;
static unsigned phase,commits,total;
static int active;
static Uint32 fake_clock;
static Uint32 ticks(void){return fake_clock;}
static int wait_event(SDL_Event *,int);
static void present(SDL_Renderer *);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetTicks ticks
/* Route input belongs to the outer wait, not an audio10ms poll slice. */
#define SDL_WaitEventTimeout(event,slice) wait_event(event,timeout)
#define SDL_RenderPresent present
#define SDL_CreateWindow probe_window
#define npe_sound_select select_sound
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef npe_sound_select
#undef SDL_PollEvent
static void state(const unsigned *s){
    assert(live->game_mode==s[0]&&live->map_number==s[1]&&live->story_progress==s[2]&&
        live->view.screen_x==s[3]&&live->view.screen_y==s[4]&&live->view.offset_x==s[5]&&
        live->view.offset_y==s[6]&&live->direction==s[7]);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){w->music_enabled=0;w->direction=farmer_entry[7];active=1;}live=w;
    npe_sound_select(p,w);SDL_PauseAudioDevice(p->device,1);
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    char path[160];if(!phase){assert(!commits);snprintf(path,sizeof(path),"tmp/native-farmer-entry.idx");}
    else {assert(phase<=sizeof(farmer_steps)/sizeof(*farmer_steps)&&commits<farmer_steps[phase-1][1]);
        snprintf(path,sizeof(path),"tmp/native-farmer-%u-%u.idx",phase-1,commits);}
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Farmer phase%u commit%u C=%u SDL=%u\n",phase,commits,d,sd);
    if(d){FILE *out=fopen("tmp/farmer-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){memset(e,0,sizeof(*e));
    if(timeout>0&&timeout<=67){fake_clock+=(Uint32)timeout;return 0;}
    /* Compare the actual entry commit; no frame or cache replacement. */
    if(!phase){assert(commits==1);state(farmer_entry);}
    else {assert(commits==farmer_steps[phase-1][1]);state(farmer_steps[phase-1]+2);}
    if(phase==sizeof(farmer_steps)/sizeof(*farmer_steps)){e->type=SDL_QUIT;return 1;}
    /* The host dispatch scope ends before the next input wait; it is not
       original RAM. Every other byte of the retained state must match. */
    NpeWorld retained=world_map_frame_state;
    assert(!live->defer_story_events);
    retained.defer_story_events=live->defer_story_events;
    assert(world_map_frame_valid&&world_map_frame_file==live->battle_file&&
        !memcmp(&retained,live,sizeof(*live)));
    e->type=SDL_KEYDOWN;e->key.keysym.sym=farmer_steps[phase][0]==8?SDLK_RIGHT:SDLK_UP;
    ++phase;commits=0;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    char position[100];snprintf(position,sizeof(position),"%u,%u,%u,%u,%u,%u",farmer_entry[1],
        farmer_entry[3]+farmer_entry[5],farmer_entry[4]+farmer_entry[6],farmer_entry[5],farmer_entry[6],farmer_entry[2]);
    char *args[]={"npe-sdl","--walk-preview",position,argv[1],NULL};assert(!run(4,args));
    assert(total==39);puts("Original bottom-edge farmer route: entry plus38 actual main/SDL commits; seven moves then two blocked keys matched");return 0;
}
