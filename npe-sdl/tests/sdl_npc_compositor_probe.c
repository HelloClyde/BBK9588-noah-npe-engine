#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct { unsigned id,graphic,first,last,direction,x,y,moving; } NpcState;
typedef struct { const char *name; unsigned hero_y; NpcState before[20],after[20];
#ifdef NPE_MAP_GROUP_RAM
    unsigned npc_base;
#endif
#ifdef NPE_NPC_RESIDENT_RAM
    uint8_t before_bytes[280],after_bytes[280];
#endif
} CompositorCase;
#ifdef NPE_NPC_RESIDENT_RAM
#include "npc_resident_native.h"
#define NPC_REFERENCE_PREFIX "tmp/native-npc-resident-"
#define NPC_ACTUAL_PREFIX "tmp/c-npc-resident-"
#elif defined(NPE_MAP_GROUP_RAM)
#include "map_group_native.h"
#define NPC_REFERENCE_PREFIX "tmp/native-map-group-"
#define NPC_ACTUAL_PREFIX "tmp/c-map-group-"
#elif defined(NPE_WORLD_BITMAP_GUARD)
#include "world_bitmap_guard_native.h"
#define NPC_REFERENCE_PREFIX "tmp/native-world-bitmap-guard-"
#define NPC_ACTUAL_PREFIX "tmp/c-world-bitmap-guard-"
#else
#include "npc_compositor_native.h"
#define NPC_REFERENCE_PREFIX "tmp/native-npc-compositor-"
#define NPC_ACTUAL_PREFIX "tmp/c-npc-compositor-"
#endif
static Uint32 fake_clock;
static unsigned stage,main_case;
static NpeWorld *live;
static int wait_outer(SDL_Event *,int);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static Uint32 ticks(void) { return fake_clock; }
static SDL_Window *create_probe_window(const char *s,int x,int y,int w,int h,Uint32 flags) {
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
#define SDL_GetTicks ticks
#define SDL_WaitEventTimeout wait_outer
#define SDL_CreateWindow create_probe_window
#define npe_sound_select select_sound
#define main npe_app_main
#include "scene_wait_input.h"
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef npe_sound_select
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_GetTicks

static void select_sound(NpeSoundPlayer *p,NpeWorld *w) { live=w;npe_sound_select(p,w); }

#ifndef NPE_NPC_RESIDENT_RAM
static void word(uint8_t *p,unsigned v) { p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v; }
#endif
static void configure(NpeFile *file,NpeWorld *before,NpeWorld *after,const CompositorCase *c) {
#ifdef NPE_NPC_RESIDENT_RAM
    assert(!npe_world_new_game(file,before));before->y=c->hero_y;
    before->view.screen_y=c->hero_y-before->view.offset_y;
    assert(!(npe_rpg_runtime_owner(file)==before?
        npe_rpg_runtime_write(file,0x526ca2,c->before_bytes,280):
        npe_world_resident_write(before,0x526ca2,c->before_bytes,280)));
    *after=*before;assert(!npe_world_resident_write(after,0x526ca2,c->after_bytes,280));
    return;
#else
    NpeMap map;assert(!npe_map(file,0,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *raw;assert(!npe_map_npc(&map,i,&raw));
        uint8_t *p=file->bytes+(raw-file->bytes);const NpcState *n=&c->before[i];
        word(p,n->id);p[2]=(uint8_t)n->x;p[3]=(uint8_t)n->y;word(p+4,n->graphic);
        p[6]=(uint8_t)n->direction;p[7]=(uint8_t)n->moving;word(p+8,n->first);word(p+10,n->last);
    }
    assert(!npe_world_new_game(file,before));before->y=c->hero_y;
    before->view.screen_y=c->hero_y-before->view.offset_y;*after=*before;
    for(unsigned i=0;i<20;++i){const NpcState *n=&c->after[i];
        after->npc_override_x[i]=(uint8_t)n->x;after->npc_override_y[i]=(uint8_t)n->y;
        after->npc_override_direction[i]=(uint8_t)n->direction;
    }
#endif
}
static void read_frame(unsigned index,const char *which,uint8_t *pixels) {
    char name[160];snprintf(name,sizeof(name),NPC_REFERENCE_PREFIX "%u-%s.idx",index,which);
    FILE *f=fopen(name,"rb");assert(f);assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT);
    assert(fgetc(f)==EOF&&!fclose(f));
}
static void compare_renderer(SDL_Renderer *renderer,unsigned index) {
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(index,"after",native);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    #ifdef NPE_NPC_RESIDENT_RAM
    uint8_t records[280];assert(!npe_rpg_runtime_read(live->battle_file,0x526ca2,records,280));
    assert(!memcmp(records,compositor_cases[index].after_bytes,280));
#endif
    unsigned differences=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"main %s stage%u: SDL differences=%u\n",compositor_cases[index].name,stage,differences);assert(!differences);
}
static int wait_outer(SDL_Event *event,int timeout) {
    memset(event,0,sizeof(*event));
    if(!stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;++stage;return 1;}
    if(stage==1){
        assert(live->game_mode==1&&timeout==4000);
        NpeWorld saved=*live,after;NpeFile *file=(NpeFile *)live->battle_file;
        configure(file,live,&after,&compositor_cases[main_case]);
#if defined(NPE_WORLD_BITMAP_GUARD) || defined(NPE_MAP_GROUP_RAM) || defined(NPE_NPC_RESIDENT_RAM)
        file->indexed_bitmap_count=1;
#endif
#ifdef NPE_MAP_GROUP_RAM
        uint8_t base[2]={(uint8_t)(compositor_cases[main_case].npc_base>>8),
                        (uint8_t)compositor_cases[main_case].npc_base};
        assert(!npe_rpg_runtime_write(file,0x527ae6,base,2));
        file->vectors[1]^=100;
#endif
        live->sound_observer=saved.sound_observer;live->sound_context=saved.sound_context;
        live->battle_animation_observer=saved.battle_animation_observer;
        live->battle_animation_context=saved.battle_animation_context;
        live->music_enabled=saved.music_enabled;live->map_music_slot=saved.map_music_slot;
        live->sound_section=saved.sound_section;live->sound_loop=saved.sound_loop;
        read_frame(main_case,"before",world_map_frame);
        world_map_frame_state=*live;world_map_frame_file=file;
        world_map_frame_renderer=last_pixel_renderer;world_map_frame_valid=1;
        fake_clock=4000;++stage;return 0;
    }
    assert((stage==2||stage==3)&&timeout==4000);compare_renderer(last_pixel_renderer,main_case);
    event->type=stage==2?SDL_KEYDOWN:SDL_QUIT;event->key.keysym.sym=SDLK_F12;++stage;return 1;
}
int main(int argc,char **argv) {
    assert(argc==2);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("NPC compositor",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    unsigned failed=0;
    for(unsigned index=0;index<sizeof(compositor_cases)/sizeof(*compositor_cases);++index){
        NpeWorld before,after;uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[sizeof(pixels)];
        configure(&file,&before,&after,&compositor_cases[index]);read_frame(index,"before",pixels);read_frame(index,"after",native);
#if defined(NPE_WORLD_BITMAP_GUARD) || defined(NPE_MAP_GROUP_RAM) || defined(NPE_NPC_RESIDENT_RAM)
        assert(!npe_world_resources_open(&file,&after));
        unsigned saved_count=file.indexed_bitmap_count;file.indexed_bitmap_count=1;
#endif
#ifdef NPE_MAP_GROUP_RAM
        uint16_t saved_base=file.vectors[1];file.vectors[1]^=100;
        uint8_t base[2]={(uint8_t)(compositor_cases[index].npc_base>>8),
                        (uint8_t)compositor_cases[index].npc_base};
        assert(!npe_rpg_runtime_write(&file,0x527ae6,base,2));
#endif
        int result=npe_render_npc_periodic(&file,&before,&after,pixels);
        unsigned diff=0;for(unsigned i=0;i<sizeof(pixels);++i)diff+=pixels[i]!=native[i];
        fprintf(stderr,"%s: result=%d C differences=%u",compositor_cases[index].name,result,diff);
        draw_pixels(renderer,texture,pixels);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
        assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
        unsigned sdl_diff=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
            sdl_diff+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
        fprintf(stderr," SDL differences=%u\n",sdl_diff);failed+=result!=0||diff||sdl_diff;
        char output[160];snprintf(output,sizeof(output),NPC_ACTUAL_PREFIX "%u.idx",index);
        FILE *f=fopen(output,"wb");assert(f&&fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels)&&!fclose(f));
#if defined(NPE_WORLD_BITMAP_GUARD) || defined(NPE_MAP_GROUP_RAM) || defined(NPE_NPC_RESIDENT_RAM)
        assert(!npe_rpg_runtime_failures(&file));
        npe_rpg_runtime_close(&file);file.indexed_bitmap_count=saved_count;
#endif
#ifdef NPE_MAP_GROUP_RAM
        file.vectors[1]=saved_base;
#endif
    }
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    if(!failed)for(main_case=0;main_case<sizeof(compositor_cases)/sizeof(*compositor_cases);++main_case){
        int moving=0;for(unsigned i=0;i<20;++i)moving|=compositor_cases[main_case].before[i].moving==1;
        if(moving)continue; /* Full main movement/RNG is covered by sdl-npc-periodic-probe. */
        fake_clock=stage=0;char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==4);
    }
    printf("NPC compositor: %zu direct C/SDL paths and public main redraws passed\n",sizeof(compositor_cases)/sizeof(*compositor_cases));
    return failed?1:0;
}
