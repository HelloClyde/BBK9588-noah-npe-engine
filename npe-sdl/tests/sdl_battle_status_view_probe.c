#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "../text.h"
#ifdef main
#undef main
#endif
#ifdef NPE_STATUS_RESOURCE_CYCLE
typedef struct {unsigned key,frames,phase,member,row,flags;} StatusStep;
typedef struct {const char *name;unsigned party,actor,member;uint32_t money;uint16_t flags;
    uint8_t entry[9][148],input[9][148];StatusStep steps[12];} StatusCase;
#include "battle_status_resources_native.h"
#define STATUS_PREFIX "tmp/battle-status-resources"
#define STATUS_NATIVE "tmp/native-battle-status-resources"
#else
typedef struct {unsigned key,frames,phase,member,row;} StatusStep;
typedef struct {const char *name;unsigned party,actor,member;uint32_t money;
    uint8_t entry[9][148],input[9][148];StatusStep steps[12];} StatusCase;
#include "battle_status_view_native.h"
#define STATUS_PREFIX "tmp/battle-status-view"
#define STATUS_NATIVE "tmp/native-battle-status-view"
#endif
static unsigned case_index,step_index,frame_index,total_frames,total_states,setup_step,idle_wait;
static int active;
static const char *route;
static NpeWorld *live;
static const NpeFile *resource_file;
static int status_new_game(const NpeFile *,NpeWorld *);
static int status_wait(SDL_Event *,int);
static void status_present(SDL_Renderer *);
static SDL_Window *status_window(const char *,int,int,int,int,Uint32);
#define npe_world_new_game status_new_game
#define npe_world_restart_game status_new_game
#define SDL_WaitEventTimeout status_wait
#define SDL_RenderPresent status_present
#define SDL_CreateWindow status_window
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef npe_world_new_game
#undef npe_world_restart_game

