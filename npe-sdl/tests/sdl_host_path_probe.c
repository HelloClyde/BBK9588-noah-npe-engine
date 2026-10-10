#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#ifdef main
#undef main
#endif

static uint8_t expected_title[NPE_WIDTH * NPE_HEIGHT];
static unsigned fonts_loaded, frames_checked;
static int title_pending;
static int path_title(const NpeFile *file, const NpeFont *font,
                       unsigned selection, uint8_t *pixels) {
    title_pending = 1;
    return npe_render_native_title_screen(file, font, selection, pixels);
}
static int path_font12(NpeFont *font, const char *path) {
    int result = npe_font_load(font, path);
    assert(!result);
    fonts_loaded |= 1;
    return result;
}
static int path_font16(NpeFont *font, const char *path) {
    int result = npe_font_load16(font, path);
    assert(!result);
    fonts_loaded |= 2;
    return result;
}
static SDL_Window *path_window(const char *title, int x, int y, int w, int h, Uint32 flags) {
    (void)w; (void)h;
    return SDL_CreateWindow(title, x, y, NPE_WIDTH, NPE_HEIGHT, flags);
}
static void path_present(SDL_Renderer *renderer) {
    uint32_t rgba[NPE_WIDTH * NPE_HEIGHT];
    assert(fonts_loaded == 3);
    SDL_RenderPresent(renderer);
    if (!title_pending) return; /* Opening pixels have their own native probe. */
    title_pending = 0;
    assert(!SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, rgba, NPE_WIDTH * 4));
    for (unsigned i = 0; i < NPE_WIDTH * NPE_HEIGHT; ++i) {
        unsigned v = expected_title[i];
        uint32_t color = UINT32_C(0xff000000) | ((183-v*142/15)<<16) | ((200-v*146/15)<<8) | (161-v*119/15);
        assert(rgba[i] == color);
    }
    ++frames_checked;
}
static int path_wait(SDL_Event *event, int timeout) {
    (void)timeout;
    assert(frames_checked);
    memset(event, 0, sizeof(*event));
    event->type = SDL_QUIT;
    return 1;
}
#define npe_font_load path_font12
#define npe_render_native_title_screen path_title
#define npe_font_load16 path_font16
#define SDL_CreateWindow path_window
#define SDL_RenderPresent path_present
#define SDL_WaitEventTimeout path_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef npe_font_load16
#undef npe_font_load
#undef npe_render_native_title_screen

#ifdef _WIN32
/* Match the production SDL2main UTF-8 argument conversion. */
#define main SDL_main
#endif
int main(int argc, char **argv) {
    NpeFile file;
    NpeFont font = {0};
    NpeWorld world, restored;
    uint8_t *bytes, *font_bytes, *save_bytes;
    size_t size, font_size, save_size;
    char error[128];
    assert(argc == 5); /* NPE, reference font12, font16, diagnostic save path. */
    assert(!read_file(argv[1], &bytes, &size));
    assert(!npe_parse(&file, bytes, size, error, sizeof(error)));
    assert(!npe_font_load(&font, argv[2]) && !npe_font_load16(&font, argv[3]));
    assert(!read_file(argv[2], &font_bytes, &font_size));
    assert(font_size > 8 && !memcmp(font.data, font_bytes, font_size)); free(font_bytes);
    assert(!read_file(argv[3], &font_bytes, &font_size));
    assert(font_size > 8 && !memcmp(font.data16, font_bytes, font_size)); free(font_bytes);
    assert(!npe_render_native_title_screen(&file, &font, 0, expected_title));
    assert(!npe_world_new_game(&file, &world));
    /* Match production startup before saving. An edge viewport can read the
       retained map-buffer tail, which requires the live resource owner. */
    assert(!npe_world_resources_open(&file, &world));
    NpeMap starting_map;
    assert(!npe_map(&file, world.map_number-1, &starting_map));
    fprintf(stderr, "new-game map%u %ux%u position=%u,%u viewport=%u,%u+%u,%u mode=%u\n",
            world.map_number, starting_map.width, starting_map.height, world.x, world.y,
            world.view.offset_x, world.view.offset_y, world.view.screen_x, world.view.screen_y, world.game_mode);
    assert(world.game_mode == 1);
    world.money = 12345; world.random_state = UINT32_C(0xf123abcd);
    assert(!npe_world_save(&file, &world, argv[4]));
    NpeWorld invalid = world;
    invalid.view.offset_x = starting_map.width;
    assert(npe_world_save(&file, &invalid, argv[4]) == -1);
    invalid = world; invalid.view.offset_y = starting_map.height;
    assert(npe_world_save(&file, &invalid, argv[4]) == -1);
    invalid = world; invalid.view.screen_x = (world.view.screen_x+1)%15;
    assert(npe_world_save(&file, &invalid, argv[4]) == -1);
    assert(!read_file(argv[4], &save_bytes, &save_size));
    assert(save_size > 36 && (!memcmp(save_bytes, "NPESAV22", 8) ||
                             !memcmp(save_bytes, "NPESAV23", 8) ||
                             !memcmp(save_bytes, "NPESAV24", 8)));
    int saved_buffer = !memcmp(save_bytes, "NPESAV24", 8);
    fprintf(stderr, "cold-save format=%.8s bytes=%zu\n", save_bytes, save_size);
    free(save_bytes);
    /* Decode after releasing the live owner; a retained V24 tail must come
       from the save, rather than a map allocation left alive by this test. */
    npe_rpg_runtime_close(&file);
    assert(!npe_world_load(&file, &restored, argv[4]));
    assert(restored.money == world.money && restored.random_state == world.random_state);
    assert(restored.map_number == world.map_number && restored.x == world.x && restored.y == world.y);
    assert(!memcmp(&restored.view, &world.view, sizeof(world.view)));
    assert(!memcmp(restored.status_guard, world.status_guard, sizeof(world.status_guard)));
    assert(!memcmp(restored.party_ids, world.party_ids, sizeof(world.party_ids)));
    if (saved_buffer) {
        assert(world.terrain_buffer_snapshot_ready);
        assert(restored.terrain_buffer_snapshot_ready);
        assert(!memcmp(restored.terrain_snapshot_cells, world.terrain_snapshot_cells,
                       NPE_MAP_BUFFER_WORDS * sizeof(uint16_t)));
    }
    fprintf(stderr, "cold-release retained-buffer-check=%d\n", saved_buffer);
    npe_font_free(&font); npe_free(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER", "dummy", 1));
    assert(!SDL_setenv("SDL_AUDIODRIVER", "dummy", 1));
    char *args[] = {"npe-sdl", argv[1], NULL};
    assert(!run(2, args) && fonts_loaded == 3 && frames_checked == 1);
    puts("UTF-8 NPE/font/save paths, installed fonts and actual SDL title frame passed");
    return 0;
}
