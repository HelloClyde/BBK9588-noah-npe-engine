#ifndef NPE_MAP_MENU_IGNORED_INPUT_H
#define NPE_MAP_MENU_IGNORED_INPUT_H
#include <stdlib.h>

/* These pages use the original item/magic dispatchers. Inject unsupported
   keys before each existing reference input without changing its oracle. */
static unsigned map_ignored_index,map_ignored_pending,map_ignored_total;
static NpeWorld map_ignored_world;
static uint8_t map_ignored_pixels[NPE_WIDTH*NPE_HEIGHT];
static const NpeFile *map_ignored_file;
static uint64_t map_ignored_reads;

static void map_menu_ignored_reset(const NpeFile *file){
    assert(!map_ignored_pending);
    map_ignored_index=0;map_ignored_file=file;
}
static void map_menu_ignored_present(void){
    if(map_ignored_pending){
        fprintf(stderr,"Unexpected LCD on ignored map menu key %u\n",map_ignored_index);
        assert(0);
    }
}
static int map_menu_ignored_input(SDL_Event *event,const NpeWorld *world){
    static const SDL_Keycode keys[]={SDLK_SPACE,SDLK_q,SDLK_u,SDLK_e,SDLK_d,SDLK_f,SDLK_TAB};
    const char *enabled=getenv("NPE_PROBE_MAP_MENU_IGNORED_KEYS");
    const char *variant=getenv("NPE_PROBE_BATTLE_KEYS");
    if(!enabled||!*enabled)return 0;
    if(map_ignored_pending){
        assert(!memcmp(&map_ignored_world,world,sizeof(*world)));
        assert(!memcmp(map_ignored_pixels,last_pixel_frame,sizeof(map_ignored_pixels)));
        assert(map_ignored_reads==npe_rpg_runtime_reads(map_ignored_file));
        map_ignored_pending=0;++map_ignored_total;
    }
    if(map_ignored_index==sizeof(keys)/sizeof(*keys)){map_ignored_index=0;return 0;}
    memcpy(&map_ignored_world,world,sizeof(*world));
    memcpy(map_ignored_pixels,last_pixel_frame,sizeof(map_ignored_pixels));
    map_ignored_reads=npe_rpg_runtime_reads(map_ignored_file);
    memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;
    event->key.keysym.sym=keys[map_ignored_index++];
    if(variant&&!strcmp(variant,"shift"))event->key.keysym.mod=KMOD_SHIFT;
    if(variant&&!strcmp(variant,"caps"))event->key.keysym.mod=KMOD_CAPS;
    map_ignored_pending=1;return 1;
}
static void map_menu_ignored_report(void){
    assert(!map_ignored_pending);
    if(map_ignored_total)printf("Map menu ignored inputs:%u, no World change, resource read or LCD submit\n",map_ignored_total);
}
#endif
