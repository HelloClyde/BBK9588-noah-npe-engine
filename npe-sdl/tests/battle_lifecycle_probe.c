#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_lifecycle_native.h"

/* RAM-only native fixtures have matching isolated C worlds. They never
   feed the continuous story replay and never edit source game bytes. */
static void probe(const NpeFile *file) {
    unsigned records = 0;
    for (unsigned i = 0; i < sizeof(battle_lifecycle_native) /
                            sizeof(battle_lifecycle_native[0]); ++i) {
        NpeWorld world;
        unsigned count = battle_lifecycle_native[i].count;
        const char *route = battle_lifecycle_native[i].route;
        assert(npe_world_init(file, &world, 9, 29, 13) == 0);
        for (unsigned m = 0; m < 3; ++m) {
            world.party_active[m] = m < count;
            if (m < count) {
                memcpy(world.status_guard[m], battle_lifecycle_native[i].before[m],
                       sizeof(world.status_guard[m]));
                world.equipped_items[m][2] = m < 2 ? 26 : 0;
                world.equipped_items[m][5] = m < 2 ? (m ? 91 : 85) : 0;
                world.hp[m] = 130;
            } else {
                for (unsigned s = 0; s < 4; ++s) world.status_guard[m][s] = 123 + s;
            }
            for (unsigned s = 0; s < 4; ++s) world.battle_party_status[m][s] = -6;
            world.battle_attack_turns[m] = world.battle_defense_turns[m] = 7;
        }
        world.game_mode = 3; world.story_event_id = 1;
        assert(npe_world_prepare_scripted_battle(file, &world) == 1);
        for (unsigned m = 0; m < count; ++m) {
            assert(memcmp(world.status_guard[m], battle_lifecycle_native[i].entry[m],
                          sizeof(world.status_guard[m])) == 0);
            for (unsigned s = 0; s < 4; ++s) assert(!world.battle_party_status[m][s]);
            assert(!world.battle_attack_turns[m] && !world.battle_defense_turns[m]);
            world.hp[m] = world.max_hp[m] = !strcmp(route, "loss") ? 1 : 2000;
            world.attack[m] = !strcmp(route, "loss") ? 0 : 9999;
            world.other_stats[m][1] = !strcmp(route, "loss") ? 1 : 999;
            for (unsigned s = 0; s < 4; ++s)
                world.battle_party_status[m][s] = (int16_t)battle_lifecycle_native[i].input[m][s];
            world.battle_attack_turns[m] = battle_lifecycle_native[i].input[m][4];
            world.battle_defense_turns[m] = battle_lifecycle_native[i].input[m][5];
            if (!strcmp(route, "win-fallen") && m == 1) world.hp[m] = 0;
        }
        if (!strncmp(route, "flee", 4)) {
            world.story_event_id = 0; world.random_state = 1;
            assert(npe_world_battle_flee(&world) == NPE_BATTLE_FLED);
        } else if (!strcmp(route, "loss")) {
            world.battle_enemy_base_attacks[1] = world.battle_enemy_attacks[1] = 9999;
            world.battle_enemy_speeds[1] = 999;
            /* Actual command execution, including enemy round and defeat. */
            for (unsigned n = 0; n < 30 && world.game_mode == 3; ++n)
                assert(npe_world_battle_attack(&world, 1) >= 0);
            assert(world.battle_lost);
        } else {
            world.battle_enemy_hps[1] = world.battle_enemy_hp = 1;
            for (unsigned n = 0; n < 12 && world.game_mode == 3; ++n)
                assert(npe_world_battle_attack(&world, 1) >= 0);
            assert(world.game_mode != 3 && !world.battle_lost);
            if (!strcmp(route, "win-fallen")) assert(world.hp[1] == 1 && world.experience[1] == 0);
        }
        for (unsigned m = 0; m < count; ++m) {
            assert(memcmp(world.status_guard[m], battle_lifecycle_native[i].exit[m],
                          sizeof(world.status_guard[m])) == 0);
            for (unsigned s = 0; s < 4; ++s)
                assert((uint16_t)world.battle_party_status[m][s] == battle_lifecycle_native[i].battle[m][s]);
            assert(world.battle_attack_turns[m] == battle_lifecycle_native[i].battle[m][4]);
            assert(world.battle_defense_turns[m] == battle_lifecycle_native[i].battle[m][5]);
            ++records;
        }
        for (unsigned m = count; m < 3; ++m)
            for (unsigned s = 0; s < 4; ++s) assert(world.status_guard[m][s] == 123 + s);
        NpeWorld restored;
        /* The host codec accepts map saves only. Verify the already
           observed exit words in a separate map-mode copy; this does
           not claim a story transition or a title-screen save. */
        NpeWorld map_save = world; map_save.game_mode = 1;
        const char *path = "tmp/battle-lifecycle-save.npes";
        assert(npe_world_save(file, &map_save, path) == 0 && npe_world_load(file, &restored, path) == 0);
        assert(memcmp(world.status_guard, restored.status_guard, sizeof(world.status_guard)) == 0);
        assert(remove(path) == 0);
        fprintf(stderr, "Original lifecycle %s/%u entry, exit and save passed\n", route, count);
    }
    fprintf(stderr, "Original lifecycle %u exit party records passed\n", records);
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *stream = fopen(argv[1], "rb"); assert(stream);
    assert(fseek(stream, 0, SEEK_END) == 0); long size = ftell(stream);
    assert(size > 0 && fseek(stream, 0, SEEK_SET) == 0);
    uint8_t *bytes = malloc((size_t)size); assert(bytes);
    assert(fread(bytes, 1, (size_t)size, stream) == (size_t)size && fclose(stream) == 0);
    NpeFile file; char error[120];
    assert(npe_parse(&file, bytes, (size_t)size, error, sizeof(error)) == 0);
    assert(npe_game_fingerprint(&file) == UINT64_C(0x114861a601117d94));
    probe(&file); free(bytes); return 0;
}
