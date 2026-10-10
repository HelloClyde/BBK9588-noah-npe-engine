#include <assert.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
static SDL_Renderer *overlay_renderer;
static SDL_Texture *overlay_texture;
static void overlay_sdl_check(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                              unsigned icon,const char *name,const uint8_t *reference) {
    assert(!draw_battle_scene(overlay_renderer,overlay_texture,file,51,world,font,icon,0,0,0));
    uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(overlay_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){
        uint32_t value=reference[i],red=183-value*142/15,
                 green=200-value*146/15,blue=161-value*119/15;
        differences+=rgba[i]!=(UINT32_C(0xff000000)|red<<16|green<<8|blue);
    }
    fprintf(stderr,"SDL status overlay %s: %u differences\n",name,differences);assert(!differences);
}
#define OVERLAY_FRAME_CHECK(file,font,world,c,reference) overlay_sdl_check(file,font,world,(c)->icon,(c)->name,reference)
#define main overlay_numeric_main
#include "status_overlay_probe.c"
#undef main
#undef OVERLAY_FRAME_CHECK
int main(int argc,char **argv) {
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    SDL_Window *window=SDL_CreateWindow("original status overlay",0,0,240,120,SDL_WINDOW_HIDDEN);assert(window);
    overlay_renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(overlay_renderer);
    overlay_texture=SDL_CreateTexture(overlay_renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(overlay_texture);
    int result=overlay_numeric_main(argc,argv);
    SDL_DestroyTexture(overlay_texture);SDL_DestroyRenderer(overlay_renderer);SDL_DestroyWindow(window);SDL_Quit();return result;
}
