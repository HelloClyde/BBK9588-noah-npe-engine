#include "../text.h"
#include <stdio.h>
#include <stdlib.h>

static int load_file(const char *path, NpeFile *file) {
    FILE *input = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    char error[128];
    if (!input || fseek(input, 0, SEEK_END) ||
        (length = ftell(input)) <= 0 || fseek(input, 0, SEEK_SET)) {
        fprintf(stderr, "cannot read NPE: %s\n", path);
        if (input) fclose(input);
        return -1;
    }
    bytes = malloc((size_t)length);
    if (!bytes || fread(bytes, 1, (size_t)length, input) !=
                  (size_t)length) {
        fprintf(stderr, "cannot load NPE: %s\n", path);
        free(bytes);
        fclose(input);
        return -1;
    }
    fclose(input);
    if (npe_parse(file, bytes, (size_t)length, error, sizeof(error))) {
        fprintf(stderr, "invalid NPE: %s\n", error);
        free(bytes);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    NpeFile file = {0};
    NpeFont font = {0};
    NpeWorld world = {0};
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    unsigned usable = 0, item_count, party_adjustments = 0, tile_writes = 0;
    unsigned maps_checked = 0, map_headers_failed = 0, map_tables_failed = 0;
    int failed = 0;
    if (argc != 3) {
        fprintf(stderr, "usage: npe-corpus-probe game.npe gbk12-native.bin\n");
        return 2;
    }
    if (load_file(argv[1], &file)) return 1;
    if (npe_font_load(&font, argv[2])) {
        fprintf(stderr, "cannot load native font\n");
        npe_free(&file);
        return 1;
    }
    if (npe_world_new_game(&file, &world) ||
        npe_render_world(&file, &world, pixels) ||
        npe_render_native_member_screen(&file, &font, &world, 0,
                                        pixels)) {
        fprintf(stderr, "new-game world or member page failed\n");
        failed = 1;
    }
    item_count = file.section_count > 2 ? file.offset_table_records[2] : 0;
    if (!item_count) {
        fprintf(stderr, "no item records\n");
        failed = 1;
    }
    for (unsigned i = 0; i < item_count; ++i) {
        NpeItem item;
        NpeIndexedBitmap icon;
        if (npe_item(&file, i, &item) ||
            npe_item_bitmap(&file, i, &icon)) {
            fprintf(stderr, "item %u record or icon failed\n", i + 1);
            failed = 1;
            continue;
        }
        if (item.effect_type != 1 && item.effect_type != 2) continue;
        ++usable;
        if (npe_render_native_item_target_screen(&file, &font, &world,
                0, i + 1, 1, pixels)) {
            fprintf(stderr, "item %u target page failed\n", i + 1);
            failed = 1;
        }
    }
    if (file.section_count > 8) {
        for (unsigned i = 0; i < file.offset_table_records[8]; ++i) {
            NpeMap map;
            unsigned count;
            ++maps_checked;
            if (npe_map(&file, i, &map)) {
                const uint8_t *record;
                size_t size;
                ++map_headers_failed;
                if (!npe_record(&file,8,i,&record,&size) && size>=17)
                    fprintf(stderr,"map %u header/grid failed: record=%zu width=%u height=%u grid=%u\n",
                            i+1,size,record[2],record[3],(unsigned)record[2]*record[3]*2);
                else fprintf(stderr,"map %u header/record failed\n",i+1);
                failed = 1;
            } else if (npe_map_group_count(&map, 3, &count)) {
                ++map_tables_failed;
                fprintf(stderr, "map %u mode-13 table failed: tail=%zu\n", i + 1,map.tail_size);
                failed = 1;
            } else {
                NpeMapTileChange changes[3];
                party_adjustments += count;
                if (npe_map_tile_changes(&map, changes, &count)) {
                    ++map_tables_failed;
                    fprintf(stderr, "map %u tile-change table failed\n", i + 1);
                    failed = 1;
                } else for (unsigned j = 0; j < count; ++j) tile_writes += changes[j].count;
            }
        }
    }
    printf("title-bytes=");
    for (unsigned i = 0; i < 12 && file.title[i]; ++i)
        printf("%02X", file.title[i]);
    printf(" maps=%u map=%u position=%u,%u items=%u usable-targets=%u party-adjustments=%u tile-writes=%u checked-maps=%u header-failures=%u table-failures=%u status=%s\n",
           file.section_count > 8 ? file.offset_table_records[8] : 0,
           world.map_number, world.x, world.y, item_count, usable,
           party_adjustments, tile_writes,maps_checked,map_headers_failed,map_tables_failed,
           failed ? "failed" : "passed");
    npe_font_free(&font);
    npe_free(&file);
    return failed ? 1 : 0;
}
