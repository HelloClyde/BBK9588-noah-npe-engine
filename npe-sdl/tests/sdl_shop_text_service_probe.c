#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
static int shop_text_wait(SDL_Event *);
static void shop_text_present(SDL_Renderer *);
#define SDL_WaitEvent shop_text_wait
#define SDL_RenderPresent shop_text_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#include "shop_text_probe_helpers.h"
static const ShopTextCase *current;
static NpeWorld *active;
static unsigned phase,frames,waits;
static void shop_text_present(SDL_Renderer *renderer){
    const ShopTextPhase *p=current->phases+phase;assert(p->count&&frames==p->index);
    unsigned bad=shop_text_difference(active,&p->frame);
    fprintf(stderr,"SDL %s phase%u state differences%u\n",current->name,phase,bad);assert(!bad);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    shop_text_frame(current->name,(int)frames,native);SDL_RenderPresent(renderer);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    bad=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        bad+=last_pixel_frame[i]!=v;
        bad+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));
    }
    fprintf(stderr,"SDL %s frame%u differences%u\n",current->name,frames,bad);assert(!bad);++frames;
}
static int shop_text_wait(SDL_Event *event){
    assert(!shop_text_difference(active,&current->phases[phase].after));
    assert(++phase<current->count);const ShopTextPhase *p=current->phases+phase;
    shop_text_patch(active,p);++waits;memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;
    switch(p->key){
    case 0x84:event->key.keysym.sym=SDLK_LEFT;break;
    case 0x85:event->key.keysym.sym=SDLK_RIGHT;break;
    case 0x82:event->key.keysym.sym=SDLK_UP;break;
    case 0x80:event->key.keysym.sym=SDLK_j;break;
    case 0x8e:event->key.keysym.sym=SDLK_F4;break;
    default:assert(0);
    }
    return 1;
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("shop text",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    NpeWorld world;active=&world;unsigned total=0;
    for(unsigned c=0;c<sizeof(shop_text_cases)/sizeof(*shop_text_cases);++c){current=shop_text_cases+c;
        assert(!npe_world_init(&file,&world,1,7,10)&&!npe_world_resources_open(&file,&world));shop_text_seed(&world,&current->before);
        shop_text_frame(current->name,-1,last_pixel_frame);last_pixel_renderer=renderer;shop_frame_valid=0;
        phase=frames=waits=0;uint8_t bitmap[32],after[32];assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32));
        assert(!show_shop_quantity(renderer,texture,&file,&font,&world));
        assert(phase+1==current->count&&waits+1==current->count&&frames==current->commits);
        assert(!shop_text_difference(&world,&current->phases[phase].after));
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,after,32)&&!memcmp(bitmap,after,32));
        total+=frames;npe_rpg_runtime_close(&file);
    }
    printf("8 actual SDL shop text paths: %u original frames/states, retained quantity/choice pixels and signed boundaries passed\n",total);
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
