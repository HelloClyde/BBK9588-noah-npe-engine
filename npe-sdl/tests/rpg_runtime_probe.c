#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
static int smoke;
static unsigned smoke_frames;
static void runtime_present(SDL_Renderer *renderer);
#define SDL_RenderPresent runtime_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
#include "rpg_runtime_native.h"
static uint8_t actual[131072],expected[131072],pixels[28800],native_pixels[28800];
static NpeWorld bound_world;
static void runtime_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);
    if(smoke){
        assert(!memcmp(last_pixel_frame,native_pixels,sizeof(native_pixels)));
        ++smoke_frames;SDL_Event event;memset(&event,0,sizeof(event));event.type=SDL_QUIT;
        assert(SDL_PushEvent(&event)==1);
    }
}
static void load(const char *path,uint8_t *data,size_t size){
    FILE *input=fopen(path,"rb");assert(input);assert(fread(data,1,size,input)==size&&fgetc(input)==EOF);fclose(input);
}
static void compare(const char *name,const uint8_t *a,const uint8_t *b,unsigned count){
    for(unsigned i=0;i<count;++i)if(a[i]!=b[i]){
        fprintf(stderr,"%s byte%u: C%02x original%02x\n",name,i,a[i],b[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2&&!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    SDL_Window *window=SDL_CreateWindow("RAM resource pipeline",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned c=0;c<sizeof(rpg_runtime_cases)/sizeof(*rpg_runtime_cases);++c){
        const RpgRuntimeCase *test=&rpg_runtime_cases[c];
        load(test->ram,expected,sizeof(expected));load(test->pixels,native_pixels,sizeof(native_pixels));
        uint8_t trace[10];load(test->trace,trace,sizeof(trace));
        for(unsigned binding=0;binding<2;++binding){
            memset(&bound_world,0,sizeof(bound_world));bound_world.battle_record_party_count=3;
            bound_world.battle_party_flags_saved=1;
            assert(!(binding?npe_world_resources_open(&file,&bound_world):npe_rpg_runtime_open(&file,NULL)));
            assert(npe_rpg_runtime_open(&file,NULL)==-1);
            /* This original fixture starts with zero diagnostic RAM and
               initializes only resource header fields. Production now also
               initializes the enemy directory; assert its file value, then
               restore the fixture's explicit initial state before comparing
               the unchanged resource-service reference RAM. */
            uint8_t enemy_header[4],zero_header[4]={0};
            assert(!npe_rpg_runtime_read(&file,0x5279b6,enemy_header,4));
            uint32_t enemy_directory=(uint32_t)enemy_header[0]<<24|(uint32_t)enemy_header[1]<<16|
                                     (uint32_t)enemy_header[2]<<8|enemy_header[3];
            assert(enemy_directory==file.sections[4].offset);
            assert(!npe_rpg_runtime_write(&file,0x5279b6,zero_header,4));
            /* These older isolated resource references also start with
               zero voice caches. Verify the production header mapping,
               then reproduce their unchanged explicit initial RAM. */
            const unsigned voice_sections[]={36,33};
            const uint32_t voice_addresses[]={0x527942,0x527962};
            for(unsigned v=0;v<2;++v){uint8_t descriptor[8],zero[8]={0};
                assert(!npe_rpg_runtime_read(&file,voice_addresses[v],descriptor,8));
                uint32_t length=(uint32_t)descriptor[0]<<24|(uint32_t)descriptor[1]<<16|
                                (uint32_t)descriptor[2]<<8|descriptor[3];
                uint32_t offset=(uint32_t)descriptor[4]<<24|(uint32_t)descriptor[5]<<16|
                                (uint32_t)descriptor[6]<<8|descriptor[7];
                assert(length==file.sections[voice_sections[v]].size&&offset==file.sections[voice_sections[v]].offset);
                assert(!npe_rpg_runtime_write(&file,voice_addresses[v],zero,8));
            }
            memset(pixels,0,sizeof(pixels));
            if(test->indexed){
                const uint32_t addresses[]={0x5279c6,0x5279ca,0x5279ce};
                const uint32_t values[]={test->directory_size,test->directory,test->payload};
                for(unsigned i=0;i<3;++i){uint32_t v=values[i];
                    uint8_t bytes[4]={(uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v};
                    assert(!npe_rpg_runtime_write(&file,addresses[i],bytes,4));
                }
                int result=npe_rpg_runtime_draw_indexed(&file,test->number,test->x,test->y,test->kind,pixels);
                assert((uint16_t)result==((unsigned)trace[8]<<8|trace[9]));
            }else assert(!npe_rpg_runtime_draw(&file,(uint16_t)test->number,test->x,test->y,(uint8_t)test->kind,pixels));
            assert(!npe_rpg_runtime_read(&file,0x510000,actual,sizeof(actual)));
            compare(test->name,actual,expected,sizeof(actual));compare(test->name,pixels,native_pixels,sizeof(pixels));
            assert(npe_rpg_runtime_cursor(&file)==((uint32_t)trace[0]<<24|((uint32_t)trace[1]<<16)|((unsigned)trace[2]<<8)|trace[3]));
            assert(npe_rpg_runtime_failures(&file)==((unsigned)trace[6]<<8|trace[7]));
            draw_pixels(renderer,texture,pixels);
            uint32_t rgba[28800];assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
            for(unsigned i=0;i<28800;++i){unsigned v=native_pixels[i];assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
            if(binding){
                assert(npe_rpg_runtime_owner(&file)==&bound_world);
                uint8_t hp[2]={0x12,0x34};assert(!npe_rpg_runtime_write(&file,0x5264a0,hp,2));assert(bound_world.hp[0]==0x1234);
                bound_world.hp[0]=0x5678;assert(!npe_rpg_runtime_read(&file,0x5264a0,hp,2));assert(hp[0]==0x56&&hp[1]==0x78);
            }
            npe_rpg_runtime_close(&file);assert(!file.rpg_runtime);
            fprintf(stderr,"RPG runtime %s binding%u: complete128KiB RAM/28800 pixels/actual SDL match\n",test->name,binding);
        }
        if(!strcmp(test->name,"viewer-514")){
            assert(!npe_rpg_runtime_open(&file,NULL));assert(!draw_battle_asset(renderer,texture,&file,513));
            compare("main asset viewer",last_pixel_frame,native_pixels,28800);npe_rpg_runtime_close(&file);
        }
    }
    assert(npe_rpg_runtime_draw(&file,1,0,0,0,pixels)==-1);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_free(&file);SDL_Quit();
    const RpgRuntimeCase *viewer=&rpg_runtime_cases[7];assert(!strcmp(viewer->name,"viewer-1"));
    load(viewer->pixels,native_pixels,sizeof(native_pixels));smoke=1;
    char *app_args[]={argv[0],"--battle-assets",argv[1]};assert(!npe_app_main(3,app_args));
    fprintf(stderr,"Actual main viewer SDL commits:%u\n",smoke_frames);
    assert(smoke_frames>=1);compare("actual main viewer",last_pixel_frame,native_pixels,28800);
    fprintf(stderr,"RPG runtime:29 original combined references/58 binding paths/actual main entry and viewer verified\n");return 0;
}
