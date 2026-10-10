#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {const char *name;unsigned event,count,wrong,keys,frames,correct_seed,wrong_seed,fixed_seed,key[48],frame[96][2],commits[48],state[7];const char *input;unsigned stats[22],initial_stats[22];} SdlQuizCase;
#include "sdl_quiz_native.h"
static unsigned case_index,phase,submitted,total,stage_submits[48];
static Uint32 fake_clock;
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 ms){fake_clock+=ms;}
static int poll(SDL_Event *e){(void)e;return 0;}
static int push(SDL_Event *e){(void)e;return 1;}
static int wait_event(SDL_Event *,int);
static void present(SDL_Renderer *);
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent poll
#define SDL_PushEvent push
#define SDL_WaitEventTimeout wait_event
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_PushEvent
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
static int wait_event(SDL_Event *e,int ms) {
    const SdlQuizCase *c=&sdl_quiz_cases[case_index];
    if(!strcmp(c->name,"category-timeout") && submitted<c->frames) {
        phase=1;fake_clock+=(Uint32)ms;return 0;
    }
    assert(stage_submits[phase]==c->commits[phase]);
    memset(e,0,sizeof(*e));if(phase>=c->keys){e->type=SDL_QUIT;return 1;}
    unsigned key=c->key[phase++];e->type=SDL_KEYDOWN;
    switch(key) {
    case 0x80:e->key.keysym.sym=SDLK_LEFT;break;
    case 0x81:e->key.keysym.sym=SDLK_RIGHT;break;
    case 0x82:e->key.keysym.sym=SDLK_UP;break;
    case 0x83:e->key.keysym.sym=SDLK_DOWN;break;
    case 0x86:e->key.keysym.sym=SDLK_RETURN;break;
    case 0x89:e->key.keysym.sym=SDLK_BACKSPACE;break;
    case 0x8e:e->key.keysym.sym=SDLK_F4;break;
    default:e->key.keysym.sym=(SDL_Keycode)key;
        if(key>='A'&&key<='Z'){e->key.keysym.sym=(SDL_Keycode)(key-'A'+'a');e->key.keysym.mod=KMOD_SHIFT;}
        break;
    }
    return 1;
}
static void present(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);const SdlQuizCase *c=&sdl_quiz_cases[case_index];
    assert(submitted<c->frames);char path[200];
    const unsigned *frame=c->frame[submitted];
    snprintf(path,sizeof(path),"tmp/native-quiz-%s-%u-%u.idx",c->name,frame[0],frame[1]);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned diff=0,sdl_diff=0;
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];diff+=last_pixel_frame[i]!=v;
        sdl_diff+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL quiz %s frame%u: C=%u SDL=%u\n",c->name,submitted,diff,sdl_diff);
    if(diff){f=fopen("tmp/c-sdl-quiz-current.idx","wb");assert(f&&fwrite(last_pixel_frame,1,sizeof(native),f)==sizeof(native)&&!fclose(f));}
    assert(!diff&&!sdl_diff);++stage_submits[phase];++submitted;++total;
}
int main(int argc,char **argv) {
    assert(argc==2&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("quiz service probe",0,0,NPE_WIDTH,NPE_HEIGHT,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    uint8_t *bytes;size_t size;NpeFile file;char error[128];NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    for(case_index=0;case_index<sizeof(sdl_quiz_cases)/sizeof(sdl_quiz_cases[0]);++case_index) {
        const SdlQuizCase *c=&sdl_quiz_cases[case_index];NpeWorld world;
        assert(!npe_world_new_game(&file,&world));npe_world_seed(&world,11);
        world.game_mode=8;world.story_event_id=(uint16_t)c->event;
        world.quiz_answer_count=(uint16_t)c->count;world.quiz_wrong_count=(uint8_t)c->wrong;
        world.quiz_category_correct[0]=(uint16_t)c->correct_seed;world.quiz_category_wrong[0]=(uint16_t)c->wrong_seed;world.quiz_fixed_correct=(uint16_t)c->fixed_seed;
        for(unsigned i=0;i<10;++i){world.quiz_category_correct[i]=(uint16_t)c->initial_stats[2*i];world.quiz_category_wrong[i]=(uint16_t)c->initial_stats[2*i+1];}
        world.quiz_fixed_wrong=(uint16_t)c->initial_stats[21];
        world_map_frame_valid=system_menu_frame_valid=0;phase=submitted=0;memset(stage_submits,0,sizeof(stage_submits));fake_clock=case_index==1?UINT32_MAX-1000:0;
        advance_world_events(window,renderer,texture,&file,&font,&world,1,1);
        if(world.game_mode==1){unsigned levels[3]={0};assert(!draw_world(renderer,texture,&file,&world,0,0,0,0,0,0,levels,0,&font));}
        const unsigned *v=c->state;
        assert(world.game_mode==v[0]&&world.story_event_id==v[1]&&world.event_context_id==v[2]&&
            world.quiz_answer_count==v[3]&&world.quiz_wrong_count==v[4]&&world.quiz_previous_index==v[5]&&world.money==v[6]);
        assert(!strcmp(world.quiz_input,c->input)&&submitted==c->frames);
        for(unsigned i=0;i<10;++i)assert(world.quiz_category_correct[i]==c->stats[2*i]&&world.quiz_category_wrong[i]==c->stats[2*i+1]);
        assert(world.quiz_fixed_correct==c->stats[20]&&world.quiz_fixed_wrong==c->stats[21]);
        for(unsigned i=0;i<=c->keys;++i)assert(stage_submits[i]==c->commits[i]);
    }
    printf("Original quiz:19 service cases,%u actual C/SDL submissions matched\n",total);
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
