#include "../npe.h"
#include "menu_guard_native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#ifdef main
#undef main
#endif

static const MusicMenuCase *current_case;
static unsigned input_index, checked_states, requests, sections[4], presents;
static unsigned integration, outer_keys, popup_keys, integration_closes, desktop_dialogs;
static int probe_wait(SDL_Event *event);
static int probe_wait_timeout(SDL_Event *event, int timeout);
static void probe_present(SDL_Renderer *renderer);
static int probe_message(Uint32, const char *, const char *, SDL_Window *);
static int probe_close(NpeWorld *world);
#define SDL_WaitEvent probe_wait
#define SDL_WaitEventTimeout probe_wait_timeout
#define SDL_RenderPresent probe_present
#define SDL_ShowSimpleMessageBox probe_message
#define npe_world_close_system_menu probe_close
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef npe_world_close_system_menu
#undef SDL_ShowSimpleMessageBox
#undef SDL_RenderPresent
#undef SDL_WaitEventTimeout
#undef SDL_WaitEvent

static SDL_Keycode native_key(unsigned mask) {
    switch (mask) {
        case 1: return SDLK_UP; case 2: return SDLK_DOWN;
        case 4: return SDLK_LEFT; case 8: return SDLK_RIGHT;
        case 16: return SDLK_RETURN; case 32: return SDLK_BACKSPACE;
        case 64: return SDLK_F4;
    }
    assert(0); return 0;
}
static void read_pixels(const char *path, uint8_t *pixels) {
    FILE *stream = fopen(path, "rb"); assert(stream);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,stream)==NPE_WIDTH*NPE_HEIGHT);
    assert(fgetc(stream)==EOF && !fclose(stream));
}
static void check_pixels(SDL_Renderer *renderer) {
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];
    uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    char path[128];
    snprintf(path,sizeof(path),"tmp/native-menu-guard-%s-frame-%u.idx",current_case->name,input_index);
    read_pixels(path,native);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        uint32_t color=UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15);
        differences += rgba[i] != color;
    }
    fprintf(stderr,"SDL music %s input%u: %u differences\n",current_case->name,input_index,differences);
    assert(!differences);
}
static void probe_present(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);
    if (current_case) { check_pixels(renderer); ++presents; }
}
static int probe_wait(SDL_Event *event) {
    memset(event,0,sizeof(*event)); event->type=SDL_KEYDOWN;
    if(integration) {
        assert(popup_keys<2);
        event->key.keysym.sym = popup_keys++ ? SDLK_RETURN : SDLK_UP;
    } else {
        assert(current_case && input_index<current_case->count);
        check_pixels(last_pixel_renderer); ++checked_states;
        event->key.keysym.sym=native_key(current_case->keys[input_index++]);
    }
    return 1;
}
static int probe_wait_timeout(SDL_Event *event,int timeout) {
    (void)timeout;assert(integration);memset(event,0,sizeof(*event));
    const SDL_Keycode keys[]={SDLK_RETURN,SDLK_ESCAPE,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,
                             SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN};
    const SDL_Keycode f4_keys[]={SDLK_RETURN,SDLK_F4,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,SDLK_F4};
    unsigned count=integration==1?sizeof(keys)/sizeof(*keys):sizeof(f4_keys)/sizeof(*f4_keys);
    if(outer_keys < count) {
        event->type=SDL_KEYDOWN;event->key.keysym.sym=integration==1?keys[outer_keys++]:f4_keys[outer_keys++];
    } else {
        assert(popup_keys==(integration==1?2:0) && integration_closes==1);
        event->type=SDL_QUIT;
    }
    return 1;
}
static int probe_message(Uint32 flags,const char *title,const char *message,SDL_Window *window) {
    (void)flags;(void)title;(void)message;(void)window;++desktop_dialogs;return 0;
}
static int probe_close(NpeWorld *world) {
    assert(world->game_mode==6);
    int result=npe_world_close_system_menu(world);
    assert(result==1 && world->game_mode==1);
    if(integration){assert(popup_keys==(integration==1?2:0) && world->music_enabled);++integration_closes;}
    return result;
}
static void observe(void *context,unsigned section,int loop) {
    (void)context;assert(requests<4);sections[requests++]=section;
    assert(loop==(section!=0));
}
int main(int argc,char **argv) {
    assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    assert(!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("music",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    for(unsigned i=0;i<sizeof(music_menu_cases)/sizeof(*music_menu_cases);++i){
        const MusicMenuCase *test=&music_menu_cases[i];uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(base)],native[sizeof(base)];char path[128];
        snprintf(path,sizeof(path),"tmp/native-menu-guard-%s-before.idx",test->name);read_pixels(path,base);
        NpeWorld world;assert(!npe_world_init(&file,&world,9,29,13));
        world.game_mode=6;world.music_enabled=(uint8_t)test->enabled;world.map_music_slot=1;
        npe_world_seed(&world,123);npe_world_sound_bind(&world,observe,NULL);requests=0;
        assert(!npe_world_resources_open(&file,&world));
        const uint8_t group[2]={(uint8_t)(test->ui_base>>8),(uint8_t)test->ui_base};
        assert(!npe_rpg_runtime_write(&file,0x527aec,group,2));
        /* Production must follow live RAM even when metadata projections differ. */
        unsigned cached_count=file.indexed_bitmap_count;uint16_t cached_base=file.vectors[7];
        file.indexed_bitmap_count=1;file.vectors[7]^=0x100;
        for(unsigned state=0;state<test->count;++state){
            memcpy(pixels,base,sizeof(base));assert(!npe_render_music_menu_overlay(&file,test->selected[state],pixels));
            snprintf(path,sizeof(path),"tmp/native-menu-guard-%s-frame-%u.idx",test->name,state);read_pixels(path,native);
            assert(!memcmp(pixels,native,sizeof(pixels)));
        }

        current_case=NULL;draw_pixels(renderer,texture,base);
        current_case=test;input_index=presents=0;
        int result=show_music_choice(renderer,texture,&file,&world);
        unsigned last=test->keys[test->count-1];
        assert(result==(last==64?0:1)&&input_index==test->count);
        assert(world.game_mode==1&&world.music_enabled==test->after_enabled&&world.random_state==123);
        unsigned expected_requests=last==64?0:test->after_enabled?2:1;
        assert(requests==expected_requests);
        for(unsigned r=0;r<requests;++r)assert(sections[r]==(test->after_enabled?21:0));
        unsigned expected_presents=1;
        for(unsigned j=0;j<test->count;++j)if(test->keys[j]==1||test->keys[j]==2)++expected_presents;
        assert(presents==expected_presents);
        file.indexed_bitmap_count=cached_count;file.vectors[7]=cached_base;
        npe_rpg_runtime_close(&file);
    }
    current_case=NULL;SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));npe_free(&file);
    integration=1;char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args));
    assert(outer_keys==9&&popup_keys==2&&integration_closes==1&&!desktop_dialogs);
    integration=2;outer_keys=popup_keys=integration_closes=0;assert(!run(2,args));
    assert(outer_keys==7&&!popup_keys&&integration_closes==1&&!desktop_dialogs);
    printf("5 original menu guard cases, %u C/SDL popup states, real system-menu entry and native F4 route passed\n",checked_states);
    return 0;
}
