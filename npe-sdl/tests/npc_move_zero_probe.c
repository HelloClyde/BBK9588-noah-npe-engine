#include "../npe.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif

typedef struct {
    unsigned commits, waits, differences;
    const uint8_t *entry;
} DisplayCheck;

static int commit(void *context, const uint8_t *pixels, unsigned wait) {
    DisplayCheck *check = context;
    ++check->commits;
    check->waits += wait;
    for (unsigned i = 0; i < NPE_WIDTH*NPE_HEIGHT; ++i)
        check->differences += pixels[i] != check->entry[i];
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    unsigned slot, npc_id, x, y, event, next_mode, next_id, direction, map_init;
    FILE *fixture = fopen(argv[2], "rb");
    if (!fixture) return 2;
    int fields = fscanf(fixture, "%u %u %u %u %u %u %u %u %u", &slot, &npc_id,
                        &x, &y, &event, &next_mode, &next_id, &direction, &map_init);
    fclose(fixture);
    if (fields != 9 || slot >= 20 || !(event & 255) || (event & 255) > 5) return 2;
    SDL_RWops *manifest = SDL_RWFromFile(argv[1], "rb");
    if (!manifest) return 2;
    char path[4096];
    size_t size = SDL_RWread(manifest, path, 1, sizeof(path)-1);
    SDL_RWclose(manifest);
    path[size] = 0;
    while (size && (path[size-1] == '\r' || path[size-1] == '\n')) path[--size] = 0;
    SDL_RWops *io = SDL_RWFromFile(path, "rb");
    if (!io) return 2;
    Sint64 length = SDL_RWsize(io);
    if (length <= 0) return 2;
    uint8_t *bytes = malloc((size_t)length);
    if (!bytes || SDL_RWread(io, bytes, 1, (size_t)length) != (size_t)length) return 2;
    SDL_RWclose(io);
    NpeFile file;
    char error[128];
    if (npe_parse(&file, bytes, (size_t)length, error, sizeof(error))) return 2;
    NpeWorld initial;
    NpeMap map;
    const uint8_t *npc;
    if (npe_world_init(&file, &initial, 1, 22, 22) || npe_map(&file, 0, &map) ||
        npe_world_npc(&initial, &map, slot, &npc) ||
        ((unsigned)npc[0] << 8 | npc[1]) != npc_id) return 2;
    initial.game_mode = 18;
    initial.story_event_id = (uint16_t)event;
    initial.map_init_pending = 0;
    initial.npc_override_x[slot] = (uint16_t)x;
    initial.npc_override_y[slot] = (uint16_t)y;
    initial.npc_override_direction[slot] = 3;
    initial.npc_move_events[(event & 255)-1] = (NpeNpcMoveCache){
        .npc_id = (uint16_t)npc_id, .x = (uint16_t)x, .y = (uint16_t)y,
        .next_mode = (uint8_t)next_mode, .next_id = (uint16_t)next_id};
    uint64_t fingerprint = npe_game_fingerprint(&file);
    NpeWorld numeric = initial, display = initial;
    int result = npe_world_apply_npc_move_event(&file, &numeric);
    unsigned nx, ny;
    npe_world_npc_position(&numeric, npc, slot, &nx, &ny);
    unsigned resolved_mode = next_mode ? next_mode : 1;
    int numeric_ok = result == 1 && numeric.game_mode == resolved_mode &&
        numeric.story_event_id == next_id && nx == x && ny == y &&
        numeric.npc_override_direction[slot] == direction &&
        numeric.random_state == initial.random_state;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT], entry[NPE_WIDTH*NPE_HEIGHT];
    if (npe_render_world_load(&file, &display, entry)) return 2;
    DisplayCheck check = {0, 0, 0, entry};
    int displayed = npe_world_npc_move_with_display(&file, &display, pixels, commit, &check);
    npe_world_npc_position(&display, npc, slot, &nx, &ny);
    int display_ok = displayed == 1 && display.game_mode == resolved_mode &&
        display.story_event_id == next_id && display.map_init_pending == map_init &&
        nx == x && ny == y && display.npc_override_direction[slot] == direction &&
        display.random_state == initial.random_state && check.commits == 1 &&
        check.waits == 0 && check.differences == 0 &&
        npe_game_fingerprint(&file) == fingerprint;
    printf("{\"slot\":%u,\"npc\":%u,\"input_event\":%u,\"result\":%d,\"display_result\":%d,"
           "\"mode\":%u,\"event\":%u,\"map_init\":%u,\"direction\":%u,\"x\":%u,\"y\":%u,"
           "\"commits\":%u,\"waits\":%u,\"entry_pixel_differences\":%u,\"passed\":%s}\n",
           slot, npc_id, event, result, displayed, display.game_mode, display.story_event_id,
           display.map_init_pending, display.npc_override_direction[slot], nx, ny,
           check.commits, check.waits, check.differences, numeric_ok && display_ok ? "true" : "false");
    npe_free(&file);
    return numeric_ok && display_ok ? 0 : 1;
}
