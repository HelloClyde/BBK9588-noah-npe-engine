#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned native_pages, native_fallback_pages;

static void check_text16_bytes(const NpeFont *font, const char *directory) {
    static const char *names[] = {"map139", "map140", "ordinary", "control",
        "unpaired", "invalid_pair", "high80", "lead_fe", "empty"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        char path[4096];uint8_t input[64], expected[NPE_WIDTH*NPE_HEIGHT];
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT] = {0};
        int n=snprintf(path,sizeof(path),"%s/%s.input",directory,names[i]);
        assert(n>0&&(size_t)n<sizeof(path));FILE *f=fopen(path,"rb");assert(f);
        size_t size=fread(input,1,sizeof(input),f);assert(feof(f)&&size>=4);fclose(f);
        unsigned left=(unsigned)input[0]|(unsigned)input[1]<<8;
        unsigned top=(unsigned)input[2]|(unsigned)input[3]<<8;
        n=snprintf(path,sizeof(path),"%s/%s.idx",directory,names[i]);
        assert(n>0&&(size_t)n<sizeof(path));f=fopen(path,"rb");assert(f);
        assert(fread(expected,1,sizeof(expected),f)==sizeof(expected)&&fgetc(f)==EOF);fclose(f);
        assert(!npe_draw_gbk_text16_color(font,input+4,size-4,left,top,15,pixels));
        /* Reference pixels are composed solely from sampled original glyph
           buffers/coordinates. They are not an original full LCD capture. */
        assert(!memcmp(expected,pixels,sizeof(pixels)));
    }
    puts("Original 16-row string glyph/position checks: 9 passed");
}

static unsigned check_page(const NpeFont *font, const uint8_t *data,
                           size_t size, const char *kind, unsigned id) {
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    size_t start, end;
    unsigned count = 0;
    for (start = 0; start < size; start = end + 1) {
        end = npe_gbk_page_end(data, size, start);
        if (end == SIZE_MAX) {
            size_t bad = start;
            while (bad < size && data[bad] != '{') {
                if (data[bad] >= 0x81 && data[bad] <= 0xfe) {
                    if (bad + 1 >= size || data[bad + 1] < 0x40 ||
                        data[bad + 1] > 0xfe || data[bad + 1] == 0x7f)
                        break;
                    bad += 2;
                } else ++bad;
            }
            fprintf(stderr, "invalid GBK page: %s=%u offset=%zu total=%zu bytes=",
                    kind, id, start, size);
            for (size_t i = start; i < size && i < start + 64; ++i)
                fprintf(stderr, "%02X", data[i]);
            fprintf(stderr, " invalid-at=%zu pair=%02X%02X\n", bad,
                    bad < size ? data[bad] : 0,
                    bad + 1 < size ? data[bad + 1] : 0);
            assert(0);
        }
        if (end == start) continue;
        memset(pixels, 0, sizeof(pixels));
        if (npe_render_gbk_dialogue(font, data + start, end - start,
                                    end < size - 1, pixels) != 0) {
            fprintf(stderr, "GBK page cannot fit: %s=%u offset=%zu size=%zu bytes=",
                    kind, id, start, end - start);
            for (size_t i = start; i < end && i < start + 48; ++i)
                fprintf(stderr, "%02X", data[i]);
            fputc('\n', stderr);
            assert(0);
        }
        if (font->data16) {
            memset(pixels, 0, sizeof(pixels));
            ++native_pages;
            if (npe_render_native_dialogue(font, data + start,
                                           end - start, pixels)) {
                ++native_fallback_pages;
                fprintf(stderr, "native dialogue fallback: %s=%u "
                        "offset=%zu size=%zu\n",
                        kind, id, start, end - start);
            }
        }
        count++;
    }
    return count;
}

