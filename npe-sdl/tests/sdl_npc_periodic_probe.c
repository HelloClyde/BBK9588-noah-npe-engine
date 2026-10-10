#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct { unsigned id,graphic,first,last,direction,x,y,moving; } NpcState;
typedef struct {
    const char *name;
    uint32_t seed,rng_after;
    NpcState before[20],after[20];
    unsigned objects[8][2],exits[12][2];
} NpcCase;
#include "npc_periodic_native.h"
static Uint32 fake_clock;
static unsigned stage;
static NpeWorld *live;
static Uint32 ticks(void) { return fake_clock; }
static int wait_outer(SDL_Event *,int);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static void seed(NpeWorld *,uint32_t);
static SDL_Window *create_probe_window(const char *s,int x,int y,int w,int h,Uint32 flags) {
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
#define SDL_GetTicks ticks
#define SDL_WaitEventTimeout wait_outer
#define SDL_CreateWindow create_probe_window
#define npe_sound_select select_sound
#define npe_world_seed seed
#define main npe_app_main
#include "scene_wait_input.h"
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef npe_world_seed
#undef npe_sound_select
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_GetTicks

static void select_sound(NpeSoundPlayer *p,NpeWorld *w) {
    live=w;
    /* Let a pending deadline expire between ignored-input draw policy
       and the timer pass; the original periodic LCD must still appear. */
    if(stage==2 && getenv("NPE_PROBE_IGNORED_DEADLINE") && !fake_clock)fake_clock=4000;
    npe_sound_select(p,w);
}
static void seed(NpeWorld *w,uint32_t unused) { (void)unused;npe_world_seed(w,npc_cases[0].seed); }
static void write_word(uint8_t *p,unsigned value) { p[0]=(uint8_t)(value>>8);p[1]=(uint8_t)value; }
static void setup(NpeFile *file,NpeWorld *w,unsigned scenario) {
    NpeMap map;assert(!npe_map(file,0,&map));const NpcCase *c=&npc_cases[scenario];
    for(unsigned i=0;i<20;++i){
        const uint8_t *raw;assert(!npe_map_npc(&map,i,&raw));uint8_t *r=file->bytes+(raw-file->bytes);
        const NpcState *n=&c->before[i];write_word(r,n->id);r[2]=(uint8_t)n->x;r[3]=(uint8_t)n->y;
        write_word(r+4,n->graphic);r[6]=(uint8_t)n->direction;r[7]=(uint8_t)n->moving;
        write_word(r+8,n->first);write_word(r+10,n->last);
    }
    assert(!npe_world_new_game(file,w));npe_world_seed(w,c->seed);
    for(unsigned i=0;i<8;++i){uint8_t xy[]={(uint8_t)c->objects[i][0],(uint8_t)c->objects[i][1]};
        assert(!(npe_rpg_runtime_owner(file)==w?npe_rpg_runtime_write(file,0x52759e +i*8+6,xy,2):
                    npe_world_resident_write(w,0x52759e +i*8+6,xy,2)));}
    /* Exit collision uses the already loaded resident slots. Do not make
       a file edit stand in for changing those live coordinates. */
    for(unsigned i=0;i<12;++i){uint8_t xy[]={(uint8_t)c->exits[i][0],(uint8_t)c->exits[i][1]};
        assert(!(npe_rpg_runtime_owner(file)==w?npe_rpg_runtime_write(file,0x526b82+i*24,xy,2):
                    npe_world_resident_write(w,0x526b82+i*24,xy,2)));}
}
static void records(const NpeFile *file,const NpeWorld *w,unsigned scenario) {
    NpeMap map;assert(!npe_map(file,0,&map));const NpcCase *c=&npc_cases[scenario];
    assert(w->random_state==c->rng_after);
    for(unsigned i=0;i<20;++i){const uint8_t *raw;unsigned x,y;assert(!npe_map_npc(&map,i,&raw));
        npe_world_npc_position(w,raw,i,&x,&y);unsigned d=w->npc_override_direction[i]<=3?w->npc_override_direction[i]:raw[6];
        if(x!=c->after[i].x||y!=c->after[i].y||d!=c->after[i].direction)
            fprintf(stderr,"%s slot%u C=%u,%u,%u native=%u,%u,%u\n",c->name,i,x,y,d,c->after[i].x,c->after[i].y,c->after[i].direction);
        assert(x==c->after[i].x&&y==c->after[i].y&&d==c->after[i].direction);
        const NpcState *n=&c->after[i];uint8_t resident[12];
        const uint8_t expected[12]={(uint8_t)(n->id>>8),(uint8_t)n->id,
            (uint8_t)(n->graphic>>8),(uint8_t)n->graphic,
            (uint8_t)(n->first>>8),(uint8_t)n->first,
            (uint8_t)(n->last>>8),(uint8_t)n->last,
            (uint8_t)n->direction,(uint8_t)n->x,(uint8_t)n->y,(uint8_t)n->moving};
        assert(!npe_world_resident_read(w,0x526ca2+14*i,resident,sizeof(resident)));
        assert(!memcmp(resident,expected,sizeof(resident)));
    }
}
static void frame(uint8_t *pixels) {
    FILE *f=fopen("tmp/native-npc-periodic-0.idx","rb");assert(f);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT&&fgetc(f)==EOF&&!fclose(f));
}
static void compare_sdl(SDL_Renderer *renderer) {
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];frame(native);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL periodic map: %u differences\n",differences);assert(!differences);
}
static int wait_outer(SDL_Event *event,int timeout) {
    memset(event,0,sizeof(*event));
    if(stage==0){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;++stage;return 1;}
    if(stage==1){
        /* Audio pumping may split the logical four-second wait. */
        assert(live->game_mode==1&&timeout>0&&timeout<=4000&&!fake_clock);
        /* Set the sampled pre-event resident state after map music has
           drawn its random slot. Startup seeding is a separate pending task. */
        npe_world_seed(live,npc_cases[0].seed);
        ++stage;
        if(getenv("NPE_PROBE_IGNORED_DEADLINE")){
            event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_q;return 1;
        }
        fake_clock=4000;return 0;
    }
    assert(stage==2&&timeout>0&&timeout<=4000&&fake_clock==4000);records(live->battle_file,live,0);compare_sdl(last_pixel_renderer);
    ++stage;event->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv) {
    assert(argc==2);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    uint8_t *original=malloc(size);assert(original);memcpy(original,bytes,size);
    for(unsigned scenario=0;scenario<sizeof(npc_cases)/sizeof(*npc_cases);++scenario){
        memcpy(bytes,original,size);NpeWorld world;setup(&file,&world,scenario);
        assert(!npe_world_resources_open(&file,&world));
        assert(npe_world_update_npcs(&file,&world)>=0);records(&file,&world,scenario);
        if(!scenario){
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[sizeof(pixels)];frame(native);
            assert(!npe_render_world(&file,&world,pixels));unsigned differences=0;
            for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=native[i];
            fprintf(stderr,"C periodic map: %u differences\n",differences);assert(!differences);
            assert(!npe_world_save(&file,&world,"tmp/npc-periodic-probe.sav"));
            NpeWorld loaded;assert(!npe_world_load(&file,&loaded,"tmp/npc-periodic-probe.sav"));records(&file,&loaded,scenario);
            assert(!remove("tmp/npc-periodic-probe.sav"));
        }
        fprintf(stderr,"NPC native case %s passed\n",npc_cases[scenario].name);
        npe_rpg_runtime_close(&file);
    }
    memcpy(bytes,original,size);free(original);NpeWorld world;setup(&file,&world,0);
    Uint32 deadline=4000;uint32_t initial=world.random_state;
    assert(!game_world_timer(&file,&world,3999,&deadline,0)&&world.random_state==initial);
    assert(!game_world_timer(&file,&world,4000,&deadline,1)&&world.random_state==initial&&deadline==8000);
    world.game_mode=6;
    assert(!game_world_timer(&file,&world,8000,&deadline,0)&&world.random_state==initial&&deadline==12000);
    world.game_mode=1;assert(!game_world_timer(&file,&world,12000,&deadline,0));records(&file,&world,0);
    deadline=0xfffffff0u;initial=world.random_state;
    assert(!game_world_timer(&file,&world,0x10,&deadline,1)&&deadline==4016&&world.random_state==initial);
    setup(&file,&world,0);deadline=100;
    assert(!game_world_timer(&file,&world,40100,&deadline,0)&&deadline==44100);
    records(&file,&world,0);initial=world.random_state;
    assert(!game_world_timer(&file,&world,40100,&deadline,0)&&world.random_state==initial);
    npe_free(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    fake_clock=stage=0;char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==3);
    puts("Native NPC positions/directions/shared RNG, save roundtrip, modal/timer gates and actual SDL timed map passed");return 0;
}
