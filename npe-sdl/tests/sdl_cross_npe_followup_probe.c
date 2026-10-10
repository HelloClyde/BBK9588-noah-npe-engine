/* Actual production title/new-game and distinct reachable followups. The route
   search uses World copies; the live controller receives SDL keys only. Audio
   hardware and time are diagnostic adapters, not original LCD/PCM references. */
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
static unsigned frames,inputs,started,planned,reached,completed,checkpoints;
static unsigned route_length,route_cursor,talk_sent,replans,encounters,limit=12;
static unsigned boost,attribute_edits;
static uint8_t route[1024];
static uint16_t expected_mode,expected_event;
static unsigned expected_move,expected_map;
static uint64_t fingerprint;
enum {FOLLOW_NPC=1,FOLLOW_EVENT,FOLLOW_EXIT,FOLLOW_EXIT_CHOICE};
typedef struct {
    unsigned map,progress,kind,mode,event,destination,x,y;
} Followup;
static Followup current,visited[64];
static int followup_wait(SDL_Event *,int,const char *);
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
#define SDL_WaitEventTimeout(event,slice) followup_wait(event,timeout,caller)
#define SDL_WaitEvent(event) followup_wait(event,-1,"game_WaitEvent")
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
    fprintf(report,"{\"phase\":\"%s\",\"checkpoint\":%u,\"frames\":%u,\"inputs\":%u,\"map\":%u,\"mode\":%u,\"event\":%u,\"progress\":%u,\"x\":%u,\"y\":%u,\"exit_choice\":%u,\"encounters\":%u,\"replans\":%u,\"battle_phase\":%u,\"enemy_count\":%u,\"hp\":%u,\"map_init\":%u,\"attribute_edits\":%u}\n",
        phase,checkpoints,frames,inputs,live->map_number,live->game_mode,live->story_event_id,live->story_progress,live->x,live->y,live->exit_choice,encounters,replans,live->battle_menu_phase,live->battle_enemy_count,live->hp[0],live->map_init_pending,attribute_edits);fflush(report);
}
static void present(SDL_Renderer *r){
    SDL_RenderPresent(r);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){unsigned v=last_pixel_frame[i];assert(v<16);
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    assert(++frames<12000);if(live)state("frame");
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);}
static int already_visited(const Followup *entry){
    for(unsigned i=0;i<checkpoints;++i){const Followup *v=visited+i;
        if(v->map!=entry->map || v->kind!=entry->kind)continue;
        if(entry->kind==FOLLOW_EXIT || entry->kind==FOLLOW_EXIT_CHOICE){
            if(v->destination==entry->destination && v->x==entry->x && v->y==entry->y)return 1;
        }else if(v->progress==entry->progress && v->mode==entry->mode && v->event==entry->event)return 1;
    }
    return 0;
}
typedef struct {unsigned x,y,parent,direction;NpeViewport view;} FollowupNode;
static int plan_route(void){
    NpeWorld *before=malloc(sizeof(*before));assert(before);*before=*live;
    const NpeFile *file=live->battle_file;NpeMap map;assert(file&&!npe_map(file,live->map_number-1,&map));
    unsigned cells=(unsigned)map.width*map.height;
    if(!cells || live->x>=map.width || live->y>=map.height){free(before);state("route-unavailable");return -1;}
    FollowupNode *nodes=malloc(cells*sizeof(*nodes));int *seen=malloc(cells*sizeof(*seen));
    NpeWorld *candidate=malloc(sizeof(*candidate));assert(nodes&&seen&&candidate);
    for(unsigned i=0;i<cells;++i)seen[i]=-1;
    nodes[0]=(FollowupNode){live->x,live->y,0,0,live->view};seen[live->y*map.width+live->x]=0;
    unsigned head=0,tail=1,found=0,last=0,direction=0;
    unsigned exit_found=0,exit_node=0,exit_direction=0,exit_move=0;
    Followup exit_entry={0};
    route_length=route_cursor=talk_sent=0;
    while(head<tail&&!found){FollowupNode node=nodes[head];
        for(unsigned d=0;d<4;++d){
            *candidate=*live;candidate->x=node.x;candidate->y=node.y;candidate->view=node.view;
            candidate->encounters_enabled=0;candidate->battle_file=NULL;candidate->defer_story_events=1;
            candidate->sound_observer=NULL;candidate->sound_range_observer=NULL;
            candidate->direction=d;unsigned npc_id;
            Followup entry={live->map_number,live->story_progress,FOLLOW_NPC,2,0,live->map_number,0,0};
            if(npe_world_facing_npc(file,candidate,&npc_id)==1){
                entry.event=npc_id;
                if(!already_visited(&entry)){found=1;last=head;direction=d;current=entry;
                    expected_move=NPE_MOVE_BLOCKED;expected_map=entry.destination;
                    expected_mode=(uint16_t)entry.mode;expected_event=(uint16_t)entry.event;break;}
            }
            int result=npe_world_step(file,candidate,d);
            if(result==NPE_MOVE_EVENT){
                entry=(Followup){live->map_number,live->story_progress,FOLLOW_EVENT,candidate->game_mode,
                    candidate->story_event_id,candidate->map_number,0,0};
                if(!already_visited(&entry)){found=1;last=head;direction=d;current=entry;
                    expected_move=NPE_MOVE_EVENT;expected_map=entry.destination;
                    expected_mode=(uint16_t)entry.mode;expected_event=(uint16_t)entry.event;break;}
            }
            if(result==NPE_MOVE_EXIT || result==NPE_MOVE_EXIT_CHOICE){
                entry=(Followup){live->map_number,live->story_progress,
                    result==NPE_MOVE_EXIT?FOLLOW_EXIT:FOLLOW_EXIT_CHOICE,candidate->game_mode,
                    candidate->story_event_id,candidate->map_number,candidate->x,candidate->y};
                if(!exit_found&&!already_visited(&entry)){exit_found=1;exit_node=head;exit_direction=d;
                    exit_entry=entry;exit_move=(unsigned)result;}
            }
            if(result!=NPE_MOVE_OK||candidate->map_number!=live->map_number||candidate->game_mode!=1)continue;
            unsigned index=candidate->y*map.width+candidate->x;assert(index<cells);
            if(seen[index]>=0)continue;
            assert(tail<cells);seen[index]=(int)tail;
            nodes[tail++]=(FollowupNode){candidate->x,candidate->y,head,d,candidate->view};
        }
        ++head;
    }
    if(!found&&exit_found){found=1;last=exit_node;direction=exit_direction;current=exit_entry;expected_move=exit_move;
        expected_map=current.destination;expected_mode=(uint16_t)current.mode;expected_event=(uint16_t)current.event;}
    if(found){route[route_length++]=(uint8_t)direction;
        while(last){assert(route_length<sizeof(route));route[route_length++]=(uint8_t)nodes[last].direction;last=nodes[last].parent;}
        for(unsigned i=0;i<route_length/2;++i){uint8_t swap=route[i];route[i]=route[route_length-1-i];route[route_length-1-i]=swap;}
    }
    fprintf(report,"{\"phase\":\"route\",\"checkpoint\":%u,\"found\":%s,\"length\":%u,\"kind\":%u,\"entry_map\":%u,\"entry_progress\":%u,\"expected_mode\":%u,\"expected_event\":%u,\"expected_map\":%u,\"expected_move\":%u,\"explored\":%u}\n",
        checkpoints,found?"true":"false",route_length,current.kind,current.map,current.progress,expected_mode,expected_event,expected_map,expected_move,tail);fflush(report);
    assert(!memcmp(before,live,sizeof(*live)));free(before);
    free(candidate);free(seen);free(nodes);return found?0:1;
}
static void replan(const char *reason){
    state(reason);assert(++replans<128);planned=reached=route_length=route_cursor=talk_sent=0;
}
static int step_display(const NpeFile *f,NpeWorld *w,unsigned d,uint8_t *pixels,NpeMapDisplayCommit commit,void *context){
    int result=npe_world_step_with_display(f,w,d,pixels,commit,context);assert(result>=0);
    if(result==NPE_MOVE_EVENT && w->game_mode==3)++encounters;
    if(current.kind!=FOLLOW_NPC && result==(int)expected_move && route_cursor==route_length &&
       w->map_number==expected_map && w->game_mode==expected_mode && w->story_event_id==expected_event){
        assert(planned);reached=1;state("trigger");
    }else if(result==NPE_MOVE_EVENT || result==NPE_MOVE_EXIT || result==NPE_MOVE_EXIT_CHOICE ||
             (result==NPE_MOVE_BLOCKED && (current.kind!=FOLLOW_NPC || route_cursor<route_length)))
        replan("route-interrupted");
    return result;
}
static int begin_talk(const NpeFile *file,NpeWorld *w){
    int result=npe_world_begin_talk(file,w);assert(result>=0);
    if(result==1){assert(planned&&current.kind==FOLLOW_NPC&&talk_sent);
        assert(w->game_mode==expected_mode&&w->story_event_id==expected_event);reached=1;state("trigger");}
    else replan("npc-moved");
    return result;
}
static int finish(SDL_Event *e,const char *reason){
    assert(!npe_rpg_runtime_failures(live->battle_file));
    assert(fingerprint==npe_game_fingerprint(live->battle_file));
    completed=1;state(reason);e->type=SDL_QUIT;return 1;
}
static int followup_wait(SDL_Event *e,int timeout,const char *caller){
    memset(e,0,sizeof(*e));
    if(strcmp(caller,"run")){
        if(timeout>=0){probe_clock+=(Uint32)timeout;return 0;}
        assert(++inputs<2400);e->type=SDL_KEYDOWN;
        e->key.keysym.sym=live && live->game_mode==3?SDLK_RETURN:SDLK_SPACE;return 1;
    }
    assert(live);e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;
    if(!started){assert(live->game_mode==7);started=1;++inputs;state("title-confirm");return 1;}
    assert(++inputs<2400);
    if(live->game_mode==7){state("defeat-or-title");e->type=SDL_QUIT;return 1;}
    if(live->game_mode==3){
        if(boost&&live->battle_menu_phase==10){
            /* User-authorized diagnostic character stats only. Preserve the
               loaded formation, targets, plot, conditions and actual actions. */
            for(unsigned i=0;i<3;++i)if(live->party_active[i]){
                live->hp[i]=live->max_hp[i]=live->qi[i]=live->max_qi[i]=30000;
                live->attack[i]=live->defense[i]=live->battle_party_defenses[i]=30000;
            }
            ++attribute_edits;state("diagnostic-attributes");
        }
        unsigned living=0;
        for(unsigned i=0;i<live->battle_enemy_count&&i<3;++i)living+=(int16_t)live->battle_enemy_hps[i]>0;
        /* Use the same visible menus as a player; F/D shortcuts no longer
           exist. Do not mutate menu or command state to select an action. */
        unsigned phase=live->battle_menu_phase;
        if(!live->story_event_id || !living){
            unsigned row=!live->story_event_id?3u:2u;
            e->key.keysym.sym=phase==31?
                (live->battle_menu_row==row?SDLK_RETURN:SDLK_DOWN):
                phase==30?SDLK_RETURN:phase==20?SDLK_DOWN:
                phase==10||phase==40?SDLK_LEFT:SDLK_F4;
        }else{
            e->key.keysym.sym=phase==10?SDLK_RETURN:phase==20?SDLK_RIGHT:
                phase==30||phase==40?SDLK_UP:SDLK_F4;
        }
        state("battle-input");return 1;
    }
    if(live->game_mode==4 || live->game_mode==6){e->key.keysym.sym=SDLK_F4;state("menu-return");return 1;}
    if(live->game_mode==17 || live->game_mode==8 || live->exit_choice){state("choice-input");return 1;}
    if(live->game_mode!=1){
        state("service-failure");
        if(live->game_mode==18){
            NpeMap map;NpeMapNpcMoveEvent event;
            assert(!npe_map(live->battle_file,live->map_number-1,&map));
            int found=npe_world_npc_move_event(live,live->story_event_id,&event);
            unsigned slot=20,x=0,y=0;
            if(found==1)for(unsigned i=0;i<20;++i){const uint8_t *npc;
                assert(!npe_world_npc(live,&map,i,&npc));
                if(((unsigned)npc[0]<<8|npc[1])==event.npc_id){
                    slot=i;npe_world_npc_position(live,npc,i,&x,&y);break;}
            }
            fprintf(report,"{\"phase\":\"npc-move-failure\",\"event_found\":%d,\"npc_id\":%u,\"slot\":%u,\"x\":%u,\"y\":%u,\"target_x\":%u,\"target_y\":%u,\"next_mode\":%u,\"next_event\":%u}\n",
                found,found==1?event.npc_id:0,slot,x,y,found==1?event.x:0,found==1?event.y:0,
                found==1?event.next_mode:0,found==1?event.next_id:0);fflush(report);
        }
        e->type=SDL_QUIT;return 1;
    }
    if(reached){
        assert(checkpoints<sizeof(visited)/sizeof(visited[0]));visited[checkpoints++]=current;state("followup");
        planned=reached=route_length=route_cursor=talk_sent=0;
    }
    if(checkpoints>=limit)return finish(e,"complete");
    if(!planned){if(!checkpoints&&!replans)state("new-game");
        int result=plan_route();if(result<0){e->type=SDL_QUIT;return 1;}
        if(result>0)return finish(e,"exhausted");
        planned=1;
    }
    if(route_cursor<route_length){static const SDL_Keycode keys[4]={SDLK_DOWN,SDLK_RIGHT,SDLK_UP,SDLK_LEFT};
        e->key.keysym.sym=keys[route[route_cursor++]];return 1;}
    if(current.kind==FOLLOW_NPC&&!talk_sent){
        unsigned npc_id; /* Facing was established by the last actual walking key. */
        if(npe_world_facing_npc(live->battle_file,live,&npc_id)!=1 || npc_id!=expected_event){replan("npc-moved");return followup_wait(e,timeout,caller);}
        talk_sent=1;e->key.keysym.sym=SDLK_SPACE;return 1;
    }
    replan("route-ended-without-trigger");return followup_wait(e,timeout,caller);
}
int main(int argc,char **argv){
    assert(argc>=3&&argc<=5);boost=argc==5&&atoi(argv[4]);if(argc>=4){limit=(unsigned)atoi(argv[3]);assert(limit&&limit<=64);}
    char path[1024];FILE *manifest=fopen(argv[1],"rb");
    assert(manifest&&fgets(path,sizeof(path),manifest)&&!fclose(manifest));path[strcspn(path,"\r\n")]=0;
    report=fopen(argv[2],"wb");assert(report);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(path,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    fingerprint=npe_game_fingerprint(&file);npe_free(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    char *args[]={"npe-sdl",path,NULL};int result=run(2,args);
    assert(!fclose(report));fprintf(stderr,"live followups result=%d completed=%u checkpoints=%u frames=%u inputs=%u encounters=%u replans=%u\n",
        result,completed,checkpoints,frames,inputs,encounters,replans);
    return result||!completed?1:0;
}
