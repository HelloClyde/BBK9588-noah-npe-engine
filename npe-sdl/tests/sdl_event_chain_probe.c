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
typedef struct {unsigned sx,sy,vx,vy,direction,background,count;ChainNpc npc[20];uint8_t state[250];} ChainFixture;
#include "event_chain_native.h"
static NpeWorld *live;
static NpeSoundPlayer *player;
static NpeSoundObserver observer;
static NpeSoundRangeObserver range_observer;
static void *observer_context;
static unsigned scenario,started,active,commits,services,plays,closed;
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static void close_sound(NpeSoundPlayer *);
static void present(SDL_Renderer *);
static int wait_event(SDL_Event *,int);
static int execute_story(NpeWorld *,unsigned);
static void pump_events(void);
static Uint32 ticks(void){return 0;}
static void delay(Uint32 n){assert(!n);}
static int no_input(SDL_Event *e){(void)e;return 0;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 f){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,f);}
#define SDL_PumpEvents pump_events
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent no_input
#define SDL_WaitEventTimeout wait_event
#define SDL_RenderPresent present
#define SDL_CreateWindow probe_window
#define npe_sound_select select_sound
#define npe_sound_close close_sound
#define npe_world_execute_story_event execute_story
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PumpEvents
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef npe_sound_select
#undef npe_sound_close
#undef npe_world_execute_story_event
static void queue_quit(void){SDL_Event e={0};e.type=SDL_QUIT;assert(SDL_PushEvent(&e)==1);}
static void pump_events(void){SDL_PumpEvents();}
static int execute_story(NpeWorld *w,unsigned id){
    assert(w==live&&w->defer_story_events&&id==15);++services;
    if(scenario==1&&services==chain.count){w->story_events[14].mode=1;w->story_events[14].next_id=777;}
    return npe_world_execute_story_event(w,id);
}
static void check_request(unsigned section,int loop){
    if(active){assert(scenario==1&&commits==chain.count&&live->game_mode==1&&section==22&&loop);++plays;}
}
static void request(void *context,unsigned section,int loop){
    (void)context;check_request(section,loop);observer(observer_context,section,loop);
}
static void range_request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    (void)context;const NpeFile *file=live->battle_file;
    assert(offset==file->sections[22].offset&&length==(int32_t)file->sections[22].size&&callback==NPE_SOUND_CALLBACK_MAP);
    check_request(22,callback!=0);range_observer(observer_context,offset,length,callback);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){if(live)return;
    live=w;player=p;NpeMap map;assert(!npe_map(w->battle_file,0,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *npc;assert(!npe_map_npc(&map,i,&npc));((uint8_t *)npc)[7]=0;
        w->npc_override_x[i]=(uint16_t)chain.npc[i].x;w->npc_override_y[i]=(uint16_t)chain.npc[i].y;w->npc_override_direction[i]=(uint8_t)chain.npc[i].direction;}
    memcpy(w->npc_state,chain.state,250);w->direction=(uint8_t)chain.direction;w->lcd_background=(uint8_t)chain.background;
    w->random_state=11;w->music_enabled=1;w->map_music_slot=2;w->sound_section=22;w->sound_loop=1;
    w->story_events[14]=(NpeMapStoryEvent){.mode=14,.next_id=15,.story_progress=1};
    npe_sound_select(p,w);observer=w->sound_observer;observer_context=w->sound_context;
    range_observer=w->sound_range_observer;assert(range_observer);
    w->sound_observer=request;w->sound_range_observer=range_request;
    SDL_PauseAudioDevice(p->device,1);SDL_LockAudioDevice(p->device);p->cursor=128;SDL_UnlockAudioDevice(p->device);
}
static void check_final(void){
    assert(commits==chain.count+(scenario==1)&&services==chain.count&&live->story_progress==1&&live->random_state==11&&!live->defer_story_events);
    assert(live->game_mode==(scenario==1?1:14)&&live->story_event_id==(scenario==1?777:15)&&live->map_init_pending==(scenario==1?0:1));
    if(memcmp(live->npc_state,chain.state,250)||plays!=(scenario==1)||npe_sound_cursor(player)!=(scenario==1?0:128))
        fprintf(stderr,"Chain final scenario%u NPCdiff=%d plays=%u cursor=%u\n",scenario,!!memcmp(live->npc_state,chain.state,250),plays,(unsigned)npe_sound_cursor(player));
    assert(!memcmp(live->npc_state,chain.state,250)&&plays==(scenario==1)&&npe_sound_cursor(player)==(scenario==1?0:128));
    assert(live->x==chain.vx+chain.sx&&live->y==chain.vy+chain.sy&&live->view.offset_x==chain.vx&&live->view.offset_y==chain.vy);
    NpeMap map;assert(!npe_map(live->battle_file,0,&map));for(unsigned i=0;i<20;++i){const uint8_t *n;unsigned x,y;
        assert(!npe_map_npc(&map,i,&n));npe_world_npc_position(live,n,i,&x,&y);assert(x==chain.npc[i].x&&y==chain.npc[i].y);}
}
static void close_sound(NpeSoundPlayer *p){assert(p==player);check_final();++closed;npe_sound_close(p);}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    assert(commits<chain.count+(scenario==1));
    if(commits<chain.count)assert(live->game_mode==14&&live->map_init_pending);
    char path[120];snprintf(path,sizeof(path),"tmp/native-event-chain-%u.idx",commits<chain.count?commits:0);
    uint8_t ref[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(ref,1,sizeof(ref),f)==sizeof(ref)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(ref);++i){unsigned v=ref[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Chain scenario%u frame%u mode%u C=%u SDL=%u\n",scenario,commits,live->game_mode,d,sd);assert(!d&&!sd);
    ++commits;if(!scenario&&commits==chain.count)queue_quit();
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;memset(e,0,sizeof(*e));
    if(!started){started=active=1;live->game_mode=14;live->story_event_id=15;e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F12;return 1;}
    assert(scenario==1);check_final();e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(scenario=0;scenario<2;++scenario){live=NULL;started=active=commits=services=plays=closed=0;char preview[90];
        snprintf(preview,sizeof(preview),"1,%u,%u,%u,%u",chain.vx+chain.sx,chain.vy+chain.sy,chain.vx,chain.vy);
        char *args[]={"npe-sdl","--walk-preview",preview,argv[1],NULL};assert(!run(4,args)&&closed==1);}
    // Repeated mode9 insertion checks the numeric backend only. The original
    // display service waits at its notice; sdl-skill-service-probe covers it.
    uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    NpeMap map;NpeMapSkillEvent event;unsigned number;
    for(number=1;number<=file.offset_table_records[8];++number){assert(!npe_map(&file,number-1,&map));if(npe_map_skill_event(&map,1,&event)==1)break;}
    assert(number<=file.offset_table_records[8]);uint8_t *raw=(uint8_t *)event.raw;uint8_t saved[7];memcpy(saved,raw,7);
    raw[2]=1;raw[3]=1;raw[4]=9;raw[5]=0;raw[6]=1;
    assert(!SDL_Init(SDL_INIT_VIDEO));SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    NpeWorld w;assert(!npe_world_init(&file,&w,number,1,1));w.game_mode=9;w.story_event_id=1;w.defer_story_events=1;
    SDL_Event key={0};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_SPACE;assert(SDL_PushEvent(&key)==1);
    for(unsigned i=0;i<1029;++i)assert(npe_world_apply_skill_event(&file,&w)==1);
    assert(w.game_mode==9&&w.story_event_id==1&&w.defer_story_events==1);
    assert(w.learned_skill_count[0]==18);for(unsigned i=0;i<18;++i)assert(w.skill_slots[0][i]==1);
    // A pending host close stops actual dispatch before it enters the notice,
    // leaving the game key to its owning input service.
    queue_quit();NpeWorld before=w;
    assert(advance_world_events(NULL,NULL,NULL,&file,NULL,&w,1,0)==-2&&!memcmp(&before,&w,sizeof(w)));
    assert(SDL_HasEvent(SDL_QUIT)&&SDL_PeepEvents(&key,1,SDL_GETEVENT,SDL_KEYDOWN,SDL_KEYDOWN)==1&&key.key.keysym.sym==SDLK_SPACE);
    unsigned old=services;w.defer_story_events=0;assert(advance_world_events(NULL,NULL,NULL,&file,NULL,&w,9,0)==-2&&services==old&&!w.defer_story_events);
    memcpy(raw,saved,7);npe_free(&file);SDL_Quit();
    puts("Event chain: 70 original C/SDL cycle frames, 70-service terminating diagnostic, actual main close propagation, 1029 numeric skill insertions, pre-dispatch close and retained input passed");return 0;
}
