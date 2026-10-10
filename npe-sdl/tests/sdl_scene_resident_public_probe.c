#include "../npe.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned id,x,y,direction;} SceneNpc;
typedef struct {unsigned mode,event,map,progress,init,sx,sy,vx,vy,direction;SceneNpc npc[20];} SceneState;
typedef struct {const char *name;unsigned commits,service_commits;SceneState before,after;uint8_t records[80],after_records[80];} SceneResidentCase;
#include "scene_resident_public_native.h"
static unsigned active,case_index,submits,total;
static Uint32 fake_clock;
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *e){(void)e;return 0;}
static void probe_present(SDL_Renderer *);
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_RenderPresent probe_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_RenderPresent
static void read_frame(unsigned before,unsigned index,uint8_t *pixels){
    char path[180];const char *name=scene_resident_cases[case_index].name;
    if(before)snprintf(path,sizeof(path),"tmp/native-scene-resident-public-%s-before.idx",name);
    else snprintf(path,sizeof(path),"tmp/native-scene-resident-public-%s-%u.idx",name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const SceneResidentCase *c=&scene_resident_cases[case_index];
    if(submits>=c->commits){fprintf(stderr,"scene %s unexpected C commit%u;original count%u\n",c->name,submits,c->commits);assert(0);}
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"scene %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);assert(!d&&!sd);
    char path[180];snprintf(path,sizeof(path),"tmp/c-scene-resident-public-%s-%u.idx",c->name,submits);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));
    ++submits;++total;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];NpeWorld baseline,w;NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!npe_world_new_game(&f,&baseline));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("scene resident",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    for(case_index=0;case_index<12;++case_index){const SceneResidentCase *c=&scene_resident_cases[case_index];const SceneState *s=&c->before;
        w=baseline;w.game_mode=15;w.story_event_id=(uint16_t)s->event;w.map_init_pending=(uint8_t)s->init;
        w.story_progress=(uint16_t)s->progress;w.direction=(uint8_t)s->direction;
        w.view.screen_x=s->sx;w.view.screen_y=s->sy;w.view.offset_x=s->vx;w.view.offset_y=s->vy;w.x=s->sx+s->vx;w.y=s->sy+s->vy;
        for(unsigned i=0;i<20&&s->npc[i].id;++i){w.npc_override_x[i]=(uint16_t)s->npc[i].x;w.npc_override_y[i]=(uint16_t)s->npc[i].y;
            w.npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
        assert(!npe_world_resources_open(&f,&w)&&!npe_rpg_runtime_write(&f,0x52754e,c->records,80));
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);active=0;draw_pixels(renderer,texture,pixels);
        save_map_edges(&f,renderer,pixels);retain_world_frame(&f,&w,renderer,pixels,1);submits=0;active=1;
        assert(advance_world_events(window,renderer,texture,&f,&font,&w,15,0)>=0);active=0;
        assert(submits==c->service_commits&&w.game_mode==c->after.mode&&w.story_event_id==c->after.event&&w.map_init_pending==c->after.init);
        active=1;unsigned levels[3]={0};assert(npe_world_open_system_menu(&w)==1);
        assert(!draw_world(renderer,texture,&f,&w,0,0,0,0,0,0,levels,0,&font));
        assert(npe_world_close_system_menu(&w)==1);
        assert(!restore_system_menu_background(&f,&w,renderer,texture));active=0;
        assert(submits==c->commits);
        uint8_t records[80];assert(!npe_rpg_runtime_read(&f,0x52754e,records,80)&&!memcmp(records,c->after_records,80));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
    }
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&f);
    printf("Scene resident:12 original paths,%u C/SDL commits,80 end-table bytes per path passed\n",total);return 0;
}
