#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned id,x,y,direction;} ReturnNpc;
typedef struct {const char *name;unsigned npc_id,map,progress,music,commits,mode,event,context,
    plays,background,vx,vy,sx,sy,direction;ReturnNpc npc[20];uint8_t before_state[250],after_state[250];
#ifdef NPE_INVALID_TEXT_FOLLOWUP
    unsigned patched_mode,patched_event,target_mode;
#endif
} ReturnCase;
#ifdef NPE_INVALID_TEXT_FOLLOWUP
#include "dialogue_invalid_native.h"
#define RETURN_FRAME_PREFIX "tmp/native-dialogue-invalid"
#else
#include "dialogue_return_native.h"
#define RETURN_FRAME_PREFIX "tmp/native-dialogue-return"
#endif
static NpeWorld *live;
static NpeSoundPlayer *player;
static NpeSoundObserver real_observer;
static NpeSoundRangeObserver real_range_observer;
static void *real_context;
static unsigned case_index,commits,total,plays,active,started,armed,story_calls;
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static void present(SDL_Renderer *);
static int wait_event(SDL_Event *,int);
static int finish_story(const NpeFile *,NpeWorld *);
static int story_key(SDL_Event *e){
    if(live->game_mode==5)assert(!live->map_init_pending);
#ifdef NPE_INVALID_TEXT_FOLLOWUP
    const ReturnCase *c=&return_cases[case_index];
    if(live->game_mode==c->target_mode){
        live->mode2_followup_mode=(uint8_t)c->patched_mode;
        live->mode2_followup_event_id=(uint16_t)c->patched_event;
        if(live->game_mode==5)assert(live->story_control_ready);
    }
#endif
    memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_SPACE;return 1;
}
static Uint32 ticks(void){return 0;}
static void delay(Uint32 ms){assert(!ms);}
static int no_input(SDL_Event *e){(void)e;return 0;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 f){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,f);}
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent no_input
/* Story keys belong to the logical blocking wait; the bridge uses a
   timed SDL backend for it while preserving this captured sequence. */
