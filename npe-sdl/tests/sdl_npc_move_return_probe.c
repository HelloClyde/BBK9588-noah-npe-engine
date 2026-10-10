#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned id,x,y,direction;} ChainNpc;
typedef struct {unsigned music,commits,mode,event,progress,plays,background;ChainNpc npc[20];} ChainCase;
#include "npc_move_return_native.h"
static NpeWorld *live;
static NpeSoundPlayer *player;
static NpeSoundObserver real_observer;
static NpeSoundRangeObserver real_range_observer;
static void *real_context;
static unsigned case_index,commits,total,plays,active,started;
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
static void check_request(unsigned section,int loop){
    if(active){assert(section==22&&loop&&live->game_mode==1&&live->story_progress==1);
        assert(fake_clock==3*141u&&commits==7);++plays;}
}
static void request(void *context,unsigned section,int loop){
    (void)context;check_request(section,loop);
    real_observer(real_context,section,loop);
}
static void range_request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    (void)context;
    const NpeFile *file=live->battle_file;
    assert(offset==file->sections[22].offset&&length==(int32_t)file->sections[22].size&&
        callback==NPE_SOUND_CALLBACK_MAP);
    check_request(22,callback!=0);
    real_range_observer(real_context,offset,length,callback);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){const ChainCase *c=&chain_cases[case_index];live=w;player=p;
        NpeMap map;assert(!npe_map((const NpeFile *)w->battle_file,0,&map));
        w->npc_move_events[0]=(NpeNpcMoveCache){.npc_id=1,.x=9,.y=9,.next_mode=1,.next_id=0};
        w->npc_override_x[0]=6;w->npc_override_y[0]=9;
        for(unsigned i=0;i<20;++i){const uint8_t *npc;assert(!npe_map_npc(&map,i,&npc));((uint8_t *)npc)[7]=0;}
        w->direction=1;w->random_state=11;w->music_enabled=(uint8_t)c->music;w->map_music_slot=2;
        w->lcd_background=(uint8_t)c->background;
        w->sound_section=c->music?22:0;w->sound_loop=(uint8_t)c->music;
        npe_sound_select(p,w);real_observer=w->sound_observer;real_context=w->sound_context;
        real_range_observer=w->sound_range_observer;assert(real_range_observer);
        w->sound_observer=request;w->sound_range_observer=range_request;
        w->sound_context=p;SDL_PauseAudioDevice(p->device,1);
        SDL_LockAudioDevice(p->device);p->cursor=128;SDL_UnlockAudioDevice(p->device);
    }
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const ChainCase *c=&chain_cases[case_index];assert(commits<c->commits);
    if(commits==7)assert(live->game_mode==1&&live->story_progress==1&&!live->map_init_pending);
    char path[160];snprintf(path,sizeof(path),"tmp/native-npc-move-return-%u-%u.idx",case_index,commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"NPC chain music%u commit%u C=%u SDL=%u clock%u\n",c->music,commits,d,sd,fake_clock);
    if(d){FILE *out=fopen("tmp/npc-move-return-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;memset(e,0,sizeof(*e));
    if(!started){started=active=1;live->game_mode=18;live->story_event_id=1;
        e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F12;return 1;}
    const ChainCase *c=&chain_cases[case_index];assert(commits==c->commits);
    assert(live->game_mode==c->mode&&live->story_event_id==c->event&&live->story_progress==c->progress&&live->random_state==11);
    NpeMap map;assert(!npe_map((const NpeFile *)live->battle_file,0,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *npc;unsigned x,y;assert(!npe_map_npc(&map,i,&npc));
        npe_world_npc_position(live,npc,i,&x,&y);unsigned dir=live->npc_override_direction[i]<=3?live->npc_override_direction[i]:npc[6];
        assert(x==c->npc[i].x&&y==c->npc[i].y&&dir==c->npc[i].direction);}
    assert(!live->map_init_pending&&fake_clock==3*141u&&plays==c->plays&&npe_sound_cursor(player)==(c->music?0:128));
    active=0;e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<sizeof(chain_cases)/sizeof(*chain_cases);++case_index){
        live=NULL;commits=fake_clock=plays=active=started=0;
        char *args[]={"npe-sdl","--walk-preview","1,7,9,0,5",argv[1],NULL};assert(!run(4,args));
    }
    printf("Original NPC map return: %u original C/SDL commits, retained map initialization, audio/SDL cursor matched\n",total);
    return 0;
}
