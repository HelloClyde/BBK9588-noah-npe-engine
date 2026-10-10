#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
static int description_wait(SDL_Event *);
static void description_present(SDL_Renderer *);
#define SDL_WaitEvent description_wait
#define SDL_RenderPresent description_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#include "map_description_probe_helpers.h"
static const MapDescriptionCase *current;
static NpeWorld *active;
static unsigned phase,frames,before_frames,quits;
static int compare_enabled;
static void description_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!compare_enabled)return;
    const MapDescriptionState *state;int index;
    if(phase==UINT_MAX){assert(current->skill&&!before_frames++);state=&current->before;index=-1;}
    else{const MapDescriptionPhase *p=current->phases+phase;assert(p->count&&p->index==frames);state=&p->frame;index=(int)frames++;}
    unsigned bad=map_description_differences(active,state);fprintf(stderr,"SDL %s phase%u state differences%u\n",current->name,phase,bad);assert(!bad);
    uint8_t native[28800];uint32_t rgba[28800];map_description_frame(current->name,index,native);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));bad=0;
    for(unsigned i=0;i<28800;++i){unsigned v=native[i];bad+=last_pixel_frame[i]!=v;bad+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL %s index%d differences%u\n",current->name,index,bad);
    if(bad){FILE *out=fopen("tmp/map-description-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,28800,out)==28800&&!fclose(out));}
    assert(!bad);
}
static int description_wait(SDL_Event *event){
    if(phase!=UINT_MAX)assert(!map_description_differences(active,&current->phases[phase].after));
    memset(event,0,sizeof(*event));
    if(phase!=UINT_MAX&&phase+1==current->count){assert(frames==current->commits);event->type=SDL_QUIT;++quits;return 1;}
    phase=phase==UINT_MAX?0:phase+1;const MapDescriptionPhase *p=current->phases+phase;map_description_patch(active,p);event->type=SDL_KEYDOWN;
    event->key.keysym.sym=p->key==16?SDLK_SPACE:p->key==0x85?SDLK_n:SDLK_p;return 1;
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("map descriptions",0,0,240,120,0);assert(window);SDL_Renderer *r=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(r);
    SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(t);
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    NpeWorld world;active=&world;unsigned total=0,initial_total=0;
    for(unsigned c=0;c<sizeof(map_description_cases)/sizeof(*map_description_cases);++c){current=map_description_cases+c;assert(!npe_world_new_game(&file,&world));map_description_seed(&world,&current->before);
        for(unsigned i=0;i<20&&current->npc[i][0];++i){world.npc_override_x[i]=(uint8_t)current->npc[i][1];world.npc_override_y[i]=(uint8_t)current->npc[i][2];world.npc_override_direction[i]=(uint8_t)current->npc[i][3];}
        assert(!npe_world_resources_open(&file,&world));
        frames=before_frames=quits=0;compare_enabled=0;system_menu_background_valid=system_menu_frame_valid=world_map_frame_valid=0;
        unsigned levels[3]={0};uint8_t base[28800];assert(!npe_render_world_load(&file,&world,base));draw_pixels(r,t,base);retain_world_frame(&file,&world,r,base,0);
        if(current->skill)assert(!draw_world(r,t,&file,&world,1,0,0,0,0,0,levels,0,&font));
        uint8_t bitmap[32],after[32];assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32));
        phase=current->skill?UINT_MAX:0;compare_enabled=1;
        if(current->skill)show_magic_list(window,r,t,&file,&font,&world);else assert(!show_use_item(window,r,t,&file,&font,&world,0));
        compare_enabled=0;assert(quits==1&&frames==current->commits&&before_frames==current->skill);
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,after,32)&&!memcmp(bitmap,after,32));total+=frames;initial_total+=before_frames;
        npe_rpg_runtime_close(&file);SDL_Event event;while(SDL_PollEvent(&event)){}
    }
    printf("actual main map descriptions: %u original page frames/states, %u original caster picker frames, host quit passed\n",total,initial_total);
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
