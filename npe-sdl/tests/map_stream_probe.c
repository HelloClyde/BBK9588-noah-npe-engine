#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_bytes(const char *path, size_t *size) {
    FILE *input = fopen(path, "rb");
    assert(input && !fseek(input, 0, SEEK_END));
    long length = ftell(input);
    assert(length >= 0 && !fseek(input, 0, SEEK_SET));
    uint8_t *bytes = malloc((size_t)length + 1);
    assert(bytes && fread(bytes, 1, (size_t)length, input) == (size_t)length);
    fclose(input); *size = (size_t)length; return bytes;
}
static void compare_resident(const NpeWorld *world, const uint8_t *ram,
                              uint32_t address, size_t size) {
    uint8_t actual[90]; assert(size <= sizeof(actual));
    assert(!npe_world_resident_read(world, address, actual, size));
    assert(!memcmp(actual, ram + address - 0x520000, size));
}
int main(int argc, char **argv) {
    assert(argc == 5);
    size_t size, grid_size, ram_size;
    uint8_t *bytes = read_bytes(argv[1], &size);
    uint8_t *grid = read_bytes(argv[3], &grid_size);
    uint8_t *ram = read_bytes(argv[4], &ram_size);
    unsigned number = (unsigned)strtoul(argv[2], NULL, 10);
    NpeFile file; NpeMap map; NpeWorld world = {0}; char error[128];
    assert(!npe_parse(&file, bytes, size, error, sizeof(error)));
    uint8_t *original = malloc(size);
    assert(original); memcpy(original, bytes, size);
    assert(ram_size == 65536);
    assert(!npe_map(&file, number-1, &map));
    assert(grid_size == (size_t)map.width * map.height * 2);
    assert(!memcmp(map.tile_words, grid, grid_size));
    assert(map.width == ram[0x7a56] && map.height == ram[0x7a5a]);
    assert(!memcmp(map.name, ram + 0x78e4, sizeof(map.name)));
    unsigned count;
    if (npe_map_group_count(&map, 3, &count)) {
        printf("map=%u grid=%zu matched; malformed-tail retained-diagnostic\n",
               number, grid_size);
    } else {
        assert(!npe_world_load_choice_events(&world, &map));
        assert(!npe_world_load_item_events(&world, &map));
        compare_resident(&world, ram, 0x52781c, 30);
        compare_resident(&world, ram, 0x5276b0, 40);
        assert(!npe_world_load_scene_events(&world, &map));
        compare_resident(&world, ram, 0x52754e, 80);
        assert(!npe_world_load_npc_move_events(&world, &map));
        compare_resident(&world, ram, 0x527652, 50);
        assert(!npe_world_load_party_adjust_event(&world, &map));
        compare_resident(&world, ram, 0x5279a2, 4);
        assert(!npe_world_load_teleport_events(&world, &map));
        assert(!npe_world_load_party_events(&world, &map));
        compare_resident(&world, ram, 0x52788e, 38);
        assert(!npe_world_load_battle_events(&world, &map));
        compare_resident(&world, ram, 0x5277fc, 32);
        assert(!npe_world_load_script_walk_events(&world, &map));
        compare_resident(&world, ram, 0x527728, 40);
        assert(!npe_world_load_story_event_cache(&world, &map));
        compare_resident(&world, ram, 0x5274f4, 90);
        assert(!npe_world_load_money_events(&world, &map));
        compare_resident(&world, ram, 0x5276da, 40);
        assert(!npe_world_load_skill_events(&world, &map));
        compare_resident(&world, ram, 0x52790a, 12);
        if (grid_size) {
            assert(!npe_world_init(&file, &world, number, 1, 1));
            assert(!npe_world_resources_open(&file, &world));
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            assert(!npe_render_world(&file, &world, pixels));
        } else {
            /* Resource decoding accepts the original signed offsets.
               The empty-grid viewport/render continuation is pending. */
            NpeMapTileChange changes[3];
            assert(!npe_map_tile_changes(&map, changes, &count));
            assert(!npe_world_init(&file, &world, number, 1, 1));
            assert(!npe_world_resources_open(&file, &world));
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            assert(!npe_render_world(&file, &world, pixels));
        }
        /* A stream may cross another record, but not the actual EOF. */
        size_t tail_start = (size_t)(map.tail-file.bytes);
        size_t tail_size = map.tail_size;
        file.size = tail_start - 1;
        assert(npe_map(&file, number-1, &map) == -1);
        file.size = tail_start + tail_size - 1;
        assert(!npe_map(&file, number-1, &map));
        assert(npe_map_group_count(&map, 3, &count) == -1);
        file.size = size;
        printf("map=%u grid=%zu tail=%zu matched; 456 native event bytes; %s; EOF passed\n",
               number, grid_size, tail_size,
               grid_size ? "world/render passed" : "runtime empty-grid render passed; original LCD pending");
    }
    assert(file.size == size && !memcmp(file.bytes, original, size));
    free(original); free(grid); free(ram); npe_free(&file); return 0;
}