static void check_real_game(const NpeFont *font, const char *path,
                            int title_only) {
    FILE *input = fopen(path, "rb");
    NpeFile file;
    uint8_t *bytes;
    long length;
    char error[160];
    unsigned npc_id, case_index, story_index, item_index, quiz_index;
    unsigned pages = 0, items = 0, quizzes = 0;
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    native_pages = native_fallback_pages = 0;
    assert(input);
    assert(fseek(input, 0, SEEK_END) == 0);
    length = ftell(input);
    assert(length > 0 && fseek(input, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes);
    assert(fread(bytes, 1, (size_t)length, input) == (size_t)length);
    fclose(input);
    assert(npe_parse(&file, bytes, (size_t)length, error,
                     sizeof(error)) == 0);
    {
        uint8_t next[NPE_WIDTH * NPE_HEIGHT];
        uint8_t expected[NPE_WIDTH * NPE_HEIGHT];
        static const uint8_t firmware_prefix[] = {
            'R','P','G',0xcf,0xb5,0xc1,0xd0,0xd6,0xae,0xa3,0xad
        };
        uint8_t full_title[sizeof(firmware_prefix) + 12];
        NpeIndexedBitmap backdrop;
        NpeFile other_title = file;
        size_t title_size = 0;
        assert(npe_render_native_title_screen(&file, font, 0, pixels) == 0);
        assert(npe_indexed_bitmap(&file, file.indexed_bitmap_count - 60,
                                  &backdrop) == 0);
        for (unsigned y = 0; y < NPE_HEIGHT; ++y)
            for (unsigned x = 0; x < NPE_WIDTH; ++x)
                expected[y * NPE_WIDTH + x] =
                    npe_indexed_pixel(&backdrop, x, y);
        while (title_size < 12 && file.title[title_size]) ++title_size;
        memcpy(full_title, firmware_prefix, sizeof(firmware_prefix));
        memcpy(full_title + sizeof(firmware_prefix), file.title, title_size);
        if (title_size == 10 && memcmp(file.title,
                "\xcc\xec\xbd\xb5\xb7\xe7\xd4\xc6\xc2\xbc", 10) == 0) {
            uint64_t checksum = UINT64_C(14695981039346656037);
            {
                const unsigned ids[3] = {69, 150, 151};
                const unsigned quantities[3] = {1, 2, 3};
                for (unsigned selected = 0; selected < 3; ++selected)
                    assert(npe_render_item_list_screen(&file, font, ids,
                           quantities, 3, selected, next) == 0);
                assert(npe_render_item_list_screen(&file, font, ids,
                       quantities, 4, 2, next) == -1);
            }
            memset(next, 0, sizeof(next));
            assert(npe_draw_gbk_text16_color(font, full_title,
                   sizeof(firmware_prefix) + title_size,
                   43, 30, 15, next) == 0);
            for (unsigned y = 30; y < 46; ++y)
                for (unsigned x = 43; x < 211; ++x) {
                    checksum ^= next[y * NPE_WIDTH + x] != 0;
                    checksum *= UINT64_C(1099511628211);
                }
            /* Original title LCD crop, 2026-09-18, 168x16 pixels. */
            assert(checksum == UINT64_C(0xcb7ab4cb5d2ec2fb));
        }
        if (title_size == 8 && memcmp(file.title,
                "\xd3\xa2\xd0\xdb\xb4\xab\xc6\xe6", 8) == 0) {
            NpeWorld equipment = {0};
            uint64_t hash = UINT64_C(14695981039346656037);
            equipment.party_active[0] = 1;
            equipment.party_ids[0] = 1;
            assert(!npe_character_metadata(&file,0,&equipment.party_characters[0]));
            assert(npe_render_native_equipment_screen(&file, &equipment,
                                                      0, next) == 0);
            for (size_t i = 0; i < sizeof(next); ++i)
                hash = (hash ^ next[i]) * UINT64_C(1099511628211);
            /* Original Hero equipment opening LCD, full 240x120 frame. */
            assert(hash == UINT64_C(0xba5fde8b1c5dae00));
            equipment.game_mode = 6;
            for (unsigned selected = 0; selected < 2; ++selected) {
                const uint64_t expected[] = {
                    UINT64_C(0x632c7578123f4b00),
                    UINT64_C(0x7b5a5a4904dab01a)
                };
                uint64_t crop = UINT64_C(14695981039346656037);
                memset(next, 0, sizeof(next));
                npe_render_item_submenu_overlay(&file, &equipment,
                                                selected, next);
                for (unsigned y = 70; y < 120; ++y)
                    for (unsigned x = 42; x < 92; ++x)
                        crop = (crop ^ next[y * NPE_WIDTH + x]) *
                               UINT64_C(1099511628211);
                assert(crop == expected[selected]);
            }
            if (file.size == 2053994) {
                const uint64_t expected[] = {
                    UINT64_C(0x2ad274729a450c9a),
                    UINT64_C(0x0ca393b1509c7817)
                };
                for (unsigned page = 0; page < 2; ++page) {
                    uint64_t full = UINT64_C(14695981039346656037);
                    assert((page ? npe_render_equipment_rejected_screen(
                                      &file, font, 55, 1, next) :
                                   npe_render_single_item_screen(
                                      &file, font, 55, 1, next)) == 0);
                    for (size_t i = 0; i < sizeof(next); ++i)
                        full = (full ^ next[i]) * UINT64_C(1099511628211);
                    assert(full == expected[page]);
                }
            }
        }
        assert(npe_draw_gbk_text16_color(font, full_title,
               sizeof(firmware_prefix) + title_size,
               43, 30, 15, expected) == 0);
        for (unsigned row = 30; row < 46; ++row)
            assert(memcmp(pixels + row * NPE_WIDTH,
                          expected + row * NPE_WIDTH, NPE_WIDTH) == 0);
        assert(npe_render_native_title_screen(&file, font, 1, next) == 0);
        assert(memcmp(pixels, next, sizeof(pixels)) != 0);
        assert(pixels[60 * NPE_WIDTH + 90] != next[60 * NPE_WIDTH + 90]);
        if (title_size == 10 && memcmp(file.title,
                "\xcc\xec\xbd\xb5\xb7\xe7\xd4\xc6\xc2\xbc", 10) == 0) {
            uint64_t hashes[2] = {
                UINT64_C(14695981039346656037),
                UINT64_C(14695981039346656037)
            };
            for (size_t i = 0; i < sizeof(pixels); ++i) {
                hashes[0] = (hashes[0] ^ pixels[i]) *
                            UINT64_C(1099511628211);
                hashes[1] = (hashes[1] ^ next[i]) *
                            UINT64_C(1099511628211);
            }
            /* Native LCD selections 0 and 1, full 240x120 frames. */
            assert(hashes[0] == UINT64_C(0xb56ebb12f8eb90f8));
            assert(hashes[1] == UINT64_C(0xbfb1db63753e2414));
            {
                uint64_t exit_hash = UINT64_C(14695981039346656037);
                assert(npe_render_native_title_screen(&file, font, 2,
                                                      next) == 0);
                for (size_t i = 0; i < sizeof(next); ++i)
                    exit_hash = (exit_hash ^ next[i]) *
                                UINT64_C(1099511628211);
                /* Native LCD gray5 uses twice the NPE 0..15 index values.
                   This hash is the original frame normalized by two. */
                assert(exit_hash == UINT64_C(0x72cdea930f14e4c8));
            }
            {
                uint64_t item_hash = UINT64_C(14695981039346656037);
                assert(npe_render_single_item_screen(&file, font, 69, 1,
                                                      next) == 0);
                for (size_t i = 0; i < sizeof(next); ++i)
                    item_hash = (item_hash ^ next[i]) *
                                UINT64_C(1099511628211);
                /* Native dragon-eye inventory LCD, full 240x120 frame. */
                assert(item_hash == UINT64_C(0x7b81de52a521610d));
            }
            {
                NpeWorld status = {0};
                uint64_t status_hash = UINT64_C(14695981039346656037);
                status.game_mode = 4;
                status.party_active[0] = 1;
                status.party_ids[0] = 1;
                NpeCharacter initial_character;
                assert(!npe_character(&file,0,&initial_character));
                memcpy(status.party_names[0],initial_character.name,12);
                assert(!npe_character_metadata(&file,0,&status.party_characters[0]));
                status.level[0] = 1;
                status.experience_to_next[0] = 10;
                status.money = 100;
                status.attack[0] = 15;
                status.defense[0] = 13;
                status.hp[0] = 86;
                status.max_hp[0] = 130;
                status.qi[0] = status.max_qi[0] = 60;
                status.other_stats[0][0] = status.other_stats[0][1] =
                    status.other_stats[0][2] = 8;
                assert(npe_render_native_member_screen(&file, font, &status,
                                                       0, next) == 0);
                for (size_t i = 0; i < sizeof(next); ++i)
                    status_hash = (status_hash ^ next[i]) *
                                  UINT64_C(1099511628211);
                /* Original Tianjiang status LCD, full 240x120 frame. */
                assert(status_hash == UINT64_C(0x9994ac648786300d));
            }
            {
                NpeWorld menu = {0};
                uint64_t crop_hash = UINT64_C(14695981039346656037);
                menu.game_mode = 6;
                memset(next, 0, sizeof(next));
                npe_render_property_submenu_overlay(&file, &menu, 0, next);
                for (unsigned y = 37; y < 87; ++y)
                    for (unsigned x = 42; x < 92; ++x)
                        crop_hash = (crop_hash ^ next[y * NPE_WIDTH + x]) *
                                    UINT64_C(1099511628211);
                /* Original Tianjiang status/wear submenu selected first. */
                assert(crop_hash == UINT64_C(0x6bbaff52448a7af4));
            }
            {
                NpeWorld target = {0};
                uint64_t hashes[2] = {
                    UINT64_C(14695981039346656037),
                    UINT64_C(14695981039346656037)
                };
                target.party_active[0] = 1;
                target.party_ids[0] = 1;
                assert(!npe_character_metadata(&file,0,&target.party_characters[0]));
                target.level[0] = 1;
                target.experience_to_next[0] = 10;
                target.hp[0] = 110;
                target.max_hp[0] = 130;
                target.qi[0] = target.max_qi[0] = 60;
                target.attack[0] = 15;
                target.defense[0] = 13;
                target.other_stats[0][0] = target.other_stats[0][1] =
                    target.other_stats[0][2] = 8;
                assert(npe_render_native_item_target_screen(&file, font,
                       &target, 0, 69, 1, next) == 0);
                for (size_t i = 0; i < sizeof(next); ++i)
                    hashes[0] = (hashes[0] ^ next[i]) *
                                UINT64_C(1099511628211);
                target.status_guard[0][1] = 1;
                assert(npe_render_native_item_target_screen(&file, font,
                       &target, 0, 69, 0, next) == 0);
                for (size_t i = 0; i < sizeof(next); ++i)
                    hashes[1] = (hashes[1] ^ next[i]) *
                                UINT64_C(1099511628211);
                /* Original dragon-eye target LCD before and after use. */
                assert(hashes[0] == UINT64_C(0x2a22999e29a42aae));
                assert(hashes[1] == UINT64_C(0x7398f5d1c89cb13b));
            }
        }
        if (file.size == 2053994 && title_size == 8 &&
            memcmp(file.title, "\xd3\xa2\xd0\xdb\xb4\xab\xc6\xe6", 8) == 0) {
            const unsigned ids[2] = {46, 55};
            const unsigned quantities[2] = {1, 1};
            uint64_t item_hash = UINT64_C(14695981039346656037);
            NpeWorld magic = {0};
            uint64_t magic_hash = UINT64_C(14695981039346656037);
            assert(npe_render_item_list_screen(&file, font, ids, quantities,
                                                2, 0, next) == 0);
            for (size_t i = 0; i < sizeof(next); ++i)
                item_hash = (item_hash ^ next[i]) *
                            UINT64_C(1099511628211);
            /* Original Hero's Legend two-item LCD, full 240x120 frame. */
            assert(item_hash == UINT64_C(0x0ff936ac8cfc2f20));
            {
                NpeWorld menu = {0};
                uint64_t crop_hash = UINT64_C(14695981039346656037);
                menu.game_mode = 6;
                memset(next, 0, sizeof(next));
                npe_render_property_submenu_overlay(&file, &menu, 1, next);
                for (unsigned y = 37; y < 87; ++y)
                    for (unsigned x = 42; x < 92; ++x)
                        crop_hash = (crop_hash ^ next[y * NPE_WIDTH + x]) *
                                    UINT64_C(1099511628211);
                /* Original Hero's Legend status/wear submenu on wear. */
                assert(crop_hash == UINT64_C(0x5da68af92dfc6b4b));
            }
            assert(npe_world_new_game(&file, &magic) == 0);
            magic.qi[0] = 59;
            magic.max_qi[0] = 60;
            magic.skill_slots[0][0] = 11;
            assert(npe_render_native_magic_screen(&file, font, &magic,
                                                   0, 11, next) == 0);
            for (size_t i = 0; i < sizeof(next); ++i)
                magic_hash = (magic_hash ^ next[i]) *
                             UINT64_C(1099511628211);
            /* Native single-spell inspection LCD, full 240x120 frame. */
            assert(magic_hash == UINT64_C(0x73ed432fddb6aba5));
            {
                static const uint16_t expected_qi[4] = {59, 44, 29, 14};
                static const uint64_t expected_hash[5] = {
                    UINT64_C(0x9d8446b0a1758e84),
                    UINT64_C(0x419fb2aeca234ff8),
                    UINT64_C(0x5710423cd8162af0),
                    UINT64_C(0xbcb70f9cfe107140),
                    UINT64_C(0x3b9fbd76721fae5d)
                };
                magic.game_mode = 6;
                magic.level[0] = 1;
                magic.experience[0] = 3;
                magic.hp[0] = 92;
                /* The native list loads the selected shared skill before
                   this target-page sequence. Include that caller state. */
                NpeSkill selected_skill;
                assert(npe_world_skill(&file, &magic, 11, &selected_skill) == 0);
                for (unsigned cast = 0; cast < 5; ++cast) {
                    uint64_t hash = UINT64_C(14695981039346656037);
                    NpeWorld before = magic;
                    if (cast && cast < 4) {
                        assert(npe_world_menu_cast_skill(&file, &magic,
                               0, 0, 11) == 1);
                        before.qi[0] -= 15;
                    }
                    if (cast == 4)
                        assert(npe_world_menu_cast_skill(&file, &magic,
                               0, 0, 11) == -3);
                    assert(memcmp(&before, &magic, sizeof(magic)) == 0);
                    assert(magic.qi[0] == expected_qi[cast == 4 ? 3 : cast]);
                    assert(magic.hp[0] == 92);
                    assert(magic.attack[0] == 13 && magic.defense[0] == 11);
                    assert(npe_render_native_magic_target_screen(&file,
                           font, &magic, 0, 0, 11, cast == 4,
                           next) == 0);
                    for (size_t i = 0; i < sizeof(next); ++i)
                        hash = (hash ^ next[i]) *
                               UINT64_C(1099511628211);
                    assert(hash == expected_hash[cast]);
                }
            }
        }
        /* The fixed RPG prefix and backdrop stay the same, but the game
           name comes from the current NPE header. */
        memset(other_title.title, 0, sizeof(other_title.title));
        memcpy(other_title.title, "OTHER", 5);
        assert(npe_render_native_title_screen(&other_title, font, 0, next) == 0);
        assert(memcmp(pixels, next, sizeof(pixels)) != 0);
        assert(memcmp(pixels, next, 30 * NPE_WIDTH) == 0);
        for (unsigned row = 46; row < NPE_HEIGHT; ++row)
            assert(memcmp(pixels + row * NPE_WIDTH,
                          next + row * NPE_WIDTH, NPE_WIDTH) == 0);
        if (file.size == 2388678 &&
            memcmp(file.title,
                   "\xb0\xa2\xc5\xb5\xb4\xb4\xca\xc0\xbc\xcd", 10) == 0) {
            const unsigned slot_levels[3] = {1, 0, 0};
            uint64_t slot_hash = UINT64_C(14695981039346656037);
            NpeIndexedBitmap icon;
            NpeMap first_map;
            NpeWorld arno = {0};
            NpeItem full_description, full_name;
            const unsigned id = 83, quantity = 1;
            unsigned usable = 0, count;
            assert(npe_render_native_title_screen(&file, font, 1, next) == 0);
            assert(npe_render_save_slots_overlay(&file, 158, 45,
                                                  slot_levels, 0,
                                                  font->data16, next) == 0);
            for (size_t i = 0; i < sizeof(next); ++i)
                slot_hash = (slot_hash ^ next[i]) *
                            UINT64_C(1099511628211);
            /* Original Arno title continue with first slot, 240x120 LCD. */
            assert(slot_hash == UINT64_C(0x4aea758929026dbd));
            assert(npe_map(&file, 0, &first_map) == 0);
            assert(npe_map_group_count(&first_map, 11, &count) == 0 &&
                   count == 3);
            assert(npe_map_group_count(&first_map, 14, &count) == 0 &&
                   count == 1);
            assert(npe_item(&file, 9, &full_description) == 0);
            assert(full_description.description_size == 100);
            assert(npe_item(&file, 174, &full_name) == 0);
            assert(full_name.name_size == 12);
            assert(npe_item_bitmap(&file, 82, &icon) == 0);
            assert(icon.width == 16 && icon.height == 32);
            assert(npe_render_item_list_screen(&file, font, &id,
                   &quantity, 1, 0, next) == 0);
            assert(npe_world_new_game(&file, &arno) == 0);
            for (unsigned index = 0;
                 index < file.offset_table_records[2]; ++index) {
                NpeItem item;
                if (npe_item(&file, index, &item)) continue;
                if (item.effect_type != 1 && item.effect_type != 2)
                    continue;
                assert(npe_render_native_item_target_screen(&file, font,
                       &arno, 0, index + 1, 1, next) == 0);
                ++usable;
            }
            assert(usable == 42);
        }
        if (file.size == 2588702 &&
            memcmp(file.title,
                   "\xbd\xad\xba\xfe\xc6\xf4\xca\xbe\xc2\xbc", 10) == 0) {
            NpeIndexedBitmap icon;
            NpeMap map;
            NpeMapPartyAdjustEvent adjustment;
            NpeWorld tianji = {0};
            NpeItem full_name;
            unsigned count;
            assert(npe_map(&file, 51, &map) == 0);
            assert(npe_map_group_count(&map, 3, &count) == 0 &&
                   count == 1);
            assert(npe_map_party_adjust_event(&map, 1, &adjustment) == 1);
            assert(adjustment.character_id == 2 &&
                   adjustment.next_mode == 1 && adjustment.next_id == 1);
            assert(npe_item(&file, 174, &full_name) == 0);
            assert(full_name.name_size == 12);
            assert(npe_item_bitmap(&file, 159, &icon) == 0);
            assert(icon.width == 32 && icon.height == 33);
            assert(npe_world_new_game(&file, &tianji) == 0);
            assert(npe_render_native_item_target_screen(&file, font,
                   &tianji, 0, 160, 1, next) == 0);
            {
                NpeWorld inactive = tianji, reordered = tianji;
                uint16_t first_hp = reordered.hp[0];
                uint16_t second_hp = reordered.hp[1];
                inactive.map_number = reordered.map_number = 52;
                assert(!npe_world_load_party_adjust_event(&inactive,&map));
                assert(!npe_world_load_party_adjust_event(&reordered,&map));
                inactive.game_mode = reordered.game_mode = 13;
                inactive.story_event_id = reordered.story_event_id = 1;
                assert(npe_world_apply_mode13_event(&file, &inactive) == 0);
                assert(inactive.game_mode == 0 &&
                       inactive.party_ids[0] == 1 &&
                       inactive.party_ids[1] == 2);
                reordered.party_active[1] = 1;
                assert(npe_world_apply_mode13_event(&file, &reordered) == 1);
                assert(reordered.party_ids[0] == 2 &&
                       reordered.party_ids[1] == 1 &&
                       reordered.hp[0] == second_hp &&
                       reordered.hp[1] == first_hp &&
                       reordered.game_mode == 1 &&
                       reordered.story_event_id == 1);
                reordered = tianji;
                assert(!npe_world_load_party_adjust_event(&reordered,&map));
                reordered.map_number = 52;
                reordered.game_mode = 13;
                reordered.story_event_id = 1;
                reordered.party_ids[1] = 3;
                reordered.party_ids[2] = 2;
                reordered.party_active[2] = 1;
                first_hp = reordered.hp[0];
                second_hp = reordered.hp[2];
                assert(npe_world_apply_mode13_event(&file, &reordered) == 1);
                assert(reordered.party_ids[0] == 2 &&
                       reordered.party_ids[2] == 1 &&
                       reordered.hp[0] == second_hp &&
                       reordered.hp[2] == first_hp);
                reordered = tianji;
                reordered.map_number = 52;
                reordered.game_mode = 13;
                reordered.story_event_id = 2;
                reordered.party_active[1] = 1;
                assert(npe_world_apply_mode13_event(&file, &reordered) == 0);
                assert(reordered.party_ids[0] == 1 &&
                       reordered.game_mode == 0);
            }
        }
        if (file.size == 2143520 &&
            memcmp(file.title,
                   "\xc8\xfd\xb7\xd6\xcc\xec\xcf\xc2", 8) == 0) {
            NpeMap map;
            NpeMapPartyAdjustEvent adjustment;
            unsigned count;
            assert(npe_map(&file, 20, &map) == 0);
            assert(npe_map_group_count(&map, 3, &count) == 0 &&
                   count == 1);
            assert(npe_map_party_adjust_event(&map, 1, &adjustment) == 1);
            assert(memcmp(adjustment.raw,
                   "\x00\x00\x00\x02\x0e\x00\x01", 7) == 0);
            assert(adjustment.character_id == 2 &&
                   adjustment.next_mode == 14 && adjustment.next_id == 1);
            assert(npe_map(&file, 30, &map) == 0);
            assert(npe_map_group_count(&map, 3, &count) == 0 &&
                   count == 1);
        }
        memset(pixels, 0, sizeof(pixels));
        assert(npe_draw_gbk_text(font,
               (const uint8_t *)"\xcc\xe5:96/130", 9,
               140, 88, pixels) == 0);
        assert(npe_draw_gbk_text(font,
               (const uint8_t *)"\xc6\xf8:60/60", 8,
               140, 103, pixels) == 0);
    }
    if (title_only) {
        const unsigned empty_levels[3] = {0, 0, 0};
        /* The continue panel must come from each game's own NPE bitmaps. */
        assert(npe_render_native_title_screen(&file, font, 1, pixels) == 0);
        assert(npe_render_save_slots_overlay(&file, 158, 45,
                   empty_levels, 0, font->data16, pixels) == 0);
        printf("NPE title bytes: ");
        for (unsigned i = 0; i < 12 && file.title[i]; ++i)
            printf("%02X", file.title[i]);
        putchar('\n');
        npe_free(&file);
        return;
    }
    for (npc_id = 1; npc_id <= 250; ++npc_id) {
        NpeNpcAction action;
        if (npe_npc_action(&file, npc_id, 0, &action)) continue;
        for (case_index = 0; case_index < action.case_count; ++case_index) {
            unsigned variant;
            assert(npe_npc_action(&file, npc_id, case_index, &action) == 0);
            for (variant = 0; variant < 3; ++variant) {
                NpeDialoguePage dialogue;
                assert(npe_dialogue_page(&file,
                       action.dialogue_offset[variant], &dialogue) == 0);
                pages += check_page(font, dialogue.bytes, dialogue.size,
                                    "npc", npc_id);
            }
        }
    }
    for (story_index = 0; story_index < file.offset_table_records[10];
         ++story_index) {
        NpeStoryRecord story;
        assert(npe_story_record(&file, story_index, &story) == 0);
        pages += check_page(font, story.text + 1, story.text_size - 1,
                            "story", story_index + 1);
    }
    for (item_index = 0; item_index < file.offset_table_records[2];
         ++item_index) {
        NpeItem item;
        uint8_t *message;
        size_t size;
        if (npe_item(&file, item_index, &item)) continue;
        size = item.name_size + 1 + item.description_size;
        message = malloc(size);
        assert(message);
        memcpy(message, item.name, item.name_size);
        message[item.name_size] = '\n';
        memcpy(message + item.name_size + 1, item.description,
               item.description_size);
        memset(pixels, 0, sizeof(pixels));
        if (npe_render_gbk_dialogue(font, message, size, 0, pixels)) {
            fprintf(stderr, "item %u failed: name=%zu description=%zu\n",
                    item_index + 1, item.name_size, item.description_size);
            assert(0);
        }
        free(message);
        items++;
    }
    {
        const uint8_t *section = file.bytes + file.sections[15].offset;
        unsigned count = (unsigned)section[0] << 8 | section[1];
        for (quiz_index = 0; quiz_index < count; ++quiz_index) {
            NpeQuizQuestion question;
            assert(npe_quiz_question(&file, quiz_index, &question) == 0);
            memset(pixels, 0, sizeof(pixels));
            assert(npe_render_gbk_dialogue(font, question.text[0],
                   question.text_size[0], 1, pixels) == 0);
            memset(pixels, 0, sizeof(pixels));
            assert(npe_render_gbk_quiz_answers(font, &question, pixels) == 0);
            quizzes++;
        }
    }
    if (memcmp(file.title,
               "\xcc\xec\xbd\xb5\xb7\xe7\xd4\xc6\xc2\xbc", 10) == 0) {
        assert(pages > 1000);
        assert(native_fallback_pages == 0);
    } else assert(pages > 0);
    printf("%u dialogue pages, %u items, %u quizzes rendered; "
           "native 16px pages %u, fallback %u\n",
           pages, items, quizzes, native_pages, native_fallback_pages);
    npe_free(&file);
}

int main(int argc, char **argv) {
    static const uint8_t title[] = {0xcc, 0xec, 0xbd, 0xb5,
                                    0xb7, 0xe7, 0xd4, 0xc6};
    NpeFont font = {0};
    uint8_t chinese[NPE_WIDTH * NPE_HEIGHT] = {0};
    uint8_t latin[NPE_WIDTH * NPE_HEIGHT] = {0};
    uint8_t long_text[96];
    unsigned x, y, dark = 0;
    assert(argc == 3 || argc == 4 ||
           (argc == 5 && (strcmp(argv[4], "--title-only") == 0 ||
                         strcmp(argv[4], "--text16-bytes") == 0)));
    assert(npe_font_load(&font, argv[1]) == 0);
    assert(npe_font_load16(&font, argv[2]) == 0);
    if (argc == 5 && !strcmp(argv[4], "--text16-bytes")) {
        check_text16_bytes(&font,argv[3]);npe_font_free(&font);return 0;
    }
    {
        uint8_t backdrop[NPE_WIDTH * NPE_HEIGHT];
        memset(backdrop, 7, sizeof(backdrop));
        assert(npe_render_native_dialogue(&font,
               (const uint8_t *)"ABC\nDEF", 7, backdrop) == 0);
        assert(backdrop[83 * NPE_WIDTH] == 7);
        assert(backdrop[84 * NPE_WIDTH] == 0);
        assert(backdrop[119 * NPE_WIDTH + 239] == 7);
    }
    {
        uint8_t empty[NPE_WIDTH * NPE_HEIGHT];
        uint64_t checksum = UINT64_C(14695981039346656037);
        assert(npe_render_empty_inventory_screen(&font, empty) == 0);
        for (size_t i = 0; i < sizeof(empty); ++i)
            checksum = (checksum ^ empty[i]) * UINT64_C(1099511628211);
        /* Native empty-inventory LCD, full 240x120 frame. */
        assert(checksum == UINT64_C(0x875307cb422c7d78));
    }
    {
        uint8_t empty[NPE_WIDTH * NPE_HEIGHT];
        uint64_t checksum = UINT64_C(14695981039346656037);
        assert(npe_render_empty_magic_screen(&font, empty) == 0);
        for (size_t i = 0; i < sizeof(empty); ++i)
            checksum = (checksum ^ empty[i]) * UINT64_C(1099511628211);
        /* Native no-learned-magic LCD, full 240x120 frame. */
        assert(checksum == UINT64_C(0xc11dd69686a50b16));
    }
    {
        const size_t dash = 8 + (95 + 2 * 94 + 12) * 32;
        /* The firmware's fullwidth dash is one 9-pixel stroke on row 7. */
        assert(font.data16[dash + 14] == 0x1f);
        assert(font.data16[dash + 15] == 0xf0);
    }
    {
        static const uint8_t body[] = {0xcc, 0xe5};
        static const uint8_t qi[] = {0xc6, 0xf8};
        uint8_t hud[NPE_WIDTH * NPE_HEIGHT] = {0};
        uint64_t checksum = UINT64_C(14695981039346656037);
        unsigned row, col;
        assert(npe_draw_gbk_text(&font, body, sizeof(body),
                                 70, 88, hud) == 0);
        assert(npe_draw_gbk_text(&font, qi, sizeof(qi),
                                 70, 104, hud) == 0);
        npe_render_battle_hud_values(86, 130, 60, 60, hud);
        for (row = 88; row < 116; ++row)
            for (col = 70; col < 126; ++col) {
                checksum ^= hud[row * NPE_WIDTH + col];
                checksum *= UINT64_C(1099511628211);
            }
        /* Original 2026-10-01 battle LCD crop at x70..125, y88..115. */
        assert(checksum == UINT64_C(0xac8b2c11686fd82d));
    }
    {
        static const uint8_t split[] = {0x81, 0x7b, 'X', '{', 'Y'};
        assert(npe_gbk_page_end(split, sizeof(split), 0) == 3);
        assert(npe_gbk_page_end(split, sizeof(split), 4) == sizeof(split));
    }
    assert(npe_render_gbk_dialogue(&font, title, sizeof(title), 1,
                                   chinese) == 0);
    {
        uint8_t lf[NPE_WIDTH * NPE_HEIGHT] = {0};
        uint8_t crlf[NPE_WIDTH * NPE_HEIGHT] = {0};
        assert(npe_render_gbk_dialogue(&font,
               (const uint8_t *)"ABC\nDEF", 7, 0, lf) == 0);
        assert(npe_render_gbk_dialogue(&font,
               (const uint8_t *)"ABC\r\nDEF", 8, 0, crlf) == 0);
        assert(memcmp(lf, crlf, sizeof(lf)) == 0);
    }
    assert(npe_render_gbk_dialogue(&font, (const uint8_t *)"ABCD", 4, 0,
                                   latin) == 0);
    assert(memcmp(chinese, latin, sizeof(chinese)) != 0);
    for (y = 89; y < 107; ++y)
        for (x = 6; x < 54; ++x)
            dark += chinese[y * NPE_WIDTH + x] == 15;
    assert(dark > 30);
    {
        /* Original C-font lookup consumes a trailing lead plus zero and draws
           A1A1, as independently sampled by reference_text12_bytes.py. */
        static const uint8_t fallback[] = {0xa1, 0xa1};
        uint8_t truncated[NPE_WIDTH * NPE_HEIGHT] = {0};
        uint8_t replacement[NPE_WIDTH * NPE_HEIGHT] = {0};
        assert(!npe_render_gbk_dialogue(&font, title, 1, 0, truncated));
        assert(!npe_render_gbk_dialogue(&font, fallback, sizeof(fallback), 0, replacement));
        assert(!memcmp(truncated, replacement, sizeof(truncated)));
    }
    {
        NpeQuizQuestion quiz = {0};
        unsigned choice;
        for (choice = 1; choice <= 4; ++choice) {
            quiz.text[choice] = title;
            quiz.text_size[choice] = sizeof(title);
        }
        assert(npe_render_gbk_quiz_answers(&font, &quiz, latin) == 0);
    }
    memset(long_text, 'A', sizeof(long_text));
    assert(npe_render_gbk_dialogue(&font, long_text, sizeof(long_text), 0,
                                   latin) == -1);
    if (argc >= 4) check_real_game(&font, argv[3], argc == 5);
    npe_font_free(&font);
    puts("NPE GBK bitmap dialogue passed");
    return 0;
}
