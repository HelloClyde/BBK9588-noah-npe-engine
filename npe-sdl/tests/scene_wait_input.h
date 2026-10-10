#include <string.h>
/* Older menu/route probes do not measure playback time. Supply an accepted
   frame-wait input so the new opening renders fully without a 12-second wait.
   Main-loop key sequences remain in SDL's real event queue. */
static int probe_scene_wait_input(SDL_Event *event) {
    memset(event, 0, sizeof(*event));
    event->type = SDL_KEYDOWN;
    event->key.keysym.sym = SDLK_RETURN;
    return 1;
}
#define SDL_PollEvent probe_scene_wait_input
