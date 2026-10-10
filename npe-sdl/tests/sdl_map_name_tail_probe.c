/* Original glyph/coordinate fixtures, not complete original LCD captures. */
#include "../text.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main

static unsigned word(const uint8_t *p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
int main(int argc,char **argv){
    static const char *names[]={"map139-loaded","map140-loaded",
        "map139-warm-ascii","map139-warm-gbk","map139-warm-control",
        "map139-warm-clip-pair","map139-warm-no-nul","map140-warm-ignored"};
    assert(argc==3);NpeFile file;uint8_t *bytes;size_t size;char error[160];NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    assert(!load_native_fonts(&font));
    SDL_Window *window=SDL_CreateWindow("NPE map name tail",0,0,NPE_WIDTH,NPE_HEIGHT,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    uint64_t fingerprint=npe_game_fingerprint(&file);unsigned failures=0;
    for(unsigned c=0;c<sizeof(names)/sizeof(names[0]);++c){
        char path[4096];uint8_t input[106],expected[NPE_WIDTH*NPE_HEIGHT],pixels[NPE_WIDTH*NPE_HEIGHT]={0};
        int n=snprintf(path,sizeof(path),"%s/%s.input",argv[2],names[c]);assert(n>0&&(size_t)n<sizeof(path));
        FILE *f=fopen(path,"rb");assert(f&&fread(input,1,sizeof(input),f)==sizeof(input)&&fgetc(f)==EOF);fclose(f);
        n=snprintf(path,sizeof(path),"%s/%s.idx",argv[2],names[c]);assert(n>0&&(size_t)n<sizeof(path));
        f=fopen(path,"rb");assert(f&&fread(expected,1,sizeof(expected),f)==sizeof(expected)&&fgetc(f)==EOF);fclose(f);
        NpeWorld world;assert(!npe_world_init(&file,&world,1,1,1));world.map_number=(uint16_t)word(input);
        assert(!npe_world_resources_open(&file,&world));
        uint8_t name[13];assert(!npe_rpg_runtime_read(&file,UINT32_C(0x5278e4),name,sizeof(name)));
        unsigned cache_bad=memcmp(name,input+10,sizeof(name))!=0;
        /* Only warm following bytes are changed. The current map loader must
           supply its own 12-byte name and terminator and preserve this tail. */
        assert(!npe_rpg_runtime_write(&file,UINT32_C(0x5278f1),input+23,83));
        int result=npe_render_map_name_notice(&file,&font,&world,pixels);unsigned pixel_bad=0;
        for(unsigned i=0;i<sizeof(pixels);++i)pixel_bad+=pixels[i]!=expected[i];
        draw_pixels(renderer,texture,pixels);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
        assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
        for(unsigned i=0;i<sizeof(pixels);++i){unsigned v=pixels[i];
            assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
        unsigned passed=!result&&!cache_bad&&!pixel_bad;failures+=!passed;
        printf("%s: cached name %s, original glyph/position pixels %u differences, actual SDL %s\n",
            names[c],cache_bad?"mismatch":"matches",pixel_bad,passed?"passed":"failed");
        /* Loading another map and returning must retain78F1, while replacing
           only the original13 name bytes. No new World/save field is needed. */
        uint8_t tail;world.map_number=1;assert(!npe_rpg_runtime_map_load(&file,&world,0));
        world.map_number=(uint16_t)word(input);assert(!npe_rpg_runtime_map_load(&file,&world,0));
        assert(!npe_rpg_runtime_read(&file,UINT32_C(0x5278f1),&tail,1)&&tail==input[23]);
        assert(!npe_rpg_runtime_failures(&file)&&npe_game_fingerprint(&file)==fingerprint);
        npe_rpg_runtime_close(&file);
    }
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_free(&file);SDL_Quit();
    if(!failures)puts("8 original map-name RAM/strlen/glyph paths and actual SDL output; warm map reload retention passed");
    return failures?1:0;
}
