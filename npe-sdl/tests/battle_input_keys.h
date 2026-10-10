#ifndef NPE_PROBE_BATTLE_INPUT_KEYS_H
#define NPE_PROBE_BATTLE_INPUT_KEYS_H
#include <stdlib.h>
#include <string.h>

/* Reuse independent original battle/map/menu frame/state fixtures with
   physical letter keys. SDL2 keeps symbols lowercase under Shift/Caps. */
static void probe_battle_input(SDL_Event *event,unsigned mask,SDL_Keycode confirm){
    const char *variant=getenv("NPE_PROBE_BATTLE_KEYS");
    int letters=variant&&*variant;
    memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;
    if(letters){
        assert(!strcmp(variant,"letters")||!strcmp(variant,"shift")||!strcmp(variant,"caps"));
        event->key.keysym.mod=!strcmp(variant,"shift")?KMOD_SHIFT:
                             !strcmp(variant,"caps")?KMOD_CAPS:KMOD_NONE;
    }
    switch(mask){
        case 1:event->key.keysym.sym=letters?SDLK_i:SDLK_UP;break;
        case 2:event->key.keysym.sym=letters?SDLK_m:SDLK_DOWN;break;
        case 4:event->key.keysym.sym=letters?SDLK_j:SDLK_LEFT;break;
        case 8:event->key.keysym.sym=letters?SDLK_l:SDLK_RIGHT;break;
        case 16:event->key.keysym.sym=letters?SDLK_k:confirm;break;
        case 32:event->key.keysym.sym=SDLK_BACKSPACE;break;
        case 64:event->key.keysym.sym=SDLK_F4;break;
        case 0x84:case 512:event->key.keysym.sym=SDLK_p;break;
        case 0x85:case 256:event->key.keysym.sym=SDLK_n;break;
        default:assert(0);
    }
}
#endif
