#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "../text.h"

static SDL_atomic_t event_loop_ready;
static unsigned submenu_seen[2], item_submenu_seen[2],
                status_frames, equipment_frames,
                desktop_dialogs, menu_close_calls;

static int probe_new_game(const NpeFile *file, NpeWorld *world);
static void probe_property_submenu(const NpeFile *file,
                                   const NpeWorld *world,
                                   unsigned selection,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
static void probe_item_submenu(const NpeFile *file, const NpeWorld *world,
                               unsigned selection,
                               uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
static int probe_status_page(const NpeFile *file, const NpeFont *font,
                             const NpeWorld *world, unsigned member,
                             uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
static int probe_status_view(const NpeFile *,const NpeFont *,const NpeWorld *,
                              const NpeWorld *,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
static int probe_equipment_page(const NpeFile *file, const NpeFont *font, NpeWorld *world,
                                const NpeWorld *before,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
static int probe_message_box(Uint32 flags, const char *title,
                             const char *message, SDL_Window *window);
static int probe_close_menu(NpeWorld *world);
static int probe_wait_event(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&event_loop_ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}

#define npe_world_new_game probe_new_game
#define npe_world_restart_game probe_new_game
#define npe_render_property_submenu_overlay probe_property_submenu
#define npe_render_item_submenu_overlay probe_item_submenu
#define npe_render_native_member_screen probe_status_page
#define npe_render_native_member_view probe_status_view
#define npe_render_equipment_view probe_equipment_page
#define SDL_ShowSimpleMessageBox probe_message_box
#define npe_world_close_system_menu probe_close_menu
#define SDL_WaitEventTimeout probe_wait_event
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_ShowSimpleMessageBox
#undef npe_world_close_system_menu
#undef npe_render_native_member_screen
#undef npe_render_native_member_view
#undef npe_render_equipment_view
#undef npe_render_property_submenu_overlay
#undef npe_render_item_submenu_overlay
#undef npe_world_new_game
#undef npe_world_restart_game

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    if (npe_world_init(file, world, 1, 43, 2)) return -1;
    world->game_mode = 6;
    return 0;
}

static void probe_property_submenu(const NpeFile *file,
                                   const NpeWorld *world,
                                   unsigned selection,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if (selection < 2) ++submenu_seen[selection];
    npe_render_property_submenu_overlay(file, world, selection, pixels);
}

static void probe_item_submenu(const NpeFile *file, const NpeWorld *world,
                               unsigned selection,
                               uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if (selection < 2) ++item_submenu_seen[selection];
    npe_render_item_submenu_overlay(file, world, selection, pixels);
}

static int probe_status_page(const NpeFile *file, const NpeFont *font,
                             const NpeWorld *world, unsigned member,
                             uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    ++status_frames;
    return npe_render_native_member_screen(file, font, world, member, pixels);
}

static int probe_equipment_page(const NpeFile *file, const NpeFont *font, NpeWorld *world,
                                const NpeWorld *before,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    ++equipment_frames;
    return npe_render_equipment_view(file, font, world, before, pixels);
}
static int probe_status_view(const NpeFile *file,const NpeFont *font,
                              const NpeWorld *world,const NpeWorld *before,
                              uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    ++status_frames;
    return npe_render_native_member_view(file,font,world,before,pixels);
}

static int probe_message_box(Uint32 flags, const char *title,
                             const char *message, SDL_Window *window) {
    (void)flags; (void)title; (void)message; (void)window;
    ++desktop_dialogs;
    return 0;
}

static int probe_close_menu(NpeWorld *world) {
    ++menu_close_calls;
    return npe_world_close_system_menu(world);
}

static int send_keys(void *unused) {
    static const SDL_Keycode keys[] = {
        SDLK_RETURN, /* title: new game, intercepted at system menu */
        SDLK_RETURN, /* 属性: open 状态／穿戴 */
        SDLK_DOWN, SDLK_UP, /* render both submenu selections */
        SDLK_RETURN, /* 状态: native status page */
        SDLK_RETURN, /* stay on native page; no English summary */
        SDLK_F4, /* native return to map */
        SDLK_F4, SDLK_RETURN, SDLK_DOWN, SDLK_RETURN,
        /* reopen menu, choose 属性 → 穿戴 */
        SDLK_RETURN, SDLK_LEFT, SDLK_RIGHT, SDLK_DOWN, SDLK_F4,
        /* original wear page ignores confirm/one-member Down, cycles slots,
           and F4 restores the menu-entry scene */
        SDLK_F4, SDLK_DOWN, SDLK_DOWN, SDLK_RETURN,
        SDLK_DOWN, SDLK_UP, SDLK_F4
        /* enter 物品 → 使用／装备 and render both selections */
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
    thread = SDL_CreateThread(send_keys, "property-keys", NULL);
    assert(thread);
    assert(run(2, arguments) == 0);
    SDL_WaitThread(thread, &thread_result);
    assert(thread_result == 0);
    assert(submenu_seen[0] && submenu_seen[1] && status_frames >= 1);
    assert(equipment_frames >= 3);
    assert(item_submenu_seen[0] && item_submenu_seen[1]);
    assert(menu_close_calls == 3);
    assert(desktop_dialogs == 0);
    puts("SDL property and item submenus, status and equipment opening passed");
    return 0;
}
