#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}
int main(void) {
    enum { BASE = 0x35a, S0 = BASE, S1 = S0 + 8,
           S8 = S1 + 138 + 6, MAP_SIZE = 8 + 17 + 8,
           FIXED_TAIL = 703, END = S8 + MAP_SIZE + FIXED_TAIL };
    uint8_t *blob = calloc(END, 1);
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    NpeFile file;
    NpeMap map;
    NpeMapExit exit;
    NpeViewport viewport;
    NpeWorld world;
    unsigned npc_id;
    const uint8_t *raw;
    char error[120];
    unsigned i, cursor = S1 + 138;
    /* Independent original-runtime samples: skill4 effect60 with ATK60
       gave80 damage; ATK120 gave100, without defense/variance deduction. */
    assert(npe_battle_party_spell_damage(60, 60) == 80);
    assert(npe_battle_party_spell_damage(60, 120) == 100);
    assert(npe_battle_party_spell_damage(60, (uint16_t)-3) == 59);
    assert(npe_battle_party_spell_damage(0, 0) == 12);
    assert(npe_battle_party_spell_damage(32767, 3) == 12);
    assert(npe_battle_party_spell_damage(65535, 6) == 1);
    {
        NpeWorld targets = {0};
        targets.party_active[0] = targets.party_active[1] =
            targets.party_active[2] = 1;
        targets.hp[0] = targets.hp[1] = targets.hp[2] = 100;
        assert(npe_battle_party_target(&targets, 0) == 0);
        assert(npe_battle_party_target(&targets, 1) == 1);
        assert(npe_battle_party_target(&targets, 2) == 2);
        /* Native reduces by100 before the party count, unlike roll%3. */
        assert(npe_battle_party_target(&targets, 100) == 0);
        assert(npe_battle_party_target(&targets, 101) == 1);
        targets.hp[0] = 0;
        assert(npe_battle_party_target(&targets, 0) == 1);
        assert(npe_battle_party_target(&targets, 2) == 2);
        targets.hp[2] = 0;
        assert(npe_battle_party_target(&targets, 2) == 1);
        targets.party_active[0] = targets.party_active[1] = 0;
        targets.hp[2] = 100;
        assert(npe_battle_party_target(&targets, 99) == 2);
        targets.hp[2] = 0;
        assert(npe_battle_party_target(&targets, 99) == -1);
        assert(npe_battle_party_target(NULL, 0) == -1);
    }
    assert(blob);
    {
        NpeWorld queue = {0};
        queue.party_active[0] = queue.party_active[2] = 1;
        queue.hp[0] = queue.hp[2] = 100;
        queue.battle_enemy_count = 2;
        queue.battle_enemy_hps[0] = queue.battle_enemy_hps[1] = 100;
        queue.other_stats[0][1] = 20;
        queue.other_stats[2][1] = 30;
        queue.battle_enemy_speeds[0] = 40;
        queue.battle_enemy_speeds[1] = 30;
        assert(npe_battle_next_actor(&queue, 0) == 3);
        assert(npe_battle_next_actor(&queue, 1u << 3) == 2);
        assert(npe_battle_next_actor(&queue, (1u << 3) | (1u << 2)) == 4);
        queue.battle_enemy_status[0][1] = -1;
        queue.battle_party_status[2][2] = UINT16_MAX;
        assert(npe_battle_next_actor(&queue, 0) == 4);
        queue.battle_enemy_hps[1] = 0;
        assert(npe_battle_next_actor(&queue, 0) == 0);
        queue.hp[0] = 0;
        assert(npe_battle_next_actor(&queue, 0) == -1);
        assert(npe_battle_next_actor(NULL, 0) == -1);
    }
    memcpy(blob, "NP560 RPG2.1", 12);
    blob[0x38] = (uint8_t)(S1 >> 8); blob[0x39] = (uint8_t)S1;
    be32(blob + 0x3a, S0); be32(blob + 0x3e, 8);
    be32(blob + 0x42, S1); be32(blob + 0x46, 138);
    for (i = 2; i < 8; ++i) {
        be32(blob + 0x3a + i * 8, cursor++);
        be32(blob + 0x3e + i * 8, 1);
    }
    /* Include the formations, random-enemy list and empty event directory
       read by the original full map loader after the twelve exit records. */
    be32(blob + 0x3a + 8 * 8, S8); be32(blob + 0x3e + 8 * 8, MAP_SIZE + FIXED_TAIL);
    be32(blob + S0, 0); be32(blob + S0 + 4, 138);
    blob[S1] = 1; blob[S1 + 1] = 0x10;
    blob[S1 + 6] = 16; blob[S1 + 8] = 16;
    memset(blob + S1 + 10, 0xab, 128);
    be32(blob + S8, 8); be32(blob + S8 + 4, UINT32_MAX);
    blob[S8 + 8] = 0; blob[S8 + 9] = 1; /* map ID */
    blob[S8 + 10] = 2; blob[S8 + 11] = 2; /* dimensions */
    blob[S8 + 25] = 0; blob[S8 + 26] = 0; /* tile index */
    blob[S8 + MAP_SIZE] = 42; /* first object */
    blob[S8 + MAP_SIZE + 81] = 7; /* first NPC ID */
    blob[S8 + MAP_SIZE + 340] = 3; /* exit x */
    blob[S8 + MAP_SIZE + 341] = 4; /* exit y */
    blob[S8 + MAP_SIZE + 343] = 0;
    blob[S8 + MAP_SIZE + 344] = 2; /* map 2 */
    blob[S8 + MAP_SIZE + 347] = 5;
    blob[S8 + MAP_SIZE + 348] = 6;
    blob[S8 + MAP_SIZE + 340 + 21] = 1; /* second exit x */
    blob[S8 + MAP_SIZE + 341 + 21] = 1; /* second exit y */
    blob[S8 + MAP_SIZE + 344 + 21] = 1; /* map 1 */
    assert(npe_parse(&file, blob, END, error, sizeof(error)) == 0);
    assert(npe_map(&file, 0, &map) == 0);
    assert(map.number == 1 && map.header_word == 1 &&
           map.width == 2 && map.height == 2);
    assert(npe_map_tile(&map, 0, 0) == 0);
    assert(npe_map_tile(&map, 1, 0) == 0);
    assert(npe_map_tile(&map, 2, 0) == UINT16_MAX);
    assert(npe_tile_base_passable(0x000b));
    assert(!npe_tile_base_passable(0x000c));
    assert(npe_tile_base_passable(0x017f));
    assert(npe_tile_base_passable(0x0182));
    assert(npe_tile_base_passable(0x0183));
    assert(!npe_tile_base_passable(0x0184));
    assert(npe_tile_base_passable(0x0288));
    assert(npe_tile_base_passable(0x0210));
    assert(npe_tile_base_passable(0x0215));
    assert(!npe_tile_base_passable(0x0216));
    assert(npe_tile_base_passable(0x026b));
    assert(npe_tile_base_passable(0x0275));
    assert(!npe_tile_base_passable(0x0276));
    assert(npe_tile_base_passable(0x028e));
    assert(npe_tile_base_passable(0x0290));
    assert(npe_tile_base_passable(0x03b0));
    assert(npe_tile_base_passable(0x03b4));
    assert(npe_tile_base_passable(0x0405));
    assert(npe_tile_base_passable(0x0416));
    assert(npe_tile_base_passable(0x0426));
    assert(npe_tile_base_passable(0x0446));
    assert(!npe_tile_base_passable(0x0447));
    assert(npe_map_object(&map, 0, &raw) == 0 && raw[0] == 42);
    assert(npe_map_npc(&map, 0, &raw) == 0 && raw[1] == 7);
    assert(npe_map_exit(&map, 0, &exit) == 0);
    assert(exit.x == 3 && exit.y == 4 && exit.destination_map == 2);
    assert(exit.destination_x == 5 && exit.destination_y == 6);
    assert(npe_map_exit(&map, 12, &exit) == -1);
    map.width = 25; map.height = 15;
    assert(npe_map_spawn_viewport(&map, 12, 13, &viewport) == 0);
    assert(viewport.screen_x == 7 && viewport.offset_x == 5);
    assert(viewport.screen_y == 4 && viewport.offset_y == 8);
    /* Original48:2290 accepts an out-of-grid coordinate, storing bytes. */
    assert(npe_map_spawn_viewport(&map, 25, 13, &viewport) == 0);
    assert(viewport.screen_x == 15 && viewport.offset_x == 10);
    map.width = 2; map.height = 2;
    assert(npe_world_init(&file, &world, 1, 0, 0) == 0);
    assert(world.view.offset_x == 243 && world.view.screen_x == 13);
    assert(world.view.offset_y == 251 && world.view.screen_y == 4);
    /* This synthetic2x2 numeric movement fixture uses an explicit viewport;
       it is independent of the original wraparound spawn check above. */
    memset(&world.view, 0, sizeof(world.view));
    assert(npe_world_step(&file, &world, 1) == NPE_MOVE_OK);
    assert(world.x == 1 && world.y == 0 && world.direction == 1);
    assert(npe_world_step(&file, &world, 1) == NPE_MOVE_BLOCKED);
    assert(npe_world_step(&file, &world, 0) == NPE_MOVE_EXIT);
    assert(world.map_number == 1 && world.x == 0 && world.y == 0);
    world.x = 1; world.direction = 3;
    /* Facing selection uses the viewport, as the original33:3772 does.
       Keep this diagnostic teleport's two position representations aligned. */
    memset(&world.view, 0, sizeof(world.view));
    world.view.screen_x = 1;
    assert(npe_world_facing_npc(&file, &world, &npc_id) == 1);
    assert(npc_id == 7);
    assert(npe_render_map(&file, &map, 0, 0, pixels) == 0);
    assert(pixels[0] == 2 && pixels[NPE_WIDTH * 3] == 2);
    assert(pixels[NPE_WIDTH * 4] == 10 &&
           pixels[NPE_WIDTH * 4 + 1] == 11);
    assert(pixels[NPE_WIDTH * 4 + 16] == 10 &&
           pixels[NPE_WIDTH * 4 + 32] == 2 &&
           pixels[NPE_WIDTH * 116] == 2 &&
           pixels[NPE_WIDTH * 119] == 2);
    {
        uint8_t tail[723] = {0};
        NpeMap indexed = map;
        NpeMapChase chase;
        const uint8_t records[20] = {
            0, 1, 0, 94, 0, 96, 0, 89, 9, 12,
            0, 2, 0, 0, 0, 0, 1, 2, 200, 201
        };
        indexed.tail = tail; indexed.tail_size = sizeof(tail);
        tail[652 + 10 * 3 + 2] = 2;
        memcpy(tail + 703, records, sizeof(records));
        assert(npe_map_chase(&indexed, 0, &chase) == 1 &&
               chase.graphic == 1 && chase.min_progress == 94 &&
               chase.max_progress == 96 && chase.enemy_id == 89 &&
               chase.x == 9 && chase.y == 12);
        assert(npe_map_chase(&indexed, 1, &chase) == 1 &&
               chase.graphic == 2 && chase.min_progress == 0 &&
               chase.max_progress == 0 && chase.enemy_id == 258 &&
               chase.x == 200 && chase.y == 201);
        assert(npe_map_chase(&indexed, 2, &chase) == -1);
        indexed.tail_size--;
        assert(npe_map_chase(&indexed, 0, &chase) == -1);
        indexed.tail_size++;
        tail[652 + 10 * 3 + 2] = 3;
        /* Original0C:EA9C caps the loop at two slots and does not read
           or skip a third record even when the directory count is larger. */
        assert(npe_map_chase(&indexed, 0, &chase) == 1);
        assert(npe_map_chase(&indexed, 1, &chase) == 1);
        assert(npe_map_chase(&indexed, 2, &chase) == -1);
    }
    {
        uint8_t tail[730] = {0};
        NpeMap changed = map;
        NpeMapTileChange changes[3];
        NpeWorld runtime = {0};
        unsigned count;
        const uint8_t records[] = {0,10,1, 0,1,0,12, 0,5,1, 0,1,0,10};
        changed.tail = tail; changed.tail_size = 703 + sizeof(records);
        tail[654] = 2;
        memcpy(tail + 703, records, sizeof(records));
        assert(npe_map_tile_changes(&changed, changes, &count) == 0 && count == 2 &&
               changes[0].progress == 10 && changes[1].progress == 5 && changes[0].indices[0] == 1);
        assert(npe_world_load_tiles(&runtime, &changed) == 0);
        runtime.story_progress = 5;
        assert(npe_world_update_tiles(&runtime) == 1 && runtime.tile_change_applied == 2 &&
               npe_world_tile(&runtime, &changed, 1, 0) == 10);
        runtime.story_progress = 10;
        assert(npe_world_update_tiles(&runtime) == 1 && runtime.tile_change_applied == 3 &&
               npe_world_tile(&runtime, &changed, 1, 0) == 12);
        runtime.story_progress = 4;
        assert(npe_world_update_tiles(&runtime) == 0 && npe_world_tile(&runtime, &changed, 1, 0) == 12);
        runtime.story_progress = 10;
        assert(npe_world_load_tiles(&runtime, &changed) == 0 && runtime.tile_change_applied == 0 &&
               npe_world_tile(&runtime, &changed, 1, 0) == 10 && npe_map_tile(&changed, 1, 0) == 0);
        assert(npe_world_update_tiles(&runtime) == 2 && npe_world_tile(&runtime, &changed, 1, 0) == 10);
        changed.tail_size--;
        assert(npe_map_tile_changes(&changed, changes, &count) == -1);
        changed.tail_size++;
        tail[705] = 16;
        assert(npe_map_tile_changes(&changed, changes, &count) == -1);
        tail[705] = 1; tail[707] = 4;
        assert(!npe_map_tile_changes(&changed, changes, &count) && changes[0].indices[0] == 4);
        tail[706] = 255; tail[707] = 255;
        assert(!npe_map_tile_changes(&changed, changes, &count) && changes[0].indices[0] == 65535);
        tail[706] = 0;
        tail[707] = 1; tail[654] = 4;
        assert(npe_map_tile_changes(&changed, changes, &count) == -1);
        {
            const uint8_t sentinel[] = {0,1,3, 0,1,0,4, 0,0,0,5, 0,2,0,6};
            tail[654] = 1; changed.tail_size = 703 + sizeof(sentinel);
            memcpy(tail + 703, sentinel, sizeof(sentinel));
            runtime.story_progress = 0;
            assert(npe_world_load_tiles(&runtime, &changed) == 0);
            runtime.story_progress = 1;
            assert(npe_world_update_tiles(&runtime) == 1 &&
                   npe_world_tile(&runtime, &changed, 1, 0) == 4 &&
                   npe_world_tile(&runtime, &changed, 0, 0) == 0 &&
                   npe_world_tile(&runtime, &changed, 0, 1) == 0);
            assert(npe_world_load_tiles(&runtime, &changed) == 0 &&
                   npe_world_tile(&runtime, &changed, 0, 0) == 5 &&
                   npe_world_tile(&runtime, &changed, 0, 1) == 6);
            /* Original42:335C..335E also stops on negative indices. */
            tail[710]=255;tail[711]=255;
            runtime.story_progress=0;
            assert(!npe_world_load_tiles(&runtime,&changed));
            runtime.story_progress=1;
            assert(npe_world_update_tiles(&runtime)==1 && runtime.tile_override_count==1 &&
                   runtime.tile_override_indices[0]==1 && runtime.tile_override_tiles[0]==4);
        }
    }
    npe_free(&file);
    puts("NPE map raster test passed");
    return 0;
}
