#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {
    const char *name,*prefix;
    unsigned direction,frames,lower,upper,after_view[4];
    uint16_t original_cache[105],before_cache[105],after_cache[105];
} WalkCacheCase;
#include "walk_live_cache_native.h"
typedef struct {SDL_Renderer *renderer;SDL_Texture *texture;const WalkCacheCase *cfg;unsigned frame;} Comparison;
static const NpeFile *comparison_file;
static void reference(const char *path,uint8_t *pixels){
    FILE *f=fopen(path,"rb");assert(f&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT&&fgetc(f)==EOF&&!fclose(f));
}
static void cache(const NpeFile *file,const uint16_t expected[105]){
    uint8_t bytes[210];assert(!npe_rpg_runtime_read(file,0x5270b4,bytes,sizeof(bytes)));
    unsigned diff=0;
    for(unsigned i=0;i<105;++i)if((unsigned)bytes[2*i]*256+bytes[2*i+1]!=expected[i]){
        fprintf(stderr,"cache word%u C=%u native=%u\n",i,(unsigned)bytes[2*i]*256+bytes[2*i+1],expected[i]);++diff;}
    assert(!diff);
}
static int commit(void *opaque,const uint8_t *pixels,unsigned wait){
    Comparison *c=opaque;(void)wait;char path[160];assert(c->frame<c->cfg->frames);
    snprintf(path,sizeof(path),"%s-%u.idx",c->cfg->prefix,c->frame);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];reference(path,native);
    unsigned diff=0;for(unsigned i=0;i<sizeof(native);++i)diff+=native[i]!=pixels[i];
    fprintf(stderr,"%s frame%u C=%u\n",c->cfg->name,c->frame,diff);
    if(c->frame+1==c->cfg->frames)cache(comparison_file,c->cfg->after_cache);
    if(diff){FILE *f=fopen("tmp/cross-npe-boot/live-cache-actual.idx","wb");assert(f&&fwrite(pixels,1,sizeof(native),f)==sizeof(native)&&!fclose(f));}
    assert(!diff);
    draw_pixels(c->renderer,c->texture,pixels);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(c->renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    ++c->frame;return 0;
}
int main(int argc,char **argv){
    assert(argc==2);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    comparison_file=&file;
    uint64_t fingerprint=npe_game_fingerprint(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("original live viewport",0,0,NPE_WIDTH,NPE_HEIGHT,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    unsigned total=0;
    for(unsigned i=0;i<sizeof(walk_cache_cases)/sizeof(*walk_cache_cases);++i){
        NpeWorld world;const WalkCacheCase *c=&walk_cache_cases[i];
        assert(!npe_world_new_game(&file,&world)&&!npe_world_resources_open(&file,&world));
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[sizeof(pixels)];
        world.lcd_background=2;assert(!npe_render_world_load(&file,&world,pixels));
        char path[160];snprintf(path,sizeof(path),"%s-before.idx",c->prefix);reference(path,native);
        assert(!memcmp(pixels,native,sizeof(pixels)));cache(&file,c->original_cache);
        uint8_t zero[2]={0,0};assert(!npe_rpg_runtime_write(&file,0x527a50,zero,2)&&!npe_rpg_runtime_write(&file,0x527a5c,zero,2));
        for(unsigned j=0;j<20;++j){
            assert(!npe_rpg_runtime_write(&file,0x526ca2+14*j,zero,2));
            assert(!npe_rpg_runtime_write(&file,0x526ca2+14*j+11,zero,1));
        }
        uint8_t lower[2]={(uint8_t)(c->lower>>8),(uint8_t)c->lower};
        uint8_t upper[2]={(uint8_t)(c->upper>>8),(uint8_t)c->upper};
        assert(!npe_rpg_runtime_write(&file,0x5270b4+44,lower,2)&&!npe_rpg_runtime_write(&file,0x5270b4+14,upper,2));
        cache(&file,c->before_cache);
        Comparison comparison={renderer,texture,c,0};
        int result=npe_world_step_with_display(&file,&world,c->direction,pixels,commit,&comparison);
        assert(result>=0&&comparison.frame==c->frames);cache(&file,c->after_cache);
        assert(world.game_mode==1&&world.map_number==1&&world.view.offset_x==c->after_view[0]&&world.view.offset_y==c->after_view[1]&&world.view.screen_x==c->after_view[2]&&world.view.screen_y==c->after_view[3]);
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);total+=comparison.frame;
    }
    assert(fingerprint==npe_game_fingerprint(&file));
    printf("3 original live-cache paths, %u C/SDL frames and 630 cache words matched; NPE unchanged\n",total);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);return 0;
}
