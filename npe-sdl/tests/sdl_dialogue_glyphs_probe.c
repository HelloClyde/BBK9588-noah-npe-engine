#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
#include "../native_npc_fonts.inc"

static unsigned little_word(const uint8_t *bytes){
    return (unsigned)bytes[0]|(unsigned)bytes[1]<<8;
}
static FILE *fixture(const char *directory,const char *name,const char *suffix){
    char path[4096];
    int n=snprintf(path,sizeof(path),"%s/%s%s",directory,name,suffix);
    assert(n>0&&(size_t)n<sizeof(path));
    FILE *stream=fopen(path,"rb");assert(stream);return stream;
}
int main(int argc,char **argv){
    assert(argc==2||(argc==3&&!strcmp(argv[2],"--rich-text")));
    unsigned rich=argc==3;
    static const char *ordinary_names[]={"npc-0","npc-1","npc-2","npc-3","npc-4","npc-5",
        "extended","fallback","control-del","pair-edge","low-trail-edge","unpaired",
        "story-extended","bottom-clip"};
    static const char *rich_names[]={"font-6","font-8","marked","marked-high","mixed",
        "marked-wrap","font-edge","marked-pair-edge","marked-newline",
        "small-control-run","zero-trail","unclosed-mark"};
    const char *const *names=rich?rich_names:ordinary_names;
    unsigned inputs=rich?sizeof(rich_names)/sizeof(*rich_names):
        sizeof(ordinary_names)/sizeof(*ordinary_names);
    if(rich){
        FILE *stream=fixture(argv[1],"fonts",".bin");
        uint8_t native[sizeof(native_npc_fonts)];
        assert(fread(native,1,sizeof(native),stream)==sizeof(native)&&fgetc(stream)==EOF&&!fclose(stream));
        assert(!memcmp(native,native_npc_fonts,sizeof(native)));
    }
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("original dialogue glyphs",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    NpeWorld *world=malloc(sizeof(*world));assert(world);
    unsigned total=0,failed=0;
    for(unsigned i=0;i<inputs;++i){
        uint8_t header[22],raw[64],expected[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(expected)];
        FILE *stream=fixture(argv[1],names[i],".input");
        assert(fread(header,1,sizeof(header),stream)==sizeof(header));
        unsigned game=little_word(header),size=little_word(header+16);
        assert(game<6&&size<sizeof(raw)&&fread(raw,1,size,stream)==size);
        assert(fgetc(stream)==EOF&&!fclose(stream));
        char path[4096];int n=snprintf(path,sizeof(path),"%s/game%u.npe",argv[1],game);
        assert(n>0&&(size_t)n<sizeof(path));uint8_t *source;size_t source_size;
        assert(!read_file(path,&source,&source_size));
        size_t original_size=source_size;
        uint32_t pointer=(uint32_t)little_word(header+18)|(uint32_t)little_word(header+20)<<16;
        if(rich||i>=6){
            /* Isolated byte/layout diagnostics append a text input, preserving
               every original resource byte. No production file is rewritten. */
            source=realloc(source,source_size+size+2u);assert(source);
            memcpy(source+source_size,raw,size);source[source_size+size]='@';
            source[source_size+size+1]=0;pointer=(uint32_t)source_size;source_size+=size+2u;
        }else assert(pointer+little_word(header+10)+size<=source_size&&
            !memcmp(source+pointer+little_word(header+10),raw,size));
        NpeFile file;char error[128];assert(!npe_parse(&file,source,original_size,error,sizeof(error)));
        if(rich||i>=6)file.size=source_size; /* Diagnostic file-read backing only. */
        for(unsigned story=0;story<(rich?1u:2u);++story){
            if(!story&&i>=12)continue; /* NPC controller owns its fixed rectangle. */
            stream=fixture(argv[1],names[i],rich?".idx":story?"-story.idx":"-npc.idx");
            assert(fread(expected,1,sizeof(expected),stream)==sizeof(expected)&&fgetc(stream)==EOF&&!fclose(stream));
            assert(!npe_world_new_game(&file,world)&&!npe_world_resources_open(&file,world));
            world->text_file_position=pointer;world->text_offset=(uint16_t)little_word(header+10);
            world->text_more=1;world->text_style=0;
            for(unsigned j=0;j<4;++j)world->text_rect[j]=(uint16_t)little_word(header+2+j*2);
            uint8_t bitmap[32],after_bitmap[32];
            assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap,sizeof(bitmap)));
            memset(pixels,7,sizeof(pixels));
            int result=story?npe_render_world_story_page(&file,&font,world,pixels):
                npe_render_world_npc_page(&file,&font,world,pixels,0,NULL,NULL);
            unsigned bad=0,state_bad=0,sdl_bad=0;
            for(size_t k=0;k<sizeof(pixels);++k)bad+=pixels[k]!=expected[k];
            state_bad+=world->text_file_position!=pointer;
            state_bad+=world->text_offset!=little_word(header+12);
            state_bad+=world->text_more!=little_word(header+14);
            assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,after_bitmap,sizeof(after_bitmap)));
            state_bad+=memcmp(bitmap,after_bitmap,sizeof(bitmap))!=0;
            draw_pixels(renderer,texture,pixels);
            uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
            assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
            for(size_t k=0;k<sizeof(pixels);++k){unsigned v=expected[k];
                uint32_t wanted=UINT32_C(0xff000000)|((183-v*142/15)<<16)|
                    ((200-v*146/15)<<8)|(161-v*119/15);
                sdl_bad+=rgba[k]!=wanted;
            }
            fprintf(stderr,"%s %s: result=%d C=%u SDL=%u state=%u\n",
                story?"story":"NPC",names[i],result,bad,sdl_bad,state_bad);
            failed+=(result!=0||bad||sdl_bad||state_bad);++total;
            npe_rpg_runtime_close(&file);
        }
        npe_free(&file);
    }
    printf("Original rectangle/glyph reference: %u resident NPC/story and actual SDL paths, %u failed\n",total,failed);
    free(world);npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);SDL_Quit();return failed?1:0;
}
