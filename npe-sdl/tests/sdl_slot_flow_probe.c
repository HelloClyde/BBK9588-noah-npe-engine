#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#ifdef main
#undef main
#endif

static int probe_show_messagebox(Uint32 flags, const char *title,
                                 const char *message, SDL_Window *window) {
    (void)flags; (void)title; (void)message; (void)window;
    return 0;
}
static SDL_atomic_t event_loop_ready;
static int probe_wait_event(SDL_Event *event, int timeout, const char *caller) {
    /* Walking waits intentionally consume ordinary keys. Only expose the
       outer event loop as ready, so the next menu key cannot land in one. */
    int outer = strcmp(caller, "run") == 0 || strcmp(caller,"game_WaitEvent")==0;
    if (outer) SDL_AtomicSet(&event_loop_ready, 1);
    int result = SDL_WaitEventTimeout(event, timeout);
    if (outer) SDL_AtomicSet(&event_loop_ready, 0);
    return result;
}
static int probe_wait_blocking(SDL_Event *event) {
    SDL_AtomicSet(&event_loop_ready, 1);
    int result = SDL_WaitEvent(event);
    SDL_AtomicSet(&event_loop_ready, 0);
    return result;
}
#define SDL_ShowSimpleMessageBox probe_show_messagebox
#define SDL_WaitEventTimeout(event, slice) probe_wait_event(event, slice, caller)
#define SDL_WaitEvent probe_wait_blocking
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_WaitEvent
#undef SDL_ShowSimpleMessageBox

typedef struct {
    const SDL_Keycode *keys;
    size_t count;
    int pushed;
} KeySequence;

static int send_keys(void *context) {
    KeySequence *sequence = context;
    for (size_t index = 0; index < sequence->count; ++index) {
        unsigned waited = 0;
        while (!SDL_AtomicGet(&event_loop_ready) && waited < 10000) {
            SDL_Delay(10); waited += 10;
        }
        if (!SDL_AtomicGet(&event_loop_ready)) return -1;
        SDL_AtomicSet(&event_loop_ready, 0);
        SDL_Event event = {0};
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = sequence->keys[index];
        if (SDL_PushEvent(&event) != 1) return -1;
        sequence->pushed++;
        SDL_Delay(70);
    }
    {
        SDL_Event event = {0};
        event.type = SDL_QUIT;
        return SDL_PushEvent(&event) == 1 ? 0 : -1;
    }
}

static void play_keys(const char *path, const SDL_Keycode *keys,
                      size_t count) {
    char *arguments[] = {"npe-sdl", (char *)path, NULL};
    KeySequence sequence = {keys, count, 0};
    int thread_result = -1;
    SDL_AtomicSet(&event_loop_ready, 0);
    SDL_Thread *thread = SDL_CreateThread(send_keys, "slot-keys", &sequence);
    assert(thread);
    assert(run(2, arguments) == 0);
    SDL_WaitThread(thread, &thread_result);
    assert(thread_result == 0 && sequence.pushed == (int)count);
}

int main(int argc, char **argv) {
    static const SDL_Keycode save_second[] = {
        SDLK_RETURN, SDLK_ESCAPE, SDLK_DOWN, SDLK_DOWN, SDLK_DOWN,
        SDLK_RETURN, SDLK_DOWN, SDLK_RETURN, SDLK_DOWN, SDLK_RETURN
    };
    static const SDL_Keycode overwrite_second[] = {
        SDLK_DOWN, SDLK_RETURN, SDLK_RETURN, /* empty first slot starts new game */
        SDLK_F4, SDLK_DOWN, SDLK_DOWN, SDLK_DOWN, SDLK_RETURN,
        SDLK_RETURN, SDLK_DOWN, SDLK_RETURN, SDLK_RIGHT, /* load second, move */
        SDLK_ESCAPE, SDLK_DOWN, SDLK_DOWN, SDLK_DOWN,
        SDLK_RETURN, SDLK_DOWN, SDLK_RETURN, SDLK_DOWN, SDLK_RETURN
    };
    static const SDL_Keycode load_second[] = {
        SDLK_DOWN, SDLK_RETURN, SDLK_DOWN, SDLK_RETURN,
        SDLK_F4, SDLK_DOWN, SDLK_DOWN, SDLK_DOWN, SDLK_RETURN,
        SDLK_DOWN, SDLK_RETURN, SDLK_RETURN
    };
    static const SDL_Keycode cancel_overwrite[] = {
        SDLK_DOWN, SDLK_RETURN, SDLK_DOWN, SDLK_RETURN, SDLK_RIGHT,
        SDLK_ESCAPE, SDLK_DOWN, SDLK_DOWN, SDLK_DOWN,
        SDLK_RETURN, SDLK_DOWN, SDLK_RETURN, SDLK_DOWN, SDLK_F4
    };
    char error[160];
    const char *copy = "tmp/npe-slot-flow.npe";
    uint8_t *bytes = NULL;
    size_t size = 0;
    NpeFile file;
    NpeWorld first, second;
    char *slot1, *slot2, *slot3;
    FILE *stream;
    assert(argc == 2);
    assert(read_file(argv[1], &bytes, &size) == 0);
    assert(size >= 24);
    memcpy(bytes + 0x0c, "SLOT-FLOW-01", 12);
    stream = fopen(copy, "wb");
    assert(stream && fwrite(bytes, 1, size, stream) == size);
    assert(fclose(stream) == 0);
    assert(npe_parse(&file, bytes, size, error, sizeof(error)) == 0);
    bytes = NULL;
    slot1 = game_save_path(&file, 0);
    slot2 = game_save_path(&file, 1);
    slot3 = game_save_path(&file, 2);
    assert(slot1 && slot2 && slot3);
    assert(strcmp(slot1, slot2) && strcmp(slot2, slot3));
    /* These paths belong to the synthetic SLOT-FLOW-01 NPE. A prior
       assertion may have left a slot; begin the empty-slot scenario fresh. */
    const char *owned_slots[] = {slot1, slot2, slot3};
    for (unsigned i = 0; i < 3; ++i) {
        FILE *old = fopen(owned_slots[i], "rb");
        if (old) { assert(!fclose(old)); assert(!remove(owned_slots[i])); }
    }
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    play_keys(copy, save_second, sizeof(save_second) / sizeof(save_second[0]));
    assert(npe_world_load(&file, &first, slot2) == 0);
    assert(npe_world_load(&file, &second, slot1) != 0);
    play_keys(copy, overwrite_second,
              sizeof(overwrite_second) / sizeof(overwrite_second[0]));
    assert(npe_world_load(&file, &second, slot2) == 0);
    assert(second.map_number == first.map_number && second.x == first.x + 1 &&
           second.y == first.y && second.story_progress == first.story_progress);
    play_keys(copy, cancel_overwrite,
              sizeof(cancel_overwrite) / sizeof(cancel_overwrite[0]));
    assert(npe_world_load(&file, &second, slot2) == 0);
    assert(second.x == first.x + 1 && second.y == first.y);
    play_keys(copy, load_second, sizeof(load_second) / sizeof(load_second[0]));
    assert(npe_world_load(&file, &second, slot1) == 0);
    assert(first.map_number == second.map_number && first.x + 1 == second.x &&
           first.y == second.y && first.story_progress == second.story_progress);
    assert(remove(slot1) == 0 && remove(slot2) == 0);
    assert(npe_world_load(&file, &second, slot3) != 0);
    assert(remove(copy) == 0);
    free(slot1); free(slot2); free(slot3);
    npe_free(&file);
    puts("SDL title and system slot flow passed");
    return 0;
}
