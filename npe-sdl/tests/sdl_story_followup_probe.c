#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned id,x,y,direction;} FollowupNpc;
typedef struct {const char *name;unsigned entry_mode,entry_event,map,entry_progress,music,
    commits,mode,event,progress,context,npc4,plays,background,vx,vy,sx,sy,direction,
    before_npc3,before_npc4;FollowupNpc npc[20];} FollowupCase;
#include "story_followup_native.h"
static NpeWorld *live;
static NpeSoundPlayer *player;
static NpeSoundObserver real_observer;
static NpeSoundRangeObserver real_range_observer;
static void *real_context;
static unsigned case_index,commits,total,plays,active,started,entry_commits;
static Uint32 fake_clock;
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static void present(SDL_Renderer *);
static int wait_event(SDL_Event *,int);
static int logical_wait(SDL_Event *,int,const char *);
static int story_key(SDL_Event *e){memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_SPACE;return 1;}
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 ms){fake_clock+=ms;}
static int no_input(SDL_Event *e){(void)e;return 0;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 f){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,f);}
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent no_input
#define SDL_WaitEventTimeout(event,slice) logical_wait(event,timeout,caller)
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
    (void)context;assert(!active);
    real_observer(real_context,section,loop);
}
static void request_range(void *context,uint32_t offset,int32_t length,uint32_t callback){
    const FollowupCase *c=&followup_cases[case_index];const NpeFile *file=live->battle_file;
    assert(context==real_context&&offset==file->sections[22].offset&&
           length==(int32_t)file->sections[22].size&&callback==UINT32_C(0x31a3f4));
    assert(active&&c->music&&live->game_mode==1&&live->story_progress==c->progress);
    assert(entry_commits==1&&commits==c->commits-1);++plays;
    real_range_observer(context,offset,length,callback);
    assert(player->selected_section==22&&player->loop&&npe_sound_cursor(player)==0);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){const FollowupCase *c=&followup_cases[case_index];live=w;player=p;
        const NpeFile *file=w->battle_file;NpeMap map;assert(!npe_map(file,c->map-1,&map));
        assert(npe_rpg_runtime_owner(file)==w);
        uint64_t fingerprint=npe_game_fingerprint(file);
        for(unsigned i=0;i<20;++i){uint8_t moving=0;assert(!npe_rpg_runtime_write(file,0x526ca2+i*14+11,&moving,1));
            w->npc_override_x[i]=(uint16_t)c->npc[i].x;w->npc_override_y[i]=(uint16_t)c->npc[i].y;w->npc_override_direction[i]=(uint8_t)c->npc[i].direction;}
        assert(npe_game_fingerprint(file)==fingerprint);
        w->npc_state[3]=(uint8_t)c->before_npc3;w->npc_state[4]=(uint8_t)c->before_npc4;
        w->direction=(uint8_t)c->direction;w->random_state=11;w->music_enabled=(uint8_t)c->music;w->map_music_slot=2;
        w->lcd_background=(uint8_t)c->background;w->sound_section=c->music?22:0;w->sound_loop=(uint8_t)c->music;
        /* Keep diagnostic and actual main World RAM owners independent. */
        NpeFile numeric_file=*file;numeric_file.rpg_runtime=NULL;
        NpeWorld numeric=*w;numeric.battle_file=&numeric_file;assert(!numeric.defer_story_events);
        if(c->entry_mode==5){assert(npe_world_step(&numeric_file,&numeric,1)==NPE_MOVE_EVENT);
            assert(numeric.game_mode==5&&numeric.story_event_id==c->entry_event);
            assert(npe_world_finish_story(&numeric_file,&numeric)==1);
        }else{assert(npe_world_begin_talk(&numeric_file,&numeric)==1);
            assert(numeric.game_mode==2&&numeric.story_event_id==c->entry_event);
            assert(npe_world_prepare_mode2(&numeric_file,&numeric)==1&&npe_world_finish_mode2(&numeric)==1);}
        assert(numeric.game_mode==c->mode&&numeric.story_event_id==c->event&&numeric.story_progress==c->progress&&numeric.npc_state[4]==c->npc4);
        npe_rpg_runtime_close(&numeric_file);
        npe_sound_select(p,w);real_observer=w->sound_observer;real_context=w->sound_context;
        real_range_observer=w->sound_range_observer;assert(real_range_observer);
        w->sound_observer=request;w->sound_range_observer=request_range;w->sound_context=p;SDL_PauseAudioDevice(p->device,1);
        SDL_LockAudioDevice(p->device);p->cursor=128;SDL_UnlockAudioDevice(p->device);
    }
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const FollowupCase *c=&followup_cases[case_index];assert(commits<c->commits);
    if(live->game_mode==14){assert(live->story_progress==c->entry_progress&&live->map_init_pending&&live->defer_story_events);++entry_commits;}
    char path[160];snprintf(path,sizeof(path),"tmp/native-story-followup-%u-%u.idx",case_index,commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Followup %s music%u commit%u mode%u progress%u C=%u SDL=%u\n",c->name,c->music,commits,live->game_mode,live->story_progress,d,sd);
    if(d){FILE *out=fopen("tmp/story-followup-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){memset(e,0,sizeof(*e));
    if(active&&timeout>=0&&timeout<2000){fake_clock+=(Uint32)timeout;return 0;}
    if(!started){started=active=1;e->type=SDL_KEYDOWN;e->key.keysym.sym=followup_cases[case_index].entry_mode==5?SDLK_RIGHT:SDLK_SPACE;return 1;}
    const FollowupCase *c=&followup_cases[case_index];assert(commits==c->commits&&entry_commits==1);
    assert(live->game_mode==c->mode&&live->story_event_id==c->event&&live->story_progress==c->progress&&live->random_state==11&&live->event_context_id==c->context&&live->npc_state[4]==c->npc4);
    assert(!live->map_init_pending&&!live->defer_story_events&&plays==c->plays&&npe_sound_cursor(player)==(c->music?0:128));
    active=0;e->type=SDL_QUIT;return 1;
}
static int logical_wait(SDL_Event *event,int timeout,const char *caller){
    /* Dialogue input belongs to its logical wait, not an audio poll slice. */
    return !strcmp(caller,"game_WaitEvent")?story_key(event):wait_event(event,timeout);
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<sizeof(followup_cases)/sizeof(*followup_cases);++case_index){
        live=NULL;fake_clock=commits=plays=active=started=entry_commits=0;char preview[100];const FollowupCase *c=&followup_cases[case_index];
        snprintf(preview,sizeof(preview),"%u,%u,%u,%u,%u,%u",c->map,c->vx+c->sx,c->vy+c->sy,c->vx,c->vy,c->entry_progress);
        char *args[]={"npe-sdl","--walk-preview",preview,argv[1],NULL};assert(!run(4,args));
    }
    printf("Original NPE story/dialogue followup: %u actual C/SDL commits, old-progress mode14 entry, new-progress return and retained music cursor matched\n",total);return 0;
}
