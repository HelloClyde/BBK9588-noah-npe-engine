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
typedef struct {unsigned id,x,y,direction;} MenuNpc;
typedef struct {unsigned mode,event,map,progress,sx,sy,vx,vy,direction,money,enabled,slot;MenuNpc npc[20];} MenuState;
typedef struct {unsigned mask,commits;MenuState after;} MenuPhase;
typedef struct {const char *name;MenuState before;uint32_t rng_before,rng_after;unsigned phases;MenuPhase phase[16];} MenuCase;
#include "system_menu_return_native.h"
static NpeWorld *live;
static unsigned case_index,phase,submits,total,stage,active,setup_index;
static Uint32 fake_clock;
static int probe_wait(SDL_Event *,int);
static int probe_popup_wait(SDL_Event *);
static void probe_present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/system-menu-return-");}
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *e){if(active)return 0;memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetPrefPath probe_pref
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout probe_wait
#define SDL_WaitEvent probe_popup_wait
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
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#undef npe_sound_select
#include "battle_input_keys.h"
#include "system_ignored_input.h"
static int input_prepared;
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);}
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[160];
    if(before)snprintf(path,sizeof(path),"tmp/native-system-menu-%s-before.idx",menu_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-system-menu-%s-%u-commit-%u.idx",menu_cases[case_index].name,phase,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void configure(NpeWorld *w,const MenuState *s){
    assert(w->map_number==s->map&&w->music_enabled==s->enabled);
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->story_progress=(uint16_t)s->progress;
    w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
    w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=(uint8_t)s->direction;w->money=s->money;
    w->map_music_slot=(uint8_t)s->slot;npe_world_seed(w,menu_cases[case_index].rng_before);
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint8_t)s->npc[i].x;
        w->npc_override_y[i]=(uint8_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
}
static void check_state(const MenuState *s){
    assert(live->game_mode==s->mode&&live->story_event_id==s->event&&live->map_number==s->map&&live->story_progress==s->progress&&
           live->view.screen_x==s->sx&&live->view.screen_y==s->sy&&live->view.offset_x==s->vx&&live->view.offset_y==s->vy&&
           live->direction==s->direction&&live->money==s->money&&live->x==s->sx+s->vx&&live->y==s->sy+s->vy&&
           live->music_enabled==s->enabled&&live->map_music_slot==s->slot);
    const MenuCase *c=&menu_cases[case_index];assert(live->random_state==(phase==c->phases-1?c->rng_after:c->rng_before));
    NpeMap map;assert(!npe_map((const NpeFile *)live->battle_file,live->map_number-1,&map));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){const uint8_t *n;unsigned x,y;assert(!npe_map_npc(&map,i,&n));npe_world_npc_position(live,n,i,&x,&y);
        unsigned direction=live->npc_override_direction[i]!=255?live->npc_override_direction[i]:n[6];
        assert((unsigned)n[0]*256+n[1]==s->npc[i].id&&x==s->npc[i].x&&y==s->npc[i].y&&direction==s->npc[i].direction);}
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;system_ignored_present();
    const MenuCase *c=&menu_cases[case_index];assert(phase<c->phases);
    if(submits>=c->phase[phase].commits){fprintf(stderr,"Extra C/SDL submit: %s phase%u mask%u native=%u\n",c->name,phase,c->phase[phase].mask,c->phase[phase].commits);assert(0);}
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"system menu %s phase%u commit%u C=%u SDL=%u\n",c->name,phase,submits,d,sd);
    if(d){FILE *out=fopen("tmp/system-menu-return-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++submits;++total;
    /* Inject only an observed pending periodic deadline, not a physical
       tick conversion. Native snapshots include four return+NPC cases. */
    if(phase==c->phases-1&&submits==1&&c->phase[phase].commits==2)fake_clock=100000;
}
static unsigned ignored_index,ignored_checked;
static int prepare_event(SDL_Event *event){memset(event,0,sizeof(*event));
    if(!stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;stage=1;return 1;}
    const MenuCase *c=&menu_cases[case_index];
    if(!input_prepared){
    if(stage==1){
        static const unsigned off_keys[]={64,2,2,2,16,2,2,16,16};
        if(!c->before.enabled&&setup_index<sizeof(off_keys)/sizeof(*off_keys)){
            probe_battle_input(event,off_keys[setup_index++],SDLK_RETURN);return 1;}
        configure(live,&c->before);read_frame(1,0,last_pixel_frame);
        retain_world_frame((const NpeFile *)live->battle_file,live,last_pixel_renderer,last_pixel_frame,0);
        fake_clock=100;phase=submits=0;active=1;stage=2;
    }else{
        if(submits<c->phase[phase].commits){assert(c->phase[phase].commits==2&&submits==1);return 0;}
        assert(submits==c->phase[phase].commits);check_state(&c->phase[phase].after);
        if(!submits)fprintf(stderr,"Original ignored input: %s phase%u mask%u\n",c->name,phase,c->phase[phase].mask);
        if(!phase && getenv("NPE_PROBE_IGNORED_SHORTCUTS") && ignored_index<2){
            assert(live->game_mode==6 && !live->property_page);
            event->type=SDL_KEYDOWN;event->key.keysym.sym=ignored_index?SDLK_e:SDLK_u;
            ++ignored_index;++ignored_checked;return 1;
        }
        submits=0;++phase;
    }
    input_prepared=1;}
    if(system_ignored_input(event,live,(const NpeFile *)live->battle_file))return 1;
    input_prepared=0;
    if(phase==c->phases){active=0;stage=3;event->type=SDL_QUIT;return 1;}
    probe_battle_input(event,c->phase[phase].mask,SDLK_RETURN);return 1;
}
static int probe_wait(SDL_Event *e,int timeout){(void)timeout;return prepare_event(e);}
static int probe_popup_wait(SDL_Event *e){assert(live->game_mode==6);return prepare_event(e);}
int main(int argc,char **argv){NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    char *paths[3];for(unsigned i=0;i<3;++i){paths[i]=game_save_path(&file,i);assert(paths[i]);remove(paths[i]);}
    NpeWorld saved;assert(!npe_world_new_game(&file,&saved)&&!npe_world_save(&file,&saved,paths[0]));
    for(case_index=0;case_index<sizeof(menu_cases)/sizeof(*menu_cases);++case_index){
        system_ignored_reset();input_prepared=0;stage=active=phase=submits=setup_index=0;ignored_index=0;fake_clock=0;
        char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==3);
        fprintf(stderr,"System menu %s complete; player/NPC/settings/shared RNG matched\n",menu_cases[case_index].name);}
    system_ignored_report();
    for(unsigned i=0;i<3;++i){remove(paths[i]);free(paths[i]);}npe_free(&file);
    printf("System menu return:%u original C/SDL commits, ignored keys, player/NPC/settings/shared RNG passed\n",total);
    if(getenv("NPE_PROBE_IGNORED_SHORTCUTS"))printf("System root U/E ignored: %u actual inputs without LCD/state changes\n",ignored_checked);
    return 0;
}
