#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "title_slot_controls_native.h"
#ifdef main
#undef main
#endif
static NpeWorld *live;
static const NpeFile *running_file;
static const TitleSlotCase *current;
static unsigned phase,submits,total,requests;
static int active,fault_removed,fault_checked;
static const TitleSlotCase failure_case={"read-failure",2,title_slot_empty};
static const char *owned_path;
static int probe_wait(SDL_Event *,int);
static void probe_present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static void probe_close(NpeSoundPlayer *);
static Uint32 probe_ticks(void){return 0;}
static Uint64 probe_seed(void){return 11;}
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/title-slot-controls/slots/");}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
static int probe_open(NpeSoundPlayer *p,const NpeFile *f){memset(p,0,sizeof(*p));p->file=running_file=f;return 0;}
#define SDL_GetPrefPath probe_pref
#define SDL_GetTicks probe_ticks
#define SDL_GetPerformanceCounter probe_seed
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout probe_wait
#define SDL_RenderPresent probe_present
#define npe_sound_open probe_open
#define npe_sound_select probe_sound
#define npe_sound_close probe_close
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef npe_sound_select
#undef npe_sound_open
#undef npe_sound_close
#undef SDL_RenderPresent
#undef SDL_WaitEventTimeout
#undef SDL_CreateWindow
#undef SDL_GetPerformanceCounter
#undef SDL_GetTicks
#undef SDL_GetPrefPath
#undef SDL_PollEvent
#include "battle_input_keys.h"
#include "system_ignored_input.h"
static int input_prepared;
static unsigned byte(const NpeFile *f,uint32_t a){uint8_t b;assert(!npe_rpg_runtime_read(f,a,&b,1));return b;}
static void observe(void *context,unsigned section,int loop){
    (void)context;
    if(!active)return; /* Opening scene precedes this title-only reference. */
    if(current==&failure_case){assert(phase==2&&!requests&&!section&&!loop);++requests;return;}
    assert(active&&!strcmp(current->name,"empty")&&phase==3&&requests<2);
    assert(section==(requests?20u:0u)&&loop==(int)(requests!=0));++requests;
}
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){
    (void)p;live=w;w->sound_observer=observe;w->sound_range_observer=NULL;w->sound_context=NULL;
}
static void probe_close(NpeSoundPlayer *p){
    if(current==&failure_case){
        uint8_t exit_word[2];assert(!npe_rpg_runtime_read(running_file,0x527a3c,exit_word,2));
        assert(phase==2&&live->game_mode==1&&exit_word[0]==0&&exit_word[1]==1);
        assert(byte(running_file,0x527a7a)==1&&byte(running_file,0x527a7c)==2&&byte(running_file,0x527a7e)==2);
        assert(fault_removed&&requests==1&&!submits);fault_checked=1;
    }
    npe_sound_close(p);
}
static void state(void){
    const TitleSlotState *s=&current->phases[phase].after;const NpeFile *f=running_file;
    unsigned slot=byte(f,0x527a7a),selection=byte(f,0x527a7c),stage=byte(f,0x527a7e);
    fprintf(stderr,"title slots %s/%s controls %u,%u,%u original %u,%u,%u\n",
        current->name,current->phases[phase].label,slot,selection,stage,s->slot,s->selection,s->stage);
    assert(slot==s->slot&&selection==s->selection&&stage==s->stage);
    assert(live->game_mode==s->mode&&live->story_event_id==s->event&&live->map_init_pending==s->init);
    if(live->game_mode==1)assert(live->map_number==s->map&&live->story_progress==s->progress);
    assert(!npe_rpg_runtime_failures(f));
}
static void probe_present(SDL_Renderer *r){
    SDL_RenderPresent(r);if(!active)return;system_ignored_present();
    const TitleSlotPhase *p=&current->phases[phase];assert(submits<p->frames);
    char path[190];snprintf(path,sizeof(path),"tmp/title-slot-controls/original/%s-%s-%u.idx",current==&failure_case?"empty":current->name,p->label,submits);
    uint8_t expected[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    FILE *in=fopen(path,"rb");assert(in&&fread(expected,1,sizeof(expected),in)==sizeof(expected)&&!fclose(in));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(expected);++i){unsigned v=expected[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    snprintf(path,sizeof(path),"tmp/title-slot-controls/c-%s-%s-%u.idx",current->name,p->label,submits);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected)&&!fclose(out));
    fprintf(stderr,"title slots %s/%s frame%u C%u SDL%u\n",current->name,p->label,submits,d,sd);
    assert(!d&&!sd);++submits;++total;
}
static int probe_wait(SDL_Event *e,int timeout){
    (void)timeout;memset(e,0,sizeof(*e));
    if(!input_prepared){
    if(!active){active=1;phase=submits=0;assert(live->game_mode==7);}
    else{assert(submits==current->phases[phase].frames);state();submits=0;++phase;}
    input_prepared=1;}
    if(system_ignored_input(e,live,running_file))return 1;
    input_prepared=0;
    if(current==&failure_case && phase==2){
        /* Only our own file is removed, after its occupied level was read.
           This creates an actual read failure without replacing the loader. */
        assert(!remove(owned_path));fault_removed=1;e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;
    }
    if(phase==current->count){active=0;e->type=SDL_QUIT;return 1;}
    e->type=SDL_KEYDOWN;
    switch(current->phases[phase].mask){
    case 1:e->key.keysym.sym=SDLK_UP;break;
    case 2:e->key.keysym.sym=SDLK_DOWN;break;
    case 16:e->key.keysym.sym=SDLK_RETURN;break;
    case 64:e->key.keysym.sym=SDLK_F4;break;
    default:assert(0);
    }
    if(!strcmp(current->name,"empty")){
        /* Same original actions through its I/M/K aliases and keypad OK. */
        if(e->key.keysym.sym==SDLK_UP)e->key.keysym.sym=SDLK_i;
        else if(e->key.keysym.sym==SDLK_DOWN)e->key.keysym.sym=SDLK_m;
        else e->key.keysym.sym=phase==1?SDLK_KP_ENTER:SDLK_k;
    }
    if(getenv("NPE_PROBE_BATTLE_KEYS"))probe_battle_input(e,current->phases[phase].mask,SDLK_RETURN);
    return 1;
}
int main(int argc,char **argv){
    assert(argc==2&&!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    unsigned count=sizeof(title_slot_cases)/sizeof(*title_slot_cases);
    for(unsigned i=0;i<=count;++i){
        NpeFile f;NpeWorld saved;uint8_t *bytes;size_t size;char error[128];
        assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
        assert(!npe_world_new_game(&f,&saved));char *path=game_save_path(&f,0);
        assert(path&&!npe_world_save(&f,&saved,path));
        current=i==count?&failure_case:&title_slot_cases[i];
        char *empty_path=NULL;
        if(!strcmp(current->name,"empty")){
            empty_path=game_save_path(&f,2);assert(empty_path);
            FILE *out=fopen(empty_path,"wb");assert(out&&fwrite("invalid",1,7,out)==7&&!fclose(out));
        }
        npe_free(&f);owned_path=path;fault_removed=fault_checked=0;
        system_ignored_reset();input_prepared=0;active=0;submits=requests=0;live=NULL;
        char *args[]={argv[0],argv[1],NULL};assert(!run(2,args)&&phase==current->count);
        assert(requests==(current==&failure_case?1u:!strcmp(current->name,"empty")?2u:0u));
        if(current==&failure_case)assert(fault_checked);
        if(!fault_removed)assert(!remove(path));
        free(path);
        if(empty_path){assert(!remove(empty_path));free(empty_path);}
    }
    system_ignored_report();
    assert(total==24);
    printf("Title slot controls:two ordinary original paths,%u C/SDL comparisons,wrap,cancel,reopen,empty-slot new game and occupied read failure passed\n",total);
    return 0;
}
