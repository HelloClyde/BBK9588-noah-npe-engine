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
typedef struct {unsigned id,x,y,direction;} SlotNpc;
typedef struct {unsigned mode,event,map,progress,sx,sy,vx,vy,direction,money;SlotNpc npc[20];} SlotState;
typedef struct {unsigned mask,commits;SlotState after;} SlotPhase;
typedef struct {const char *name;SlotState before;unsigned phases;SlotPhase phase[16];} SlotCase;
#include "system_slot_return_native.h"
static NpeWorld *live;
static unsigned case_index,phase,submits,total,stage,active;
static Uint32 fake_clock;
static int probe_wait(SDL_Event *,int);
static void probe_present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/system-slot-return-");}
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *event){(void)event;return 0;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetPrefPath probe_pref
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout probe_wait
#define SDL_RenderPresent probe_present
#define npe_sound_select probe_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetPrefPath
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef npe_sound_select
#include "battle_input_keys.h"
#include "system_ignored_input.h"
static int input_prepared;
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);}
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[160];
    if(before)snprintf(path,sizeof(path),"tmp/native-system-slot-%s-before.idx",slot_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-system-slot-%s-%u-commit-%u.idx",slot_cases[case_index].name,phase,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void configure(NpeWorld *w,const SlotState *s){
    assert(w->map_number==s->map);
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->story_progress=(uint16_t)s->progress;
    w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
    w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=(uint8_t)s->direction;w->money=s->money;
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint8_t)s->npc[i].x;
        w->npc_override_y[i]=(uint8_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
}
static void check_state(const SlotState *s){
    assert(live->game_mode==s->mode&&live->story_event_id==s->event&&live->map_number==s->map&&live->story_progress==s->progress&&
           live->view.screen_x==s->sx&&live->view.screen_y==s->sy&&live->view.offset_x==s->vx&&live->view.offset_y==s->vy&&
           live->direction==s->direction&&live->money==s->money&&live->x==s->sx+s->vx&&live->y==s->sy+s->vy);
    NpeMap map;assert(!npe_map((const NpeFile *)live->battle_file,live->map_number-1,&map));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){const uint8_t *n;unsigned x,y;
        assert(!npe_map_npc(&map,i,&n));npe_world_npc_position(live,n,i,&x,&y);
        unsigned direction=live->npc_override_direction[i]!=255?live->npc_override_direction[i]:n[6];
        assert((unsigned)n[0]*256+n[1]==s->npc[i].id&&x==s->npc[i].x&&y==s->npc[i].y&&direction==s->npc[i].direction);}
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;system_ignored_present();
    const SlotCase *c=&slot_cases[case_index];assert(phase<c->phases&&submits<c->phase[phase].commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"system slot %s phase%u commit%u C=%u SDL=%u\n",c->name,phase,submits,d,sd);
    if(d){FILE *out=fopen("tmp/system-slot-return-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++submits;++total;
}
static int probe_wait(SDL_Event *event,int timeout){(void)timeout;memset(event,0,sizeof(*event));
    if(!stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;stage=1;return 1;}
    const SlotCase *c=&slot_cases[case_index];
    if(!input_prepared){
    if(stage==1){configure(live,&c->before);read_frame(1,0,last_pixel_frame);
        retain_world_frame((const NpeFile *)live->battle_file,live,last_pixel_renderer,last_pixel_frame,0);
        fake_clock=100;phase=submits=0;active=1;stage=2;}
    else{assert(submits==c->phase[phase].commits);check_state(&c->phase[phase].after);submits=0;++phase;}
    input_prepared=1;}
    if(system_ignored_input(event,live,(const NpeFile *)live->battle_file))return 1;
    input_prepared=0;
    if(phase==c->phases){active=0;stage=3;event->type=SDL_QUIT;return 1;}
    probe_battle_input(event,c->phase[phase].mask,SDLK_RETURN);return 1;
}
int main(int argc,char **argv){NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    char *paths[3];for(unsigned i=0;i<3;++i){paths[i]=game_save_path(&file,i);assert(paths[i]);remove(paths[i]);}
    for(case_index=0;case_index<sizeof(slot_cases)/sizeof(*slot_cases);++case_index){NpeWorld saved;
        assert(!npe_world_new_game(&file,&saved));configure(&saved,&slot_cases[case_index].before);
        assert(!npe_world_save(&file,&saved,paths[0]));
        system_ignored_reset();input_prepared=0;stage=active=phase=submits=0;fake_clock=0;world_map_frame_valid=world_map_saved_edges_valid=0;
        char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==3);
        fprintf(stderr,"System slot %s complete; retained/reloaded NPC states matched\n",slot_cases[case_index].name);}
    system_ignored_report();
    for(unsigned i=0;i<3;++i){remove(paths[i]);free(paths[i]);}npe_free(&file);
    printf("System slot return:%u original C/SDL commits and player/NPC states passed\n",total);return 0;
}
