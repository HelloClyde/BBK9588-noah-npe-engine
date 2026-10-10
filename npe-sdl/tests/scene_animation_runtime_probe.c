#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#ifdef NPE_SCENE_CONTROLLER
static Uint32 controller_clock;
static Uint32 controller_ticks(void){return controller_clock;}
static void controller_delay(Uint32 n){controller_clock+=n;}
static int controller_poll(SDL_Event *event){(void)event;return 0;}
static void controller_present(SDL_Renderer *);
#define SDL_GetTicks controller_ticks
#define SDL_Delay controller_delay
#define SDL_PollEvent controller_poll
#define SDL_RenderPresent controller_present
#endif
#define main npe_app_main
#include "../main.c"
#undef main
#ifdef NPE_SCENE_CONTROLLER
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_RenderPresent
#endif
#ifndef NPE_SCENE_FIXTURE_HEADER
#define NPE_SCENE_FIXTURE_HEADER "scene_animation_runtime_native.h"
#endif
#include NPE_SCENE_FIXTURE_HEADER
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
#ifdef NPE_SCENE_CONTROLLER
static const SceneRuntimeCase *controller_case;
static unsigned controller_frames;
static void controller_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);
    if(!controller_case)return;
    assert(controller_frames<controller_case->frames);
    load(controller_case->references[controller_frames].pixels,expected_pixels,sizeof(expected_pixels));
    compare("Opening controller pixels",last_pixel_frame,expected_pixels,sizeof(expected_pixels));
    uint32_t rgba[28800];assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    for(unsigned p=0;p<28800;++p){unsigned v=expected_pixels[p];
        assert(rgba[p]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    ++controller_frames;
}
#endif
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
    assert((argc==2||argc==3)&&!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    SDL_Window *window=SDL_CreateWindow("Original scene stream",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    unsigned checked=0;
    for(unsigned c=0;c<sizeof(scene_runtime_cases)/sizeof(*scene_runtime_cases);++c){
        const SceneRuntimeCase *test=&scene_runtime_cases[c];
        for(unsigned binding=0;binding<2;++binding){
            memset(&world,0,sizeof(world));world.battle_record_party_count=3;world.battle_party_flags_saved=1;
            assert(!(binding?npe_world_resources_open(&file,&world):npe_rpg_runtime_open(&file,NULL)));
            /* Optional original pre-call RAM is an isolated service input,
               captured before any measured frame or result was produced. */
            if(argc==3){
                load(argv[2],ram,sizeof(ram));
                assert(!npe_rpg_runtime_write(&file,UINT32_C(0x510000),ram,sizeof(ram)));
                assert(!npe_rpg_runtime_read(&file,UINT32_C(0x510000),expected_ram,sizeof(expected_ram)));
                compare("Original service input",ram,expected_ram,sizeof(ram));
            }
            const uint32_t addresses[]={0x5279f6,0x5279fa},values[]={test->directory,test->payload};
            for(unsigned i=0;i<2;++i){uint32_t v=values[i];uint8_t data[4]={(uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v};
                assert(!npe_rpg_runtime_write(&file,addresses[i],data,4));}
            memset(pixels,0,sizeof(pixels));int result=npe_rpg_runtime_scene_begin(&file,test->group);
            if(test->frames){
                assert(!result && npe_rpg_runtime_scene_frames(&file)==test->frames);
                assert(npe_rpg_runtime_scene_begin(&file,test->group)==-1);
                for(unsigned i=0;i<test->frames;++i){
                    assert(!npe_render_scene_animation_frame(&file,(uint16_t)(test->group-1),i,(uint16_t)test->x,(uint16_t)test->y,test->kind,pixels));
                    const SceneRuntimeFrame *frame=&test->references[i];
                    check(&file,test->name,frame->ram,frame->pixels,frame->trace,8);
                    draw_pixels(renderer,texture,pixels);uint32_t rgba[28800];
                    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
                    for(unsigned p=0;p<28800;++p){unsigned v=expected_pixels[p];
                        assert(rgba[p]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
                    ++checked;
                }
                assert(npe_rpg_runtime_scene_frame(&file,0,0,0,pixels)==-1);
                npe_rpg_runtime_scene_end(&file);
            }
            uint8_t trace[10];load(test->trace,trace,10);
            assert((uint16_t)result==((unsigned)trace[8]<<8|trace[9]));
            check(&file,test->name,test->ram,test->pixels,test->trace,10);
            assert(!npe_rpg_runtime_scene_frames(&file));npe_rpg_runtime_scene_end(&file);
            npe_rpg_runtime_close(&file);
            fprintf(stderr,"Scene stream %s binding%u: full RAM/pixels/IO/status match\n",test->name,binding);
        }
    }
    unsigned cases=(unsigned)(sizeof(scene_runtime_cases)/sizeof(*scene_runtime_cases));
    fprintf(stderr,"Scene stream:%u original cases/%u binding paths/%u actual SDL frames matched\n",cases,2u*cases,checked);
#ifdef NPE_SCENE_CONTROLLER
    /* Normal C initialization, rather than the isolated pre-call RAM above.
       Exercise the production stream opener, full frame loop and release. */
    assert(cases==1);const SceneRuntimeCase *test=&scene_runtime_cases[0];
    memset(&world,0,sizeof(world));world.battle_file=&file;
    assert(!npe_world_resources_open(&file,&world));last_pixel_renderer=NULL;
    controller_case=test;controller_frames=0;
    assert(play_scene_group(renderer,texture,&file,&world,(uint16_t)test->group,
        (uint16_t)test->x,(uint16_t)test->y,test->kind,0)==1);
    assert(controller_frames==test->frames&&!npe_rpg_runtime_scene_frames(&file));
    controller_case=NULL;npe_rpg_runtime_close(&file);
    fprintf(stderr,"Production opening controller:%u original C/SDL frames matched; stream released\n",controller_frames);
#endif
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_free(&file);SDL_Quit();return 0;
}
