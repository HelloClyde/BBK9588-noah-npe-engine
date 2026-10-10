#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "../text.h"

static SDL_atomic_t event_loop_ready;
static unsigned list_frames, rejection_frames, desktop_dialogs;

static int probe_new_game(const NpeFile *file, NpeWorld *world);
static int probe_runtime_list(const NpeFile *,const NpeFont *,NpeWorld *,const unsigned *,const unsigned *,unsigned,unsigned,unsigned,int,uint8_t *);
static int probe_item_list(const NpeFile *file, const NpeFont *font,
                           const unsigned *ids, const unsigned *quantities,
                           unsigned count, unsigned selected,
                           uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
static int probe_rejection(const NpeFont *font,
                           uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
static int probe_simple_message(Uint32 flags, const char *title,
                                const char *message, SDL_Window *window);
static int probe_message(const SDL_MessageBoxData *data, int *buttonid);
static int probe_wait_event(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&event_loop_ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}

#define npe_world_new_game probe_new_game
#define npe_world_restart_game probe_new_game
#define npe_render_map_item_list probe_runtime_list
#define npe_render_item_list_screen probe_item_list
#define npe_render_equipment_rejected_overlay probe_rejection
#define SDL_ShowSimpleMessageBox probe_simple_message
#define SDL_ShowMessageBox probe_message
#define SDL_WaitEventTimeout probe_wait_event
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_ShowMessageBox
#undef SDL_ShowSimpleMessageBox
#undef npe_render_equipment_rejected_overlay
#undef npe_render_item_list_screen
#undef npe_world_new_game
#undef npe_world_restart_game
#undef npe_render_map_item_list

static uint64_t screen_hash(const uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned i = 0; i < NPE_WIDTH * NPE_HEIGHT; ++i) {
        hash ^= pixels[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    if (npe_world_new_game(file, world)) return -1;
    for (unsigned id = 1; id <= file->offset_table_records[2] &&
                          id < sizeof(world->condition_words) /
                               sizeof(world->condition_words[0]); ++id)
        world->condition_words[id] = 0;
    world->condition_words[55] = 1; /* Original Hero capture has one Lingzhi. */
    world->game_mode = 6;
    return 0;
}

static int probe_item_list(const NpeFile *file, const NpeFont *font,
                           const unsigned *ids, const unsigned *quantities,
                           unsigned count, unsigned selected,
                           uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    int result = npe_render_item_list_screen(file, font, ids, quantities,
                                             count, selected, pixels);
    if (!result) {
        assert(count == 1 && ids[0] == 55 && quantities[0] == 1);
        assert(screen_hash(pixels) == UINT64_C(0x2ad274729a450c9a));
        ++list_frames;
    }
    return result;
}

static int probe_rejection(const NpeFont *font,
                           uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    int result = npe_render_equipment_rejected_overlay(font, pixels);
    if (!result) {
        assert(screen_hash(pixels) == UINT64_C(0x0ca393b1509c7817));
        ++rejection_frames;
    }
    return result;
}

static int probe_runtime_list(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                              const unsigned *ids,const unsigned *quantities,unsigned count,
                              unsigned selected,unsigned previous,int full,uint8_t *pixels){
    int result=npe_render_map_item_list(file,font,world,ids,quantities,count,selected,previous,full,pixels);
    if(!result){assert(count==1 && ids[0]==55 && quantities[0]==1);
        assert(screen_hash(pixels)==UINT64_C(0x2ad274729a450c9a));++list_frames;}
    return result;
}

static int probe_simple_message(Uint32 flags, const char *title,
                                const char *message, SDL_Window *window) {
    (void)flags; (void)title; (void)message; (void)window;
    ++desktop_dialogs;
    return 0;
}

static int probe_message(const SDL_MessageBoxData *data, int *buttonid) {
    (void)data;
    ++desktop_dialogs;
    if (buttonid) *buttonid = 0;
    return 0;
}

static int send_keys(void *unused) {
    static const SDL_Keycode keys[] = {
        SDLK_RETURN, /* title -> seeded mode-6 menu */
        SDLK_DOWN, SDLK_DOWN, SDLK_RETURN, /* 物品 submenu */
        SDLK_DOWN, SDLK_RETURN, /* 装备 -> native item list */
        SDLK_RETURN, /* Lingzhi -> native rejection */
        SDLK_F4, /* dismiss rejection -> list */
        SDLK_F4, /* list -> submenu */
        SDLK_F4 /* submenu -> menu */
    };
    unsigned waited = 0;
    (void)unused;
    while (!SDL_AtomicGet(&event_loop_ready) && waited < 5000) {
        SDL_Delay(10);
        waited += 10;
    }
    if (!SDL_AtomicGet(&event_loop_ready)) return -1;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        SDL_Event event = {0};
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = keys[i];
        if (SDL_PushEvent(&event) != 1) return -1;
        SDL_Delay(70);
    }
    {
        SDL_Event event = {0};
        event.type = SDL_QUIT;
        return SDL_PushEvent(&event) == 1 ? 0 : -1;
    }
}

int main(int argc, char **argv) {
    char *arguments[] = {"npe-sdl", NULL, NULL};
    int thread_result = -1;
    SDL_Thread *thread;
    assert(argc == 2);
    arguments[1] = argv[1];
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    SDL_AtomicSet(&event_loop_ready, 0);
    thread = SDL_CreateThread(send_keys, "equip-keys", NULL);
    assert(thread);
    assert(run(2, arguments) == 0);
    SDL_WaitThread(thread, &thread_result);
    assert(thread_result == 0);
    assert(list_frames == 1 && rejection_frames == 1);
    assert(desktop_dialogs == 0);
    puts("SDL native equipment list and rejection flow passed");
    return 0;
}
