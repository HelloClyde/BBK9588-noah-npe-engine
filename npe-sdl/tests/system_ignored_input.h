#ifndef NPE_SYSTEM_IGNORED_INPUT_H
#define NPE_SYSTEM_IGNORED_INPUT_H
#include <stdlib.h>

static unsigned system_ignored_index,system_ignored_pending,system_ignored_total;
static NpeWorld system_ignored_world;
static uint8_t system_ignored_pixels[NPE_WIDTH*NPE_HEIGHT],system_ignored_controls[3];

static void system_ignored_reset(void){assert(!system_ignored_pending);system_ignored_index=0;}
static void system_ignored_present(void){
    if(system_ignored_pending){fprintf(stderr,"Unexpected LCD on ignored title/system key %u\n",system_ignored_index);assert(0);}
}
static int system_ignored_input(SDL_Event *event,const NpeWorld *world,const NpeFile *file){
    static const SDL_Keycode keys[]={SDLK_SPACE,SDLK_q,SDLK_u,SDLK_e,SDLK_d,SDLK_f,SDLK_TAB};
    static const SDL_Keycode host_keys[]={SDLK_F5,SDLK_F9};
    const char *enabled=getenv("NPE_PROBE_SYSTEM_IGNORED_KEYS"),*variant=getenv("NPE_PROBE_BATTLE_KEYS");
    int host_only=getenv("NPE_PROBE_HOST_SAVE_KEYS")!=NULL;
    const SDL_Keycode *selected=host_only?host_keys:keys;
    unsigned key_count=host_only?sizeof(host_keys)/sizeof(*host_keys):sizeof(keys)/sizeof(*keys);
    uint8_t controls[3];
    if(!host_only&&(!enabled||!*enabled))return 0;
    if(system_ignored_pending){
        assert(same_display_world(&system_ignored_world,world));
        assert(!memcmp(system_ignored_pixels,last_pixel_frame,sizeof(system_ignored_pixels)));
        assert(!npe_rpg_runtime_read(file,0x527a7a,controls,1));
        assert(!npe_rpg_runtime_read(file,0x527a7c,controls+1,1));
        assert(!npe_rpg_runtime_read(file,0x527a7e,controls+2,1));
        assert(!memcmp(controls,system_ignored_controls,3));
        system_ignored_pending=0;++system_ignored_total;
    }
    if(world->game_mode!=6&&world->game_mode!=7){assert(!system_ignored_index);return 0;}
    if(system_ignored_index==key_count){system_ignored_index=0;return 0;}
    memcpy(&system_ignored_world,world,sizeof(*world));
    memcpy(system_ignored_pixels,last_pixel_frame,sizeof(system_ignored_pixels));
    assert(!npe_rpg_runtime_read(file,0x527a7a,system_ignored_controls,1));
    assert(!npe_rpg_runtime_read(file,0x527a7c,system_ignored_controls+1,1));
    assert(!npe_rpg_runtime_read(file,0x527a7e,system_ignored_controls+2,1));
    memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;event->key.keysym.sym=selected[system_ignored_index++];
    if(variant&&!strcmp(variant,"shift"))event->key.keysym.mod=KMOD_SHIFT;
    if(variant&&!strcmp(variant,"caps"))event->key.keysym.mod=KMOD_CAPS;
    system_ignored_pending=1;return 1;
}
static void system_ignored_report(void){
    assert(!system_ignored_pending);
    if(system_ignored_total)printf("Title/system ignored inputs:%u, retained World, controls and LCD\n",system_ignored_total);
}
#endif
