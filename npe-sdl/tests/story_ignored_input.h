#ifndef NPE_STORY_IGNORED_INPUT_H
#define NPE_STORY_IGNORED_INPUT_H
#include <stdlib.h>

static unsigned story_ignored_index,story_ignored_pending,story_ignored_total;
static NpeWorld story_ignored_world;
static uint8_t story_ignored_pixels[NPE_WIDTH*NPE_HEIGHT];
static uint64_t story_ignored_reads;

static void story_ignored_reset(void){assert(!story_ignored_pending);story_ignored_index=0;}
static void story_ignored_present(void){
    if(story_ignored_pending){fprintf(stderr,"Unexpected story LCD on ignored key %u\n",story_ignored_index);assert(0);}
}
static int story_ignored_input(SDL_Event *event,const NpeWorld *world,const NpeFile *file){
    static const SDL_Keycode keys[]={SDLK_RETURN,SDLK_KP_ENTER,SDLK_k,SDLK_q,SDLK_u,SDLK_e,
        SDLK_d,SDLK_f,SDLK_TAB,SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT,SDLK_n,SDLK_p,
        SDLK_F4,SDLK_BACKSPACE,SDLK_ESCAPE};
    const char *enabled=getenv("NPE_PROBE_STORY_IGNORED_KEYS"),*variant=getenv("NPE_PROBE_BATTLE_KEYS");
    if(!enabled||!*enabled)return 0;
    if(story_ignored_pending){
        assert(!memcmp(&story_ignored_world,world,sizeof(*world)));
        assert(!memcmp(story_ignored_pixels,last_pixel_frame,sizeof(story_ignored_pixels)));
        assert(story_ignored_reads==npe_rpg_runtime_reads(file));
        story_ignored_pending=0;++story_ignored_total;
    }
    if(story_ignored_index==sizeof(keys)/sizeof(*keys)){story_ignored_index=0;return 0;}
    memcpy(&story_ignored_world,world,sizeof(*world));
    memcpy(story_ignored_pixels,last_pixel_frame,sizeof(story_ignored_pixels));
    story_ignored_reads=npe_rpg_runtime_reads(file);
    memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;event->key.keysym.sym=keys[story_ignored_index++];
    if(variant&&!strcmp(variant,"shift"))event->key.keysym.mod=KMOD_SHIFT;
    if(variant&&!strcmp(variant,"caps"))event->key.keysym.mod=KMOD_CAPS;
    story_ignored_pending=1;return 1;
}
static void story_ignored_report(void){
    assert(!story_ignored_pending);
    if(story_ignored_total)printf("Story ignored inputs:%u, no World change, resource read or LCD submit\n",story_ignored_total);
}
#endif
