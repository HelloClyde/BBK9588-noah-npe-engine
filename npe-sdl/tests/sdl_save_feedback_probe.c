#include "../npe.h"
#include "../text.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
static Uint32 clock_ms;
static unsigned direct, saving, presents, polls, quit_prompt, integration, key_index, dismiss_key;
static NpeWorld *live;
static uint8_t expected_frame[2][NPE_WIDTH*NPE_HEIGHT];
static int wait_outer(SDL_Event *, int);
static Uint32 ticks(void) { return clock_ms; }
static void delay(Uint32 ms) { clock_ms += ms; }
static int poll(SDL_Event *event);
static void present(SDL_Renderer *);
static void select_sound(NpeSoundPlayer *, NpeWorld *);
static int save_world(const NpeFile *, const NpeWorld *, const char *);
static SDL_Window *create_window(const char *title,int x,int y,int width,int height,Uint32 flags) {
    (void)width;(void)height;
    return SDL_CreateWindow(title,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
#define SDL_CreateWindow create_window
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent poll
#define SDL_RenderPresent present
#define SDL_WaitEventTimeout wait_outer
#define npe_sound_select select_sound
#define npe_world_save save_world
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_CreateWindow
#undef npe_world_save
#undef npe_sound_select
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef SDL_PollEvent
#undef SDL_Delay
#undef SDL_GetTicks

static int save_world(const NpeFile *f,const NpeWorld *w,const char *path) {
    return integration==3 ? -1 : npe_world_save(f,w,path);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w) {live=w;npe_sound_select(p,w);}
static int poll(SDL_Event *event) {
    if(quit_prompt && polls++==0) { memset(event,0,sizeof(*event));event->type=SDL_QUIT;return 1; }
    if(direct && dismiss_key && clock_ms>=250 && polls++==0) {
        memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;
        event->key.keysym.sym=dismiss_key==2?SDLK_F4:SDLK_RETURN;return 1;
    }
    return 0;
}
static void read_frame(const char *path,uint8_t *pixels) {
    FILE *stream=fopen(path,"rb");assert(stream);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,stream)==NPE_WIDTH*NPE_HEIGHT);
    assert(fgetc(stream)==EOF&&!fclose(stream));
}
static void compare(SDL_Renderer *renderer,const uint8_t *pixels) {
    uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];unsigned differences=0;
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){unsigned v=pixels[i];
        uint32_t color=UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15);
        differences+=rgba[i]!=color;
    }
    assert(!differences);
}
static void present(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);
    if(direct) {
        assert(presents<2);compare(renderer,expected_frame[presents]);++presents;
    }
}
static int wait_outer(SDL_Event *event,int timeout) {
    static const SDL_Keycode read_keys[]={SDLK_F4,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN};
    static const SDL_Keycode save_keys[]={SDLK_F4,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_RETURN};
    static const SDL_Keycode title_keys[]={SDLK_DOWN,SDLK_RETURN,SDLK_RETURN};
    static const SDL_Keycode new_keys[]={SDLK_RETURN};
    (void)timeout;memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;
    if(!key_index){
        if(integration>=4){live->game_mode=7;live->direction=3;}
        live->story_progress=4;
    }
    const SDL_Keycode *keys=integration==6?new_keys:integration>=4?title_keys:integration==3?save_keys:read_keys;
    unsigned count=integration==6?1:integration>=4?3:integration==3?8:7;
    if(key_index<count){event->key.keysym.sym=keys[key_index++];return 1;}
    if(integration==4||integration==6) {
        assert(live->game_mode==1&&live->map_number==1&&live->story_progress==1);
        uint8_t native[NPE_WIDTH*NPE_HEIGHT];
        read_frame(integration==4?"tmp/native-new-game-empty-prior-3-commit-0.idx":"tmp/native-new-game-new-prior-3-commit-0.idx",native);
        compare(last_pixel_renderer,native);
        assert(live->direction==1);
    } else if(integration==5) {
        assert(live->game_mode==7&&live->story_progress==4);
    } else {
        assert(live->game_mode==1&&live->map_number==9&&live->story_progress==4);
        assert(clock_ms==2000);
    }
    event->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv) {
    assert(argc==2);NpeFile file;NpeFont font={0};uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&
           !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    assert(!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("save feedback",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    for(saving=0;saving<2;++saving) for(dismiss_key=0;dismiss_key<3;++dismiss_key) {
        uint8_t before[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(before)],expected[sizeof(before)];char path[160];
        if(!saving&&dismiss_key)snprintf(path,sizeof(path),"tmp/native-empty-notice-key-%u-before.idx",dismiss_key==1?16:64);
        else snprintf(path,sizeof(path),"tmp/native-empty-system-1-before.idx");
        read_frame(path,before);
        memcpy(pixels,before,sizeof(pixels));assert(!npe_render_save_failure(&font,saving,pixels));
        if(!saving) {
            if(dismiss_key)snprintf(path,sizeof(path),"tmp/native-empty-notice-key-%u-commit-0.idx",dismiss_key==1?16:64);
            else snprintf(path,sizeof(path),"tmp/native-empty-system-1-commit-0.idx");
            read_frame(path,expected);
            assert(!memcmp(pixels,expected,sizeof(pixels)));
        }
        memcpy(expected_frame[0],pixels,sizeof(pixels));memcpy(expected_frame[1],before,sizeof(before));
        if(!saving&&dismiss_key){snprintf(path,sizeof(path),"tmp/native-empty-notice-key-%u-commit-1.idx",dismiss_key==1?16:64);
            read_frame(path,expected);assert(!memcmp(expected_frame[1],expected,sizeof(expected)));}
        direct=0;draw_pixels(renderer,texture,before);direct=1;clock_ms=polls=presents=0;
        assert(!show_save_failure(renderer,texture,&font,saving));
        assert(clock_ms==(dismiss_key?250u:2000u)&&presents==2&&!memcmp(last_pixel_frame,before,sizeof(before)));
    }
    /* Closing the host window during the prompt neither restores nor returns to map. */
    direct=0;quit_prompt=1;polls=clock_ms=0;
    assert(show_save_failure(renderer,texture,&font,0)==-2&&clock_ms==0);
    quit_prompt=0;
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_font_free(&font);npe_free(&file);

    const char *copy="tmp/npe-save-feedback.npe";
    assert(!read_file(argv[1],&bytes,&size));memcpy(bytes+0x0c,"SAVE-FAIL-01",12);
    FILE *stream=fopen(copy,"wb");assert(stream&&fwrite(bytes,1,size,stream)==size&&!fclose(stream));
    assert(!npe_parse(&file,bytes,size,error,sizeof(error)));
    char *path=game_save_path(&file,0);assert(path);remove(path);
    for(integration=1;integration<=6;++integration) {
        if(integration==2 || integration==5) {
            stream=fopen(path,"wb");assert(stream&&fwrite("broken",1,6,stream)==6&&!fclose(stream));
        } else remove(path);
        clock_ms=key_index=polls=0;
        char *args[]={"npe-sdl","--walk-preview","9,29,13",(char *)copy,NULL};
        assert(!run(4,args));
        fprintf(stderr,"SDL save failure integration%u passed\n",integration);
    }
    remove(path);assert(!remove(copy));free(path);npe_free(&file);
    puts("Original load failure pixels, source save prompt, 2000ms timeout/early key/quit behavior, six SDL save/title paths and two original initial map comparisons passed");
    return 0;
}
