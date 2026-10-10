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
typedef struct {unsigned music,commits,mode,event,progress,plays,background,vx,vy,sx,sy,direction;ChainNpc npc[20];NpeScriptWalkCache cache[4];} RealCase;
#include "script_walk_real_native.h"
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
static int story_key(SDL_Event *e){memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;}
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 ms){fake_clock+=ms;}
static int no_input(SDL_Event *e){(void)e;return 0;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 f){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,f);}
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent no_input
/* Keep an indefinite story wait distinct from the audio bridge's10ms
   slices, so the scripted key reaches the dialogue after the last step. */
#define SDL_WaitEventTimeout(event,slice) ((void)(slice),wait_event(event,timeout))
#define SDL_WaitEvent story_key
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
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef npe_sound_select
static void request(void *context,unsigned section,int loop){
    (void)context;
    if(active){assert(section==22&&loop&&live->game_mode==1&&live->story_progress==168);
        assert(fake_clock==7*205u&&commits==41);++plays;}
    real_observer(real_context,section,loop);
}
static void range_request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    const NpeFile *file=live->battle_file;
    assert(context==player&&offset==file->sections[22].offset&&
           (uint32_t)length==file->sections[22].size&&callback==NPE_SOUND_CALLBACK_MAP);
    assert(active&&live->game_mode==1&&live->story_progress==168&&
           fake_clock==7*205u&&commits==41);
    ++plays;real_range_observer(context,offset,length,callback);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){const RealCase *c=&real_cases[case_index];live=w;player=p;
        NpeMap map;assert(!npe_map((const NpeFile *)w->battle_file,125,&map));
        assert(!npe_world_load_tiles(w,&map));
        for(unsigned i=0;i<4;++i){const NpeScriptWalkCache *a=&w->script_walk_events[i],*b=&c->cache[i];
            assert(a->x==b->x&&a->y==b->y&&a->marker==b->marker&&a->next_mode==b->next_mode&&a->next_id==b->next_id);}
        w->direction=2;w->random_state=11;w->music_enabled=(uint8_t)c->music;w->map_music_slot=2;
        w->lcd_background=(uint8_t)c->background;
        w->sound_section=c->music?22:0;w->sound_loop=(uint8_t)c->music;
        NpeWorld numeric=*w;numeric.game_mode=19;numeric.story_event_id=1;
        assert(npe_world_apply_script_walk_event((const NpeFile *)w->battle_file,&numeric)==1);
        assert(numeric.game_mode==5&&numeric.story_event_id==181&&numeric.x==20&&numeric.y==13&&numeric.random_state==11);
        npe_sound_select(p,w);real_observer=w->sound_observer;real_context=w->sound_context;
        real_range_observer=w->sound_range_observer;
        w->sound_observer=request;w->sound_range_observer=range_request;w->sound_context=p;
        SDL_PauseAudioDevice(p->device,1);
        SDL_LockAudioDevice(p->device);p->cursor=128;SDL_UnlockAudioDevice(p->device);
    }
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const RealCase *c=&real_cases[case_index];assert(commits<c->commits);
    if(commits==41)assert(live->game_mode==1&&live->story_progress==168&&!live->map_init_pending);
    char path[160];snprintf(path,sizeof(path),"tmp/native-script-walk-real-%u-%u.idx",case_index,commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"NPC chain music%u commit%u C=%u SDL=%u clock%u\n",c->music,commits,d,sd,fake_clock);
    if(d){FILE *out=fopen("tmp/script-walk-real-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){memset(e,0,sizeof(*e));
    if(active&&timeout>=0&&timeout<=16&&commits<real_cases[case_index].commits){fake_clock+=(Uint32)timeout;return 0;}
    if(active&&live->game_mode==5){e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;}
    if(!started){started=active=1;live->game_mode=19;live->story_event_id=1;
        e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F12;return 1;}
    const RealCase *c=&real_cases[case_index];assert(commits==c->commits);
    assert(live->game_mode==c->mode&&live->story_event_id==c->event&&live->story_progress==c->progress&&live->random_state==11);
    assert(live->view.offset_x==c->vx&&live->view.offset_y==c->vy&&live->view.screen_x==c->sx&&live->view.screen_y==c->sy);
    assert(live->direction==c->direction&&live->x==20&&live->y==13);
    NpeMap map;assert(!npe_map((const NpeFile *)live->battle_file,125,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *npc;unsigned x,y;assert(!npe_map_npc(&map,i,&npc));
        npe_world_npc_position(live,npc,i,&x,&y);unsigned dir=live->npc_override_direction[i]<=3?live->npc_override_direction[i]:npc[6];
        assert(x==c->npc[i].x&&y==c->npc[i].y&&dir==c->npc[i].direction);}
    fprintf(stderr,"chain return init=%u clock=%u plays=%u cursor=%u\n",
        live->map_init_pending,fake_clock,plays,npe_sound_cursor(player));
    assert(!live->map_init_pending&&fake_clock==7*205u&&plays==c->plays&&npe_sound_cursor(player)==(c->music?0:128));
    active=0;e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<sizeof(real_cases)/sizeof(*real_cases);++case_index){
        live=NULL;commits=fake_clock=plays=active=started=0;
        char *args[]={"npe-sdl","--walk-preview","126,20,20,13,16,168",argv[1],NULL};assert(!run(4,args));
    }
    printf("Actual NPE temple script walk: %u original C/SDL commits, original four-slot cache, seven steps, story181, map and audio/SDL cursor matched\n",total);
    return 0;
}