static void set_records(NpeWorld *w,const uint8_t records[9][148]){
    for(unsigned i=0;i<9;++i)assert(!npe_world_record_write(w,i>=3,i>=3?i-3:i,0,records[i],148));
}
static SDL_Window *status_window(const char *title,int x,int y,int width,int height,Uint32 flags){
    (void)width;(void)height;
    /* SDL_RenderReadPixels(NULL) reads physical output pixels. Match the
       original240x120 LCD rather than the app's normal960x480 window. */
    return SDL_CreateWindow(title,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
static int status_new_game(const NpeFile *f,NpeWorld *w){
    resource_file=f;
    const StatusCase *c=&status_cases[case_index];
    assert(!npe_world_init(f,w,9,29,13));
    for(unsigned i=0;i<3;++i){w->party_active[i]=(uint8_t)(i<c->party);w->party_ids[i]=1;
        w->party_characters[i]=w->party_characters[0];memcpy(w->party_names[i],w->party_names[0],12);}
    w->game_mode=3;w->story_event_id=1;
    assert(npe_world_prepare_scripted_battle(f,w)==1);
    set_records(w,c->entry);w->money=c->money;w->music_enabled=0;
    live=w;return 0;
}
static void input_records(NpeWorld *w){
    const StatusCase *c=&status_cases[case_index];set_records(w,c->input);
    w->battle_actor=(uint8_t)c->actor;
    uint8_t member=(uint8_t)c->member;
    assert(!npe_world_resident_write(w,0x527aac,&member,1));
#ifdef NPE_STATUS_RESOURCE_CYCLE
    uint8_t flags[]={(uint8_t)(c->flags>>8),(uint8_t)c->flags};
    assert(!npe_rpg_runtime_write(resource_file,0x527aa4,flags,2));
#endif
}
static void check_state(const NpeWorld *w,unsigned step){
    const StatusCase *c=&status_cases[case_index];const StatusStep *s=&c->steps[step];
    assert(w->game_mode==3 && w->battle_actor==c->actor && w->battle_menu_phase==s->phase &&
        w->battle_status_member==s->member && w->battle_menu_row==s->row);
    uint8_t member;assert(!npe_world_resident_read(w,0x527aac,&member,1)&&member==s->member);
    char path[200];snprintf(path,sizeof(path),STATUS_PREFIX "-c-%s-%s-state-%u.bin",route,c->name,step);
    FILE *out=fopen(path,"wb");assert(out);
    uint8_t controls[]={(uint8_t)w->battle_menu_phase,w->battle_actor,member,w->battle_menu_row};
    assert(fwrite(controls,1,4,out)==4);
#ifdef NPE_STATUS_RESOURCE_CYCLE
    assert(w->battle_resource_flags==s->flags);
    uint8_t flags[2];assert(!npe_rpg_runtime_read(resource_file,0x527aa4,flags,2));
    assert(((unsigned)flags[0]*256+flags[1])==s->flags&&fwrite(flags,1,2,out)==2);
    assert(!npe_rpg_runtime_failures(resource_file));
#endif
    for(unsigned i=0;i<9;++i){uint8_t r[148];
        assert(!(i<3?npe_world_map_record_image(w,i,r):npe_world_battle_record_image(w,i-3,r)));
        unsigned diff=0;for(unsigned j=0;j<148;++j)if(r[j]!=c->input[i][j]){
            if(diff++<5)fprintf(stderr,"%s record%u byte%u %02x/%02x\n",c->name,i,j,r[j],c->input[i][j]);}
        assert(!diff && fwrite(r,1,148,out)==148);
    }
    assert(!fclose(out));++total_states;
}
static void status_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!active)return;
    const StatusCase *c=&status_cases[case_index];
    assert(step_index<12 && frame_index<c->steps[step_index].frames);
    uint8_t expected[28800];uint32_t rgba[28800];char path[200];
    snprintf(path,sizeof(path),STATUS_NATIVE "-%s-%u-%u.idx",c->name,step_index,frame_index);
    FILE *in=fopen(path,"rb");assert(in&&fread(expected,1,28800,in)==28800&&fgetc(in)==EOF&&!fclose(in));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned diff=0,sd=0;for(unsigned i=0;i<28800;++i){unsigned v=expected[i];diff+=v!=last_pixel_frame[i];
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s %s step%u frame%u C=%u SDL=%u\n",route,c->name,step_index,frame_index,diff,sd);
    snprintf(path,sizeof(path),STATUS_PREFIX "-c-%s-%s-%u-%u.idx",route,c->name,step_index,frame_index);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,28800,out)==28800&&!fclose(out));
    assert(!diff&&!sd);++frame_index;++total_frames;
}
static SDL_Keycode keycode(unsigned key){
    return key==1?SDLK_UP:key==2?SDLK_DOWN:key==4?SDLK_LEFT:key==8?SDLK_RIGHT:key==16?SDLK_RETURN:SDLK_F4;
}
static int status_wait(SDL_Event *event,int timeout){
    (void)timeout;memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;
    if(!live){event->key.keysym.sym=SDLK_RETURN;return 1;}
    if(!setup_step)input_records(live);
    static const unsigned setup[]={4,2,16,1};
    if(setup_step<4){event->key.keysym.sym=keycode(setup[setup_step++]);return 1;}
    /* Exercise an outer-loop observation timeout while the status page
       is visible. It must retain the LCD without another submission. */
    if(active && live->battle_menu_phase==35 && !idle_wait){idle_wait=1;return 0;}
    idle_wait=0;
    if(active){assert(frame_index==status_cases[case_index].steps[step_index].frames);
        check_state(live,step_index++);frame_index=0;}
    if(step_index==12){active=0;event->type=SDL_QUIT;return 1;}
    active=1;event->key.keysym.sym=keycode(status_cases[case_index].steps[step_index].key);return 1;
}
int main(int argc,char **argv){
    assert(argc==2 && !SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    assert(!SDL_Init(SDL_INIT_VIDEO));
    NpeFile f;NpeFont font={0};uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *window=SDL_CreateWindow("battle status",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(case_index=0;case_index<4;++case_index){
        NpeWorld w;live=NULL;assert(!status_new_game(&f,&w));
#ifdef NPE_STATUS_RESOURCE_CYCLE
        assert(!npe_world_resources_open(&f,&w));
#endif
        BattleAnimationDisplay d={0};d.renderer=renderer;d.texture=texture;d.font=&font;
        w.battle_animation_observer=play_battle_animation;w.battle_animation_context=&d;
        assert(!draw_battle_entry(renderer,texture,&f,&w,&font));input_records(&w);
        static const unsigned setup[]={4,2,16,1};
        for(unsigned i=0;i<4;++i){assert(battle_menu_navigation(&w,setup[i])==1);
            assert(!draw_battle_scene(renderer,texture,&f,w.battle_enemy_id,&w,&font,
                (unsigned)battle_menu_icon(&w),battle_menu_level(&w),w.battle_menu_row,w.battle_item_row));}
        route="direct";
        for(step_index=0;step_index<12;++step_index){active=1;frame_index=0;
            assert(battle_status_input(renderer,texture,&f,&font,&w,status_cases[case_index].steps[step_index].key)==1);
            active=0;assert(frame_index==status_cases[case_index].steps[step_index].frames);check_state(&w,step_index);}
#ifdef NPE_STATUS_RESOURCE_CYCLE
        npe_rpg_runtime_close(&f);
#endif
    }
    live=NULL;npe_font_free(&font);npe_free(&f);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    char *arguments[]={"npe-sdl",argv[1],NULL};route="main";
    for(case_index=0;case_index<4;++case_index){active=0;live=NULL;setup_step=step_index=frame_index=0;
        assert(run(2,arguments)==0 && step_index==12);live=NULL;}
    printf("battle status: four paths, %u LCD commits and %u complete record/control states, direct and actual main passed\n",total_frames,total_states);
    return 0;
}
