#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
#include "map_bitmap_runtime_native.h"
static uint8_t ram[131072],reference_ram[131072],pixels[28800],reference_pixels[28800];
static NpeWorld world;
static void load(const char *path,uint8_t *data,unsigned size){
    FILE *f=fopen(path,"rb");assert(f && fread(data,1,size,f)==size && fgetc(f)==EOF && !fclose(f));
}
static void compare(const char *name,const uint8_t *a,const uint8_t *b,unsigned size){
    for(unsigned i=0;i<size;++i)if(a[i]!=b[i]){fprintf(stderr,"%s byte%u:C%02x original%02x\n",name,i,a[i],b[i]);assert(0);}
}
int main(int argc,char **argv){
    assert(argc==2 && !SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    SDL_Window *window=SDL_CreateWindow("Original map resource pipeline",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned c=0;c<sizeof(map_bitmap_runtime_cases)/sizeof(*map_bitmap_runtime_cases);++c){
        const MapBitmapRuntimeCase *test=&map_bitmap_runtime_cases[c];
        load(test->ram,reference_ram,sizeof(reference_ram));load(test->pixels,reference_pixels,sizeof(reference_pixels));
        uint8_t trace[10];load(test->trace,trace,sizeof(trace));
        for(unsigned binding=0;binding<2;++binding){
            memset(&world,0,sizeof(world));world.battle_record_party_count=3;world.battle_party_flags_saved=1;
            assert(!(binding?npe_world_resources_open(&file,&world):npe_rpg_runtime_open(&file,NULL)));
            static const uint32_t slots[]={0x527afc,0x527ae6,0x527aee,0x527afe,0x527af2};
            assert(test->group<=4);
            uint32_t address=slots[test->group];
            uint8_t base[2]={(uint8_t)(test->base>>8),(uint8_t)test->base};
            assert(!npe_rpg_runtime_write(&file,address,base,2));
            assert(npe_image_group_index(&file,test->group,test->offset)==test->index);
            /* Cached total count and header projection must not override
               the original live group word after the runtime is attached. */
            NpeFile changed=file;changed.indexed_bitmap_count+=75;changed.vectors[test->group]^=0x100;
            assert(npe_image_group_index(&changed,test->group,test->offset)==test->index);
            memset(pixels,0,sizeof(pixels));
            assert(!npe_render_indexed_resource(&file,test->index,test->x,test->y,test->kind,pixels));
            assert(!npe_rpg_runtime_read(&file,0x510000,ram,sizeof(ram)));
            compare(test->name,ram,reference_ram,sizeof(ram));compare(test->name,pixels,reference_pixels,sizeof(pixels));
            uint32_t cursor=(uint32_t)trace[0]<<24|(uint32_t)trace[1]<<16|(uint32_t)trace[2]<<8|trace[3];
            assert(npe_rpg_runtime_cursor(&file)==cursor);
            assert((uint16_t)npe_rpg_runtime_reads(&file)==((unsigned)trace[4]<<8|trace[5]));
            assert(npe_rpg_runtime_failures(&file)==((unsigned)trace[6]<<8|trace[7]));
            draw_pixels(renderer,texture,pixels);uint32_t rgba[28800];
            assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
            for(unsigned i=0;i<28800;++i){unsigned v=reference_pixels[i];
                assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
            npe_rpg_runtime_close(&file);
            fprintf(stderr,"Map resource %s binding%u:full RAM/pixels/IO/actual SDL match\n",test->name,binding);
        }
    }
    fprintf(stderr,"Map resources:30 original cases/60 binding paths and actual SDL frames matched\n");
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_free(&file);SDL_Quit();return 0;
}
