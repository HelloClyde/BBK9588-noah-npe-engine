#include "../npe.h"
#include "../sound_sdl.h"
#include "../text.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned id,x,y,direction;} ShopReturnNpc;
typedef struct {unsigned mode,event,context,map,progress,sx,sy,vx,vy,direction,money,enabled,slot,ui,selected,first,last,row,quantity,choice,descStart,descEnd;uint32_t rng;uint16_t stocks[255];ShopReturnNpc npc[20];} ShopReturnState;
typedef struct {unsigned mask,period,commits;ShopReturnState after;} ShopReturnPhase;
typedef struct {const char *name;unsigned sell;ShopReturnState before;unsigned phases;ShopReturnPhase phase[8];} ShopReturnCase;
#include "shop_return_periodic_native.h"
static NpeWorld *live;
static unsigned case_index,phase,submits,total,stage,active;
static Uint32 fake_clock;
static SDL_Texture *app_texture;
static NpeFont probe_font;
static int probe_wait(SDL_Event *,int);
static int probe_popup_wait(SDL_Event *);
static void probe_present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/shop-return-periodic-");}
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *e){if(active)return 0;memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
static SDL_Texture *probe_texture(SDL_Renderer *r,Uint32 f,int a,int w,int h){app_texture=SDL_CreateTexture(r,f,a,w,h);return app_texture;}
#define SDL_GetPrefPath probe_pref
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_CreateWindow probe_window
#define SDL_CreateTexture probe_texture
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
#undef SDL_CreateTexture
#undef SDL_WaitEventTimeout
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#undef npe_sound_select
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);}
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[160];
    if(before)snprintf(path,sizeof(path),"tmp/native-shop-return-%s-before.idx",shop_return_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-shop-return-%s-%u-commit-%u.idx",shop_return_cases[case_index].name,phase,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void configure(NpeWorld *w,const ShopReturnState *s){
    assert(w->map_number==s->map&&w->music_enabled==s->enabled);
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->story_progress=(uint16_t)s->progress;w->event_context_id=(uint16_t)s->context;
    w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
    w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=(uint8_t)s->direction;w->money=s->money;
    w->map_music_slot=(uint8_t)s->slot;npe_world_seed(w,s->rng);memcpy(w->condition_words,s->stocks,sizeof(s->stocks));
    w->shop_ui=(uint8_t)s->ui;w->shop_selected_item=(uint16_t)s->selected;w->shop_first_item=(uint16_t)s->first;w->shop_last_item=(uint16_t)s->last;
    w->shop_row=(uint8_t)s->row;w->shop_quantity=(uint8_t)s->quantity;w->shop_choice=(uint8_t)s->choice;
    w->shop_description_start=(uint16_t)s->descStart;w->shop_description_end=(uint16_t)s->descEnd;
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint8_t)s->npc[i].x;w->npc_override_y[i]=(uint8_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
}
static void check_state(const ShopReturnState *s){
    fprintf(stderr,"Shop return %s phase%u state mode%u ui%u selected%u quantity%u choice%u RNG%u\n",shop_return_cases[case_index].name,phase,live->game_mode,live->shop_ui,live->shop_selected_item,live->shop_quantity,live->shop_choice,live->random_state);
    assert(live->game_mode==s->mode&&live->story_event_id==s->event&&live->event_context_id==s->context&&live->map_number==s->map&&live->story_progress==s->progress&&
           live->view.screen_x==s->sx&&live->view.screen_y==s->sy&&live->view.offset_x==s->vx&&live->view.offset_y==s->vy&&
           live->direction==s->direction&&live->money==s->money&&live->music_enabled==s->enabled&&live->map_music_slot==s->slot&&live->random_state==s->rng);
    assert(live->shop_ui==s->ui&&live->shop_selected_item==s->selected&&live->shop_first_item==s->first&&live->shop_last_item==s->last&&live->shop_row==s->row&&
           live->shop_quantity==s->quantity&&live->shop_choice==s->choice&&live->shop_description_start==s->descStart&&live->shop_description_end==s->descEnd);
    assert(!memcmp(live->condition_words,s->stocks,sizeof(s->stocks)));
    NpeMap map;assert(!npe_map((const NpeFile *)live->battle_file,live->map_number-1,&map));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){const uint8_t *n;unsigned x,y;assert(!npe_map_npc(&map,i,&n));npe_world_npc_position(live,n,i,&x,&y);
        unsigned direction=live->npc_override_direction[i]!=255?live->npc_override_direction[i]:n[6];
        assert((unsigned)n[0]*256+n[1]==s->npc[i].id&&x==s->npc[i].x&&y==s->npc[i].y&&direction==s->npc[i].direction);}
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const ShopReturnCase *c=&shop_return_cases[case_index];assert(phase<c->phases&&submits<c->phase[phase].commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"shop return %s phase%u commit%u C=%u SDL=%u\n",c->name,phase,submits,d,sd);
    if(d){FILE *out=fopen("tmp/shop-return-periodic-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++submits;++total;
}
static int prepare_event(SDL_Event *event){memset(event,0,sizeof(*event));
    if(!stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;stage=1;return 1;}
    const ShopReturnCase *c=&shop_return_cases[case_index];
    if(stage==1){
        configure(live,&c->before);read_frame(1,0,last_pixel_frame);
        const NpeFile *file=(const NpeFile *)live->battle_file;retain_world_frame(file,live,last_pixel_renderer,last_pixel_frame,0);
        live->game_mode=4;live->story_event_id=1;live->event_context_id=c->sell?10:13;live->shop_kind=live->shop_ready=c->sell?2:1;live->shop_list_ready=0;
        assert(!npe_world_shop_begin(file,live));
        fake_clock=100;phase=submits=0;active=1;stage=2;unsigned levels[3]={0};
        assert(!draw_world(last_pixel_renderer,app_texture,file,live,0,0,0,0,0,0,levels,0,&probe_font));
    }
    if(submits<c->phase[phase].commits){assert(c->phase[phase].period);return 0;}
    assert(submits==c->phase[phase].commits);check_state(&c->phase[phase].after);
    submits=0;++phase;
    if(phase==c->phases){active=0;stage=3;event->type=SDL_QUIT;return 1;}
    if(c->phase[phase].period){fake_clock+=5000;return 0;}
    event->type=SDL_KEYDOWN;event->key.keysym.sym=c->phase[phase].mask==64?SDLK_F4:SDLK_RETURN;return 1;
}
static int probe_wait(SDL_Event *e,int timeout){if(active&&shop_return_cases[case_index].phase[phase].period&&submits==0){fake_clock+=(Uint32)(timeout>0?timeout:1);return 0;}return prepare_event(e);}
static int probe_popup_wait(SDL_Event *e){assert(live->game_mode==4);return prepare_event(e);}
int main(int argc,char **argv){assert(argc==2);assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    assert(!npe_font_load(&probe_font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&probe_font,"npe-sdl/assets/gbk16-native.bin"));
    for(case_index=0;case_index<sizeof(shop_return_cases)/sizeof(*shop_return_cases);++case_index){
        stage=active=phase=submits=0;fake_clock=0;char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==3);
        fprintf(stderr,"Shop return %s complete; player/NPC/shop/shared RNG matched\n",shop_return_cases[case_index].name);}
    npe_font_free(&probe_font);printf("Shop return:%u original C/SDL commits passed\n",total);return 0;
}
