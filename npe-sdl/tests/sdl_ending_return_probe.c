#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ending_return_native.h"
#ifdef main
#undef main
#endif
static NpeWorld *live;
static const EndingCase *current;
static int phase;
static unsigned submits,total,requests;
static uint64_t fingerprint;
static int probe_wait(SDL_Event *,int);
static void probe_present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static Uint32 probe_ticks(void){return 0;}
static Uint64 probe_seed(void){return 11;}
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/ending-return/slots/");}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
static int probe_open(NpeSoundPlayer *p,const NpeFile *f){memset(p,0,sizeof(*p));p->file=f;return 0;}
#define SDL_GetPrefPath probe_pref
#define SDL_GetTicks probe_ticks
#define SDL_GetPerformanceCounter probe_seed
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout probe_wait
#define SDL_RenderPresent probe_present
#define npe_sound_open probe_open
#define npe_sound_select probe_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef npe_sound_select
#undef npe_sound_open
#undef SDL_RenderPresent
#undef SDL_WaitEventTimeout
#undef SDL_CreateWindow
#undef SDL_GetPerformanceCounter
#undef SDL_GetTicks
#undef SDL_GetPrefPath
static void observe(void *context,unsigned section,int loop){
    (void)context;
    if(current->load){assert(0);}
    else{assert(phase==3&&requests<2);assert(section==(requests?20u:0u));assert(loop==(int)(requests!=0));}
    ++requests;
}
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){
    (void)p;live=w;w->sound_observer=observe;w->sound_range_observer=NULL;w->sound_context=NULL;
}
static uint16_t word(const NpeFile *f,uint32_t a){uint8_t b[2];assert(!npe_rpg_runtime_read(f,a,b,2));return (uint16_t)((unsigned)b[0]<<8|b[1]);}
static unsigned byte(const NpeFile *f,uint32_t a){uint8_t b;assert(!npe_rpg_runtime_read(f,a,&b,1));return b;}
static int allocated(const NpeFile *f,unsigned h){return !!(word(f,NPE_RESIDENT_HEAP_BITMAP+2u*(h/16))&(0x8000u>>(h%16)));}
static void seed(void){
    const EndingState *s=&current->before;NpeFile *f=(NpeFile *)live->battle_file;
    assert(f&&npe_rpg_runtime_owner(f)==live);
    live->game_mode=(uint16_t)s->mode;live->story_event_id=(uint16_t)s->event;live->map_init_pending=(uint8_t)s->init;
    live->music_enabled=(uint16_t)current->music;live->lcd_background=0;
    live->map_music_slot=0;live->sound_section=20;live->sound_loop=1;
    live->view=(NpeViewport){.screen_x=s->sx,.screen_y=s->sy,.offset_x=s->vx,.offset_y=s->vy};
    live->x=s->sx+s->vx;live->y=s->sy+s->vy;live->direction=(uint8_t)s->direction;
    assert(!npe_rpg_runtime_write(f,0x526ca2,current->npc,280));
    uint8_t view[210];for(unsigned i=0;i<105;++i){view[2*i]=(uint8_t)(current->viewport[i]>>8);view[2*i+1]=(uint8_t)current->viewport[i];}
    assert(!npe_rpg_runtime_write(f,0x5270b4,view,210));
    uint8_t slot=(uint8_t)s->titleSlot,selection=(uint8_t)s->titleSelection,stage=(uint8_t)s->titleStage;
    assert(!npe_rpg_runtime_write(f,0x527a7a,&slot,1)&&!npe_rpg_runtime_write(f,0x527a7c,&selection,1)&&!npe_rpg_runtime_write(f,0x527a7e,&stage,1));
    assert(word(f,0x527a6a)==s->mapHandle&&allocated(f,s->mapHandle));
    fingerprint=npe_game_fingerprint(f);
}
static void check_state(void){
    const EndingState *s=&current->phases[phase].after;const NpeFile *f=live->battle_file;
    fprintf(stderr,"ending state %s/%s mode%u/%u event%u/%u init%u/%u\n",current->kind,current->phases[phase].label,live->game_mode,s->mode,live->story_event_id,s->event,live->map_init_pending,s->init);
    assert(live->game_mode==s->mode&&live->story_event_id==s->event&&live->map_init_pending==s->init);
    assert(live->map_number==s->map&&live->story_progress==s->progress);
    assert(live->view.screen_x==s->sx&&live->view.screen_y==s->sy&&live->view.offset_x==s->vx&&live->view.offset_y==s->vy);
    assert(live->direction==s->direction&&byte(f,0x527a7a)==s->titleSlot&&byte(f,0x527a7c)==s->titleSelection&&byte(f,0x527a7e)==s->titleStage);
    assert(word(f,0x527a6a)==s->mapHandle&&word(f,0x527a88)==s->battleHandle&&word(f,0x527aa4)==s->battleFlags);
    assert(allocated(f,s->mapHandle)==(phase==3));
    assert(npe_rpg_runtime_map_current(f,live->map_number)==(phase==3));
    assert(!npe_rpg_runtime_failures(f)&&fingerprint==npe_game_fingerprint(f));
    assert(requests==(current->load?0u:(phase==3&&current->music?2u:0u)));
    if(current->load || phase<3)assert(live->sound_section==20&&live->sound_loop==1);
}
static void probe_present(SDL_Renderer *r){
    SDL_RenderPresent(r);if(phase<0)return;
    assert(submits<current->phases[phase].frames);
    char path[180];snprintf(path,sizeof(path),"tmp/ending-return/original/%u-%s-%s-%u.idx",current->music,current->kind,current->phases[phase].label,submits);
    uint8_t expected[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    FILE *in=fopen(path,"rb");assert(in&&fread(expected,1,sizeof(expected),in)==sizeof(expected)&&!fclose(in));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(expected);++i){unsigned v=expected[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    snprintf(path,sizeof(path),"tmp/ending-return/c-%u-%s-%s-%u.idx",current->music,current->kind,current->phases[phase].label,submits);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected)&&!fclose(out));
    fprintf(stderr,"ending music%u %s %s frame%u C%u SDL%u\n",current->music,current->kind,current->phases[phase].label,submits,d,sd);
    assert(!d&&!sd);++submits;++total;
}
static int probe_wait(SDL_Event *e,int timeout){
    (void)timeout;memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;
    if(phase<0){seed();phase=0;e->key.keysym.sym=SDLK_RETURN;return 1;}
    assert(submits==current->phases[phase].frames);check_state();submits=0;++phase;
    if(phase==4){e->type=SDL_QUIT;return 1;}
    static const SDL_Keycode keys[]={0,SDLK_DOWN,SDLK_UP,SDLK_RETURN};
    e->key.keysym.sym=current->load&&phase==2?SDLK_RETURN:keys[phase];return 1;
}
int main(int argc,char **argv){
    assert(argc==2&&!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(unsigned i=0;i<sizeof(ending_cases)/sizeof(*ending_cases);++i){
        current=&ending_cases[i];phase=-1;submits=requests=0;live=NULL;
        char *save_path=NULL;
        if(current->load){
            NpeFile f;NpeWorld saved;uint8_t *bytes;size_t size;char error[128];
            assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
            assert(!npe_world_new_game(&f,&saved));save_path=game_save_path(&f,0);
            assert(save_path&&!npe_world_save(&f,&saved,save_path));npe_free(&f);
        }
        char *args[]={argv[0],"--walk-preview","1,43,2",argv[1],NULL};assert(!run(4,args)&&phase==4);
        if(save_path){assert(!remove(save_path));free(save_path);}
    }
    assert(total==24);puts("Ending return:four original actual-main paths,24 C/SDL frames,title controls,new/occupied-slot map reallocation and audio requests passed");return 0;
}