#define SDL_WaitEventTimeout(event,slice) (!strcmp(caller,"game_WaitEvent")?story_key(event):wait_event(event,slice))
#define SDL_WaitEvent story_key
#define SDL_RenderPresent present
#define SDL_CreateWindow probe_window
#define npe_sound_select select_sound
#define npe_world_finish_story finish_story
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
#undef npe_world_finish_story
static int finish_story(const NpeFile *file,NpeWorld *w){
    NpeStoryRecord story;assert(!npe_story_record(file,w->story_event_id-1,&story));
    assert(w->map_init_pending&&w->defer_story_events&&story.control_byte==1);++story_calls;
    int result=npe_world_finish_story(file,w);
#ifdef NPE_INVALID_TEXT_FOLLOWUP
    const ReturnCase *c=&return_cases[case_index];
    assert(result==1&&w->game_mode==c->patched_mode&&w->story_event_id==c->patched_event&&!w->story_control_ready);
#endif
    return result;
}
static void request(void *context,unsigned section,int loop){
    (void)context;const ReturnCase *c=&return_cases[case_index];
    if(active){assert(section==22&&loop&&live->game_mode==1&&live->story_progress==c->progress);
        assert(armed&&commits==c->commits-1);++plays;}
    real_observer(real_context,section,loop);
}
/* Observe the same native map request through its live range adapter. */
static void range_request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    const ReturnCase *c=&return_cases[case_index];const NpeFile *file=live->battle_file;
    assert(offset==file->sections[22].offset && (uint32_t)length==file->sections[22].size && callback==NPE_SOUND_CALLBACK_MAP);
    if(active){assert(live->game_mode==1 && live->story_progress==c->progress);
        assert(armed && commits==c->commits-1);++plays;}
    real_range_observer(context,offset,length,callback);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){const ReturnCase *c=&return_cases[case_index];live=w;player=p;
        const NpeFile *file=w->battle_file;NpeMap map;assert(!npe_map(file,c->map-1,&map));
        for(unsigned i=0;i<20;++i){const uint8_t *npc;assert(!npe_map_npc(&map,i,&npc));((uint8_t *)npc)[7]=0;
            w->npc_override_x[i]=(uint16_t)c->npc[i].x;w->npc_override_y[i]=(uint16_t)c->npc[i].y;w->npc_override_direction[i]=(uint8_t)c->npc[i].direction;}
        memcpy(w->npc_state,c->before_state,sizeof(c->before_state));
        w->direction=(uint8_t)c->direction;w->random_state=11;w->music_enabled=(uint8_t)c->music;w->map_music_slot=2;
        w->lcd_background=(uint8_t)c->background;w->sound_section=c->music?22:0;w->sound_loop=(uint8_t)c->music;
        /* The production loader owns live RAM. Give this diagnostic World
           its own runtime rather than sharing the actual main World heap. */
        NpeFile numeric_file=*file;numeric_file.rpg_runtime=NULL;
        NpeWorld numeric=*w;numeric.battle_file=&numeric_file;
        assert(!numeric.defer_story_events&&npe_world_begin_talk(&numeric_file,&numeric)==1);
        assert(numeric.game_mode==2&&numeric.story_event_id==c->npc_id);
        assert(npe_world_prepare_mode2(&numeric_file,&numeric)==1);
#ifdef NPE_INVALID_TEXT_FOLLOWUP
        if(c->target_mode==2){numeric.mode2_followup_mode=(uint8_t)c->patched_mode;numeric.mode2_followup_event_id=(uint16_t)c->patched_event;}
#endif
        assert(npe_world_finish_mode2(&numeric)==1);
        for(unsigned i=0;numeric.game_mode==5&&i<4;++i){
#ifdef NPE_INVALID_TEXT_FOLLOWUP
            assert(npe_world_prepare_story(&numeric_file,&numeric)==1);
            numeric.mode2_followup_mode=(uint8_t)c->patched_mode;numeric.mode2_followup_event_id=(uint16_t)c->patched_event;
#endif
            assert(npe_world_finish_story(&numeric_file,&numeric)==1);
        }
        assert(numeric.game_mode==c->mode&&numeric.story_event_id==c->event&&numeric.story_progress==c->progress);
        assert(!memcmp(numeric.npc_state,c->after_state,sizeof(c->after_state)));
        npe_rpg_runtime_close(&numeric_file);
        npe_sound_select(p,w);real_observer=w->sound_observer;real_context=w->sound_context;
        real_range_observer=w->sound_range_observer;w->sound_range_observer=range_request;
        w->sound_observer=request;w->sound_context=p;SDL_PauseAudioDevice(p->device,1);
        SDL_LockAudioDevice(p->device);p->cursor=128;SDL_UnlockAudioDevice(p->device);
    }
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const ReturnCase *c=&return_cases[case_index];assert(commits<c->commits);
    if(live->map_init_pending)++armed;
    char path[160];snprintf(path,sizeof(path),RETURN_FRAME_PREFIX "-%u-%u.idx",case_index,commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Return %s music%u commit%u mode%u init%u C=%u SDL=%u\n",c->name,c->music,commits,live->game_mode,live->map_init_pending,d,sd);
    if(d){FILE *out=fopen("tmp/dialogue-return-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;memset(e,0,sizeof(*e));
    if(!started){started=active=1;e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_SPACE;return 1;}
    const ReturnCase *c=&return_cases[case_index];assert(commits==c->commits&&armed);
    assert(live->game_mode==c->mode&&live->story_event_id==c->event&&live->story_progress==c->progress&&live->random_state==11&&live->event_context_id==c->context);
    assert(!memcmp(live->npc_state,c->after_state,sizeof(c->after_state)));
    assert(live->x==c->vx+c->sx&&live->y==c->vy+c->sy&&live->direction==c->direction&&
           live->view.offset_x==c->vx&&live->view.offset_y==c->vy&&live->view.screen_x==c->sx&&live->view.screen_y==c->sy);
    NpeMap map;assert(!npe_map(live->battle_file,c->map-1,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *npc;unsigned x,y;assert(!npe_map_npc(&map,i,&npc));
        npe_world_npc_position(live,npc,i,&x,&y);assert(x==c->npc[i].x&&y==c->npc[i].y);}
    assert(!live->map_init_pending&&!live->defer_story_events&&plays==c->plays&&npe_sound_cursor(player)==(c->music?0:128));
    assert(story_calls==(c->npc_id==1?0u:1u));active=0;e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<sizeof(return_cases)/sizeof(*return_cases);++case_index){
        live=NULL;commits=plays=active=started=armed=story_calls=0;char preview[100];const ReturnCase *c=&return_cases[case_index];
        snprintf(preview,sizeof(preview),"%u,%u,%u,%u,%u,%u",c->map,c->vx+c->sx,c->vy+c->sy,c->vx,c->vy,c->progress);
        char *args[]={"npe-sdl","--walk-preview",preview,argv[1],NULL};assert(!run(4,args));
    }
    printf("Original NPC/plain/full/window returns: %u actual C/SDL commits, map initialization and retained music cursor matched\n",total);return 0;
}
