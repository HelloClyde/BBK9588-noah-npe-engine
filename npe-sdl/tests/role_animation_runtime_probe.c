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
#include "role_animation_runtime_native.h"
static uint8_t ram[131072],expected_ram[131072],pixels[28800],expected_pixels[28800];
static NpeWorld world;
static void load(const char *path,uint8_t *data,unsigned size){
    FILE *f=fopen(path,"rb");assert(f);
    assert(fread(data,1,size,f)==size && fgetc(f)==EOF);assert(!fclose(f));
}
static void compare(const char *name,const uint8_t *a,const uint8_t *b,unsigned size){
    for(unsigned i=0;i<size;++i)if(a[i]!=b[i]){
        fprintf(stderr,"%s byte%u: C%02x original%02x\n",name,i,a[i],b[i]);assert(0);
    }
}
static void check(const NpeFile *file,const char *name,const char *ram_path,const char *pixel_path,const char *trace_path,unsigned trace_size){
    load(ram_path,expected_ram,sizeof(expected_ram));load(pixel_path,expected_pixels,sizeof(expected_pixels));
    assert(!npe_rpg_runtime_read(file,0x510000,ram,sizeof(ram)));
    compare(name,ram,expected_ram,sizeof(ram));compare(name,pixels,expected_pixels,sizeof(pixels));
    uint8_t trace[10];load(trace_path,trace,trace_size);
    uint32_t cursor=(uint32_t)trace[0]<<24|(uint32_t)trace[1]<<16|(uint32_t)trace[2]<<8|trace[3];
    assert(npe_rpg_runtime_cursor(file)==cursor);
    assert((uint16_t)npe_rpg_runtime_reads(file)==((unsigned)trace[4]<<8|trace[5]));
    assert(npe_rpg_runtime_failures(file)==((unsigned)trace[6]<<8|trace[7]));
}
int main(int argc,char **argv){
    assert(argc==2&&!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    SDL_Window *window=SDL_CreateWindow("Original role stream",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    unsigned checked=0;
    for(unsigned c=0;c<sizeof(role_runtime_cases)/sizeof(*role_runtime_cases);++c){
        const RoleRuntimeCase *test=&role_runtime_cases[c];
        for(unsigned binding=0;binding<2;++binding){
            memset(&world,0,sizeof(world));world.battle_record_party_count=3;world.battle_party_flags_saved=1;
            assert(!(binding?npe_world_resources_open(&file,&world):npe_rpg_runtime_open(&file,NULL)));
            const uint32_t addresses[]={0x5279de,0x5279e2},values[]={test->directory,test->payload};
            for(unsigned i=0;i<2;++i){uint32_t v=values[i];uint8_t data[4]={(uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v};
                assert(!npe_rpg_runtime_write(&file,addresses[i],data,4));}
            memset(pixels,0,sizeof(pixels));int result=npe_rpg_runtime_role_begin(&file,test->group,test->x,test->y,test->end_x,test->end_y);
            if(test->frames){
                assert(!result && npe_rpg_runtime_role_steps(&file)==test->frames);
                assert(npe_rpg_runtime_role_begin(&file,test->group,test->x,test->y,test->end_x,test->end_y)==-1);
                for(unsigned i=0;i<test->frames;++i){
                    int16_t position[2];
                    assert(!npe_rpg_runtime_role_frame(&file,test->kind,pixels,position));
                    const RoleRuntimeFrame *frame=&test->references[i];
                    assert(position[0]==frame->x && position[1]==frame->y);
                    check(&file,test->name,frame->ram,frame->pixels,frame->trace,8);
                    draw_pixels(renderer,texture,pixels);uint32_t rgba[28800];
                    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
                    for(unsigned p=0;p<28800;++p){unsigned v=expected_pixels[p];
                        assert(rgba[p]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
                    ++checked;
                }
                assert(npe_rpg_runtime_role_frame(&file,0,pixels,NULL)==-1);
                npe_rpg_runtime_role_end(&file);
            }
            if(!result && !test->frames){
                assert(!npe_rpg_runtime_role_steps(&file));
                assert(npe_rpg_runtime_role_frame(&file,test->kind,pixels,NULL)==-1);
                npe_rpg_runtime_role_end(&file);
            }
            uint8_t trace[10];load(test->trace,trace,10);
            assert((uint16_t)result==((unsigned)trace[8]<<8|trace[9]));
            check(&file,test->name,test->ram,test->pixels,test->trace,10);
            assert(!npe_rpg_runtime_role_steps(&file));npe_rpg_runtime_role_end(&file);
            npe_rpg_runtime_close(&file);
            fprintf(stderr,"Role stream %s binding%u: full RAM/pixels/IO/status match\n",test->name,binding);
        }
    }
    fprintf(stderr,"Role stream:20 original cases/40 binding paths/%u actual SDL frames matched\n",checked);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_free(&file);SDL_Quit();return 0;
}
