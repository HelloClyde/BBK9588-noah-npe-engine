/* Run the actual title/new-game controller and walk to the nearest current-
   NPE trigger or facing NPC. No plot-state/attribute injection. Audio hardware and time
   are diagnostic adapters; pixels are checked against C, not original LCD. */
#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
static NpeWorld *live;
static FILE *report;
static Uint32 probe_clock;
static unsigned frames,inputs,started,planned,reached,completed,route_length,route_cursor,talk_route,talk_sent;
static uint8_t route[256];
static uint16_t expected_mode,expected_event;
static unsigned expected_move,expected_map;
static uint64_t fingerprint;
static int opening_wait(SDL_Event *,int,const char *);
static void present(SDL_Renderer *);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static int begin_talk(const NpeFile *,NpeWorld *);
static int step_display(const NpeFile *,NpeWorld *,unsigned,uint8_t *,NpeMapDisplayCommit,void *);
static Uint32 ticks(void){return probe_clock;}
static Uint64 seed_clock(void){return 1;}
static void delay(Uint32 n){probe_clock+=n;}
static int no_input(SDL_Event *e){(void)e;return 0;}
static void open_sound(NpeSoundPlayer *p,const NpeFile *f){(void)f;memset(p,0,sizeof(*p));p->active=p->selected_section=-1;}
static SDL_Window *small_window(const char *s,int x,int y,int w,int h,Uint32 f){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,f|SDL_WINDOW_HIDDEN);}
#define SDL_GetTicks ticks
#define SDL_GetPerformanceCounter seed_clock
#define SDL_Delay delay
#define SDL_PollEvent no_input
#define SDL_WaitEventTimeout(event,slice) opening_wait(event,timeout,caller)
#define SDL_WaitEvent(event) opening_wait(event,-1,"game_WaitEvent")
#define SDL_RenderPresent present
#define SDL_CreateWindow small_window
#define npe_sound_open open_sound
#define npe_sound_select select_sound
#define npe_world_step_with_display step_display
#define npe_world_begin_talk begin_talk
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_GetPerformanceCounter
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef npe_sound_open
#undef npe_sound_select
#undef npe_world_step_with_display
#undef npe_world_begin_talk

static void state(const char *phase){
    fprintf(report,"{\"phase\":\"%s\",\"frames\":%u,\"inputs\":%u,\"map\":%u,\"mode\":%u,\"event\":%u,\"progress\":%u,\"x\":%u,\"y\":%u}\n",
        phase,frames,inputs,live->map_number,live->game_mode,live->story_event_id,live->story_progress,live->x,live->y);fflush(report);
}
static void present(SDL_Renderer *r){
    SDL_RenderPresent(r);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){unsigned v=last_pixel_frame[i];assert(v<16);
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    assert(++frames<3000);if(live)state("frame");
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);}

