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
typedef struct { unsigned graphic,first,last,enemy,x,y,cooldown; } ChaseState;
typedef struct {
    const char *name;unsigned hero_y,mode;uint32_t seed,rng_after;
    NpcState before[20],after[20];ChaseState chase_before[2],chase_after[2];
    uint16_t cache_tail[15];
#ifdef NPE_CHASE_RESIDENT_RAM
    uint8_t npc_before_bytes[280],npc_after_bytes[280],chase_before_bytes[28],chase_after_bytes[28];
#endif
} ChaseCase;
#ifdef NPE_CHASE_RESIDENT_RAM
#include "chase_resident_native.h"
#define CHASE_REFERENCE_PREFIX "tmp/native-chase-resident"
#define CHASE_OUTPUT_PREFIX "tmp/c-chase-resident"
#elif defined(NPE_CHASE_MARGIN_MATRIX)
#include "chase_margin_native.h"
#define CHASE_REFERENCE_PREFIX "tmp/native-chase-margin"
#define CHASE_OUTPUT_PREFIX "tmp/c-chase-margin"
#else
#include "chase_compositor_native.h"
#define CHASE_REFERENCE_PREFIX "tmp/native-chase-compositor"
#define CHASE_OUTPUT_PREFIX "tmp/c-chase-compositor"
#endif
static Uint32 fake_clock;
static unsigned stage,main_case,periodic_commit;
static NpeWorld *live;
static int wait_outer(SDL_Event *,int);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static void present(SDL_Renderer *);
static Uint32 ticks(void) { return fake_clock; }
static SDL_Window *create_probe_window(const char *s,int x,int y,int w,int h,Uint32 flags) {
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
#define SDL_GetTicks ticks
#define SDL_WaitEventTimeout wait_outer
#define SDL_CreateWindow create_probe_window
#define SDL_RenderPresent present
#define npe_sound_select select_sound
#define main npe_app_main
#include "scene_wait_input.h"
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef npe_sound_select
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_GetTicks

static void select_sound(NpeSoundPlayer *p,NpeWorld *w) {
    live=w;if(stage>=2)w->music_enabled=0;npe_sound_select(p,w);
}
#ifndef NPE_CHASE_RESIDENT_RAM
static unsigned get_word(const uint8_t *p) { return (unsigned)p[0]*256+p[1]; }
static void word(uint8_t *p,unsigned v) { p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v; }
static void dword(uint8_t *p,unsigned v) { word(p,v>>16);word(p+2,v); }
static void add_chase_directory(NpeFile *file) {
    NpeMap map;const uint8_t *raw;size_t record_size;
    assert(!npe_map(file,0,&map)&&!npe_record(file,8,0,&raw,&record_size));
    assert(map.tail[652+10*3+2]==0);
    size_t raw_offset=(size_t)(raw-file->bytes),tail_offset=(size_t)(map.tail-raw);
    unsigned directory_base=get_word(map.tail+652);
    size_t insert=tail_offset+703+get_word(map.tail+652+10*3)-directory_base;
    size_t old_size=file->size,new_size=old_size+4+record_size+20;
    uint8_t *bytes=realloc(file->bytes,new_size);assert(bytes);file->bytes=bytes;
    uint8_t *copy=bytes+old_size+4;raw=bytes+raw_offset;
    dword(bytes+old_size,4);memcpy(copy,raw,insert);memset(copy+insert,0,20);
    memcpy(copy+insert+20,raw+insert,record_size-insert);
    copy[tail_offset+652+10*3+2]=2;
    for(unsigned i=11;i<17;++i){uint8_t *p=copy+tail_offset+652+i*3;word(p,get_word(p)+20);}
    /* Diagnostic in-memory map section: keep original resources; insert two
       ordinary current-NPE chase rows in an isolated copy of the map record. */
    file->sections[8].offset=(uint32_t)old_size;
    file->sections[8].size=(uint32_t)(4+record_size+20);
    file->offset_table_records[8]=1;file->size=new_size;
}
#else
static void word(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
#endif
static void configure(NpeFile *file,NpeWorld *world,unsigned index) {
    const ChaseCase *c=&chase_cases[index];
#ifndef NPE_CHASE_RESIDENT_RAM
    NpeMap map;assert(!npe_map(file,0,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *raw;assert(!npe_map_npc(&map,i,&raw));
        uint8_t *p=file->bytes+(raw-file->bytes);const NpcState *n=&c->before[i];
        word(p,n->id);p[2]=(uint8_t)n->x;p[3]=(uint8_t)n->y;word(p+4,n->graphic);
        p[6]=(uint8_t)n->direction;p[7]=(uint8_t)n->moving;word(p+8,n->first);word(p+10,n->last);
    }
    for(unsigned i=0;i<2;++i){NpeMapChase row;assert(npe_map_chase(&map,i,&row)==1);
        uint8_t *p=file->bytes+(row.raw-file->bytes);const ChaseState *n=&c->chase_before[i];
        word(p,n->graphic);word(p+2,n->first);word(p+4,n->last);word(p+6,n->enemy);p[8]=(uint8_t)n->x;p[9]=(uint8_t)n->y;
    }
#endif
    assert(!npe_world_new_game(file,world));world->y=c->hero_y;
#ifdef NPE_CHASE_RESIDENT_RAM
    uint64_t fingerprint=npe_game_fingerprint(file);
    int attached=npe_rpg_runtime_owner(file)==world;
    assert(!(attached?npe_rpg_runtime_write(file,0x526ca2,c->npc_before_bytes,280):
                      npe_world_resident_write(world,0x526ca2,c->npc_before_bytes,280)));
    assert(!(attached?npe_rpg_runtime_write(file,0x52783a,c->chase_before_bytes,28):
                      npe_world_resident_write(world,0x52783a,c->chase_before_bytes,28)));
    assert(fingerprint==npe_game_fingerprint(file));
#endif
    world->view.screen_y=c->hero_y-world->view.offset_y;world->music_enabled=0;npe_world_seed(world,c->seed);
    for(unsigned i=0;i<2;++i){world->chase_x[i]=(uint16_t)c->chase_before[i].x;
        world->chase_y[i]=(uint16_t)c->chase_before[i].y;world->chase_cooldown[i]=(uint8_t)c->chase_before[i].cooldown;}
    for(unsigned i=0;i<15;++i)word(world->condition5_bytes+i*2,c->cache_tail[i]);
}
static void records(const NpeFile *file,const NpeWorld *world,unsigned index) {
#ifdef NPE_CHASE_RESIDENT_RAM
    uint8_t npc_bytes[280],chase_bytes[28];
    assert(!npe_world_resident_read(world,0x526ca2,npc_bytes,280));
    assert(!npe_world_resident_read(world,0x52783a,chase_bytes,28));
    assert(!memcmp(npc_bytes,chase_cases[index].npc_after_bytes,280));
    assert(!memcmp(chase_bytes,chase_cases[index].chase_after_bytes,28));
#endif
    const ChaseCase *c=&chase_cases[index];NpeMap map;assert(!npe_map(file,0,&map));
    assert(world->random_state==c->rng_after&&world->game_mode==c->mode);
    for(unsigned i=0;i<20;++i){const uint8_t *raw;unsigned x,y;assert(!npe_map_npc(&map,i,&raw));
        npe_world_npc_position(world,raw,i,&x,&y);unsigned d=world->npc_override_direction[i]<=3?world->npc_override_direction[i]:raw[6];
        assert(x==c->after[i].x&&y==c->after[i].y&&d==c->after[i].direction);}
    for(unsigned i=0;i<2;++i){const ChaseState *n=&c->chase_after[i];
        assert(world->chase_x[i]==n->x&&world->chase_y[i]==n->y&&world->chase_cooldown[i]==n->cooldown);}
}
static void read_frame(unsigned index,const char *which,uint8_t *pixels) {
    char name[160];snprintf(name,sizeof(name),CHASE_REFERENCE_PREFIX "-%u-%s.idx",index,which);
    FILE *f=fopen(name,"rb");assert(f);assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT);
    assert(fgetc(f)==EOF&&!fclose(f));
}
static void compare_renderer(SDL_Renderer *renderer,unsigned index) {
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(index,"commit-0",native);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"main %s: SDL differences=%u\n",chase_cases[index].name,differences);assert(!differences);
}
static void present(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);
    if(stage==2&&chase_cases[main_case].mode==3&&!periodic_commit){
        records(live->battle_file,live,main_case);compare_renderer(renderer,main_case);++periodic_commit;
    }
}
static int wait_outer(SDL_Event *event,int timeout) {
    memset(event,0,sizeof(*event));
    if(!stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;++stage;return 1;}
    if(stage==1){
        assert(live->game_mode==1&&timeout==4000);NpeWorld saved=*live;
        NpeFile *file=(NpeFile *)live->battle_file;
#ifndef NPE_CHASE_RESIDENT_RAM
        add_chase_directory(file);
#endif
        configure(file,live,main_case);
        live->sound_observer=saved.sound_observer;live->sound_context=saved.sound_context;
        live->battle_animation_observer=saved.battle_animation_observer;
        live->battle_animation_context=saved.battle_animation_context;
        read_frame(main_case,"before",world_map_frame);world_map_frame_state=*live;world_map_frame_file=file;
        world_map_frame_renderer=last_pixel_renderer;world_map_frame_valid=1;
        fake_clock=4000;++stage;return 0;
    }
    assert(stage==2&&timeout==4000);records(live->battle_file,live,main_case);
    if(chase_cases[main_case].mode==3)assert(periodic_commit==1);
    else compare_renderer(last_pixel_renderer,main_case);
    ++stage;event->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv) {
    assert(argc==2);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
#ifndef NPE_CHASE_RESIDENT_RAM
    add_chase_directory(&file);
#endif
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("chase compositor",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    unsigned failed=0;uint8_t matching[sizeof(chase_cases)/sizeof(*chase_cases)]={0};
    for(unsigned i=0;i<sizeof(chase_cases)/sizeof(*chase_cases);++i){
        NpeWorld before,after;uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[sizeof(pixels)];
        configure(&file,&before,i);after=before;
#ifdef NPE_CHASE_RESIDENT_RAM
        assert(!npe_world_resources_open(&file,&after));
#endif
        assert(npe_world_update_chase(&file,&after)>=0&&npe_world_update_npcs(&file,&after)>=0);records(&file,&after,i);
        read_frame(i,"before",pixels);read_frame(i,"commit-0",native);
        int result=npe_render_world_periodic(&file,&before,&after,pixels);
        unsigned diff=0;for(unsigned k=0;k<sizeof(pixels);++k)diff+=pixels[k]!=native[k];
        fprintf(stderr,"%s: result=%d C differences=%u\n",chase_cases[i].name,result,diff);
        failed+=result!=0||diff;
        matching[i]=(uint8_t)(result==0&&!diff);
        if(!diff&&!result){draw_pixels(renderer,texture,pixels);compare_renderer(renderer,i);}
#ifdef NPE_CHASE_RESIDENT_RAM
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
#endif
        char name[100];snprintf(name,sizeof(name),CHASE_OUTPUT_PREFIX "-%u.idx",i);
        FILE *f=fopen(name,"wb");assert(f&&fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels)&&!fclose(f));
    }
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    for(main_case=0;main_case<sizeof(chase_cases)/sizeof(*chase_cases);++main_case){
        if(!matching[main_case])continue;
        fake_clock=stage=periodic_commit=0;char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==3);
    }
    fprintf(stderr,"Chase periodic unresolved full frames: %u\n",failed);
    return failed?1:0;
}
