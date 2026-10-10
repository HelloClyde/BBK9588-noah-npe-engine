#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned id,x,y,direction;} StoryNpc;
typedef struct {const char *name;unsigned input,next_mode,next_id,target_progress,music,
    commits,mode,event,progress,plays,background,vx,vy,sx,sy,direction;StoryNpc npc[20];} StoryCase;
#include "story_dispatch_native.h"
static NpeWorld *live;
static NpeSoundPlayer *player;
static NpeSoundObserver real_observer;
static NpeSoundRangeObserver real_range_observer;
static void *real_context;
static unsigned case_index,commits,total,plays,active,started;
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static void present(SDL_Renderer *);
static int wait_event(SDL_Event *,int);
static Uint32 ticks(void){return 0;}
static void delay(Uint32 ms){assert(!ms);}
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
static void request(void *context,unsigned section,int loop){
    (void)context;
    /* Map replay now uses the original cached file range, not a section
       observer. An active story must not issue a substitute legacy request. */
    assert(!active);
    real_observer(real_context,section,loop);
}
static void request_range(void *context,uint32_t offset,int32_t length,uint32_t callback){
    const StoryCase *c=&story_cases[case_index];const NpeFile *file=live->battle_file;
    assert(context==real_context&&offset==file->sections[22].offset&&
           length==(int32_t)file->sections[22].size&&callback==UINT32_C(0x31a3f4));
    assert(active&&c->music&&live->game_mode==1&&live->story_progress==c->progress&&commits==1);
    ++plays;real_range_observer(context,offset,length,callback);
    assert(player->selected_section==22&&player->loop&&npe_sound_cursor(player)==0);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){const StoryCase *c=&story_cases[case_index];live=w;player=p;
        const NpeFile *file=w->battle_file;assert(npe_rpg_runtime_owner(file)==w);
        for(unsigned i=0;i<20;++i){uint8_t moving=0;assert(!npe_rpg_runtime_write(file,0x526ca2+i*14+11,&moving,1));
            w->npc_override_x[i]=(uint16_t)c->npc[i].x;w->npc_override_y[i]=(uint16_t)c->npc[i].y;w->npc_override_direction[i]=(uint8_t)c->npc[i].direction;}
        const uint8_t event[]={(uint8_t)(c->next_id>>8),(uint8_t)c->next_id,
            (uint8_t)(c->target_progress>>8),(uint8_t)c->target_progress,(uint8_t)c->next_mode};
        assert(!npe_rpg_runtime_write(file,0x527548,event,sizeof(event)));
        assert(w->story_event_count==8); /* The service may still execute slot15. */
        w->direction=(uint8_t)c->direction;w->random_state=11;w->music_enabled=(uint8_t)c->music;w->map_music_slot=2;
        w->lcd_background=(uint8_t)c->background;w->sound_section=c->music?22:0;w->sound_loop=(uint8_t)c->music;
        NpeWorld numeric=*w;numeric.game_mode=14;numeric.story_event_id=(uint16_t)c->input;
        unsigned valid=(c->input&255)>=1&&(c->input&255)<=15;
        assert(npe_world_apply_story_event(&numeric,c->input)==(int)valid);
        assert(numeric.game_mode==(valid?c->next_mode:0)&&numeric.story_progress==(valid?c->target_progress:1)&&numeric.story_event_id==(valid?c->next_id:c->input));
        npe_sound_select(p,w);real_observer=w->sound_observer;real_context=w->sound_context;
        real_range_observer=w->sound_range_observer;assert(real_range_observer);
        w->sound_observer=request;w->sound_range_observer=request_range;
        w->sound_context=p;SDL_PauseAudioDevice(p->device,1);
        SDL_LockAudioDevice(p->device);p->cursor=128;SDL_UnlockAudioDevice(p->device);
    }
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const StoryCase *c=&story_cases[case_index];assert(commits<c->commits);
    if(!commits)assert(live->game_mode==14&&live->story_progress==1&&live->map_init_pending);
    else assert(live->game_mode==1&&live->story_progress==c->progress&&!live->map_init_pending);
    char path[160];snprintf(path,sizeof(path),"tmp/native-story-dispatch-%u-%u.idx",case_index,commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Story %s commit%u C=%u SDL=%u\n",c->name,commits,d,sd);
    if(d){FILE *out=fopen("tmp/story-dispatch-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;memset(e,0,sizeof(*e));
    if(!started){started=active=1;live->game_mode=14;live->story_event_id=(uint16_t)story_cases[case_index].input;
        e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F12;return 1;}
    const StoryCase *c=&story_cases[case_index];assert(commits==c->commits);
    assert(live->game_mode==c->mode&&live->story_event_id==c->event&&live->story_progress==c->progress&&live->random_state==11);
    assert(!live->map_init_pending&&plays==c->plays&&npe_sound_cursor(player)==(c->music?0:128));
    active=0;e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<sizeof(story_cases)/sizeof(*story_cases);++case_index){
        live=NULL;commits=plays=active=started=0;char preview[80];const StoryCase *c=&story_cases[case_index];
        snprintf(preview,sizeof(preview),"1,%u,%u,%u,%u",c->vx+c->sx,c->vy+c->sy,c->vx,c->vy);
        char *args[]={"npe-sdl","--walk-preview",preview,argv[1],NULL};assert(!run(4,args));
    }
    printf("Original story dispatch: %u actual C/SDL commits, retained slot15, low byte, invalid/empty/followup returns and audio cursor matched\n",total);
    return 0;
}