typedef struct {unsigned x,y,parent,direction;NpeViewport view;} OpeningNode;
static int plan_route(void){
    NpeWorld *before=malloc(sizeof(*before));assert(before);*before=*live;
    const NpeFile *file=live->battle_file;NpeMap map;assert(file&&!npe_map(file,live->map_number-1,&map));
    unsigned cells=(unsigned)map.width*map.height;assert(cells&&live->x<map.width&&live->y<map.height);
    OpeningNode *nodes=malloc(cells*sizeof(*nodes));int *seen=malloc(cells*sizeof(*seen));
    NpeWorld *candidate=malloc(sizeof(*candidate));assert(nodes&&seen&&candidate);
    for(unsigned i=0;i<cells;++i)seen[i]=-1;
    nodes[0]=(OpeningNode){live->x,live->y,0,0,live->view};seen[live->y*map.width+live->x]=0;
    unsigned head=0,tail=1,found=0,last=0,direction=0;
    unsigned exit_found=0,exit_node=0,exit_direction=0,exit_map=0,exit_mode=0,exit_event=0;
    while(head<tail&&!found){OpeningNode node=nodes[head];
        for(unsigned d=0;d<4;++d){
            *candidate=*live;candidate->x=node.x;candidate->y=node.y;candidate->view=node.view;
            candidate->encounters_enabled=0;candidate->battle_file=NULL;candidate->defer_story_events=1;
            candidate->sound_observer=NULL;candidate->sound_range_observer=NULL;
            candidate->direction=d;unsigned npc_id;
            if(npe_world_facing_npc(file,candidate,&npc_id)==1){found=1;last=head;direction=d;talk_route=1;
                expected_move=NPE_MOVE_BLOCKED;expected_map=candidate->map_number;
                expected_mode=2;expected_event=(uint16_t)npc_id;break;}
            int result=npe_world_step(file,candidate,d);
            if(result==NPE_MOVE_EVENT){found=1;last=head;direction=d;
                expected_move=NPE_MOVE_EVENT;expected_map=candidate->map_number;
                expected_mode=candidate->game_mode;expected_event=candidate->story_event_id;break;}
            if(result==NPE_MOVE_EXIT&&!exit_found){exit_found=1;exit_node=head;exit_direction=d;
                exit_map=candidate->map_number;exit_mode=candidate->game_mode;exit_event=candidate->story_event_id;}
            if(result!=NPE_MOVE_OK||candidate->map_number!=live->map_number||candidate->game_mode!=1)continue;
            unsigned index=candidate->y*map.width+candidate->x;assert(index<cells);
            if(seen[index]>=0)continue;
            assert(tail<cells);seen[index]=(int)tail;
            nodes[tail++]=(OpeningNode){candidate->x,candidate->y,head,d,candidate->view};
        }
        ++head;
    }
    if(!found&&exit_found){found=1;last=exit_node;direction=exit_direction;expected_move=NPE_MOVE_EXIT;
        expected_map=exit_map;expected_mode=(uint16_t)exit_mode;expected_event=(uint16_t)exit_event;}
    if(found){route[route_length++]=(uint8_t)direction;
        while(last){assert(route_length<sizeof(route));route[route_length++]=(uint8_t)nodes[last].direction;last=nodes[last].parent;}
        for(unsigned i=0;i<route_length/2;++i){uint8_t swap=route[i];route[i]=route[route_length-1-i];route[route_length-1-i]=swap;}
    }
    fprintf(report,"{\"phase\":\"route\",\"found\":%s,\"length\":%u,\"expected_mode\":%u,\"expected_event\":%u,\"expected_map\":%u,\"expected_move\":%u,\"npc_dialogue\":%s,\"explored\":%u}\n",
        found?"true":"false",route_length,expected_mode,expected_event,expected_map,expected_move,talk_route?"true":"false",tail);fflush(report);
    assert(!memcmp(before,live,sizeof(*live)));free(before);
    free(candidate);free(seen);free(nodes);return found?0:-1;
}
static int step_display(const NpeFile *f,NpeWorld *w,unsigned d,uint8_t *pixels,NpeMapDisplayCommit commit,void *context){
    int result=npe_world_step_with_display(f,w,d,pixels,commit,context);
    assert(result>=0);if(!talk_route&&result==(int)expected_move){assert(planned&&route_cursor==route_length);
        assert(w->map_number==expected_map);
        assert(w->game_mode==expected_mode&&w->story_event_id==expected_event);reached=1;state("trigger");}
    return result;
}
static int begin_talk(const NpeFile *file,NpeWorld *w){
    int result=npe_world_begin_talk(file,w);assert(result==1&&talk_route&&talk_sent);
    assert(w->game_mode==expected_mode&&w->story_event_id==expected_event);reached=1;state("trigger");return result;
}
static int opening_wait(SDL_Event *e,int timeout,const char *caller){
    memset(e,0,sizeof(*e));
    if(strcmp(caller,"run")){
        if(timeout>=0){probe_clock+=(Uint32)timeout;return 0;}
        assert(++inputs<300);e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_SPACE;return 1;
    }
    assert(live);e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;
    if(!started){assert(live->game_mode==7);started=1;++inputs;state("title-confirm");return 1;}
    if(!planned){assert(live->game_mode==1);state("new-game");planned=1;
        if(plan_route()){e->type=SDL_QUIT;return 1;}}
    if(route_cursor<route_length){static const SDL_Keycode keys[4]={SDLK_DOWN,SDLK_RIGHT,SDLK_UP,SDLK_LEFT};
        assert(live->game_mode==1);e->key.keysym.sym=keys[route[route_cursor++]];++inputs;return 1;}
    if(talk_route&&!talk_sent){assert(live->game_mode==1);talk_sent=1;++inputs;e->key.keysym.sym=SDLK_SPACE;return 1;}
    assert(reached);assert(!npe_rpg_runtime_failures(live->battle_file));
    assert(fingerprint==npe_game_fingerprint(live->battle_file));completed=1;state("complete");e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){
    assert(argc==3);char path[1024];FILE *manifest=fopen(argv[1],"rb");
    assert(manifest&&fgets(path,sizeof(path),manifest)&&!fclose(manifest));path[strcspn(path,"\r\n")]=0;
    report=fopen(argv[2],"wb");assert(report);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(path,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    fingerprint=npe_game_fingerprint(&file);npe_free(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    char *args[]={"npe-sdl",path,NULL};int result=run(2,args);
    assert(!fclose(report));fprintf(stderr,"natural opening result=%d completed=%u frames=%u inputs=%u route=%u\n",result,completed,frames,inputs,route_length);
    return result||!completed?1:0;
}
