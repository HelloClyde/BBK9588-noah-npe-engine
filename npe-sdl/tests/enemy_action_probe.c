#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* All modified states below are isolated rule fixtures. None is loaded by
   the continuous story replay or changes the original NPE bytes. */
static uint32_t advance(uint32_t state, unsigned count) {
    while (count--) state = state * UINT32_C(0x514f111f) + 1;
    return state;
}

static NpeWorld fixture(const NpeFile *file, unsigned enemy_id, unsigned slot) {
    NpeWorld world;
    NpeEnemy enemy;
    assert(npe_world_new_game(file, &world) == 0);
    assert(npe_enemy(file, enemy_id - 1, &enemy) == 0);
    world.game_mode = 3;
    world.story_event_id = 1;
    world.battle_file = file;
    world.hp[0] = world.max_hp[0] = 30000;
    world.battle_enemy_count = 3;
    world.battle_target = (uint8_t)slot;
    world.battle_enemy_ids[slot] = (uint8_t)enemy_id;
    world.battle_enemy_hps[slot] = world.battle_enemy_max_hps[slot] = enemy.max_hp;
    world.battle_enemy_base_attacks[slot] = world.battle_enemy_attacks[slot] = enemy.attack;
    world.battle_enemy_base_defenses[slot] = world.battle_enemy_defenses[slot] = enemy.defense;
    world.battle_enemy_speeds[slot] = enemy.speed;
    world.battle_enemy_lucks[slot] = enemy.luck;
    world.battle_enemy_damage_floor_stats[slot] = enemy.damage_floor_stat;
    world.battle_enemy_spell_stats[slot] = (uint16_t)((unsigned)enemy.raw[23] << 8 | enemy.raw[24]);
    memcpy(world.battle_enemy_skill_ids[slot], enemy.skill_ids, 3);
    world.battle_enemy_hp = enemy.max_hp;
    return world;
}

static int run_enemy_only_round(NpeWorld *world) {
    /* Isolated scheduler fixture: a previously selected defender becomes
       asleep before execution. The scheduler skips its action and the
       -1 sleep word expires at round end. This does not model flee input. */
    assert((int16_t)world->battle_party_status[0][1] >= 0);
    world->battle_party_status[0][1] = (uint16_t)-1;
    return npe_world_battle_defend(world);
}

static void set_entry_status(NpeWorld *world) {
    for (unsigned member = 0; member < 3; ++member) {
        world->status_guard[member][0] = (uint16_t)-2;
        world->status_guard[member][1] = 3;
        world->status_guard[member][2] = (uint16_t)-4;
        world->status_guard[member][3] = 0;
        for (unsigned status = 0; status < 4; ++status)
            world->battle_party_status[member][status] = status % 2 ? 5 : -6;
    }
    world->equipped_items[0][2] = 26;
    world->equipped_items[0][5] = 85;
}

static void assert_entry_status(const NpeWorld *world) {
    for (unsigned member = 0; member < 3; ++member) {
        assert(world->status_guard[member][0] == (member == 0 ? 5 : (uint16_t)-2));
        assert(world->status_guard[member][1] == 3);
        assert(world->status_guard[member][2] == (uint16_t)-4);
        assert(world->status_guard[member][3] == (member == 0 ? 5 : 0));
        for (unsigned status = 0; status < 4; ++status)
            assert(world->battle_party_status[member][status] == 0);
    }
}

static void assert_exit_status(const NpeWorld *world, int victory) {
    for (unsigned member = 0; member < 3; ++member) {
        for (unsigned status = 0; status < 4; ++status) {
            if (victory) assert(world->battle_party_status[member][status] == 0);
            if (world->party_active[member])
                assert(world->status_guard[member][status] ==
                       (uint16_t)world->battle_party_status[member][status]);
        }
        if (!world->party_active[member]) {
            assert(world->status_guard[member][0] == (uint16_t)-2);
            assert(world->status_guard[member][1] == 3);
            assert(world->status_guard[member][2] == (uint16_t)-4);
        }
    }
}

static void probe_entry(const NpeFile *file) {
    NpeWorld world;
    /* Native re-entry clears six battle words, then equipped positive
       effects replace map words. Check all three C entry paths. */
    assert(npe_world_init(file, &world, 9, 29, 13) == 0);
    set_entry_status(&world);
    world.game_mode = 3;
    world.story_event_id = 1;
    assert(npe_world_prepare_scripted_battle(file, &world) == 1);
    assert_entry_status(&world);
    assert(world.battle_enemy_hps[1] == 500);
    assert(world.battle_enemy_status[1][1] == 10);
    assert(world.battle_enemy_status[0][1] == 0 && world.battle_enemy_status[2][1] == 0);

    assert(npe_world_init(file, &world, 80, 16, 3) == 0);
    world.story_progress = 4;
    world.encounter_steps = 999;
    npe_world_seed(&world, 1);
    set_entry_status(&world);
    assert(npe_world_step(file, &world, 1) == NPE_MOVE_EVENT);
    assert_entry_status(&world);
    /* Original ordinary entry does not execute scripted sleep immunity;
       independently captured two/three-enemy entry records confirm0. */
    assert(world.battle_enemy_status[1][1] == 0);
    world.condition_words[69] = 1;
    assert(npe_world_battle_use_item(file, &world, 69, 0) == NPE_BATTLE_CONTINUE);
    assert(world.battle_party_status[0][1] == 5 && world.status_guard[0][1] == 3);
    /* Victory clears battle words before copying them back to the map. */
    world.attack[0] = 999;
    assert(npe_world_battle_attack(&world, world.battle_target) == NPE_BATTLE_WON);
    assert_exit_status(&world, 1);
    {
        NpeWorld restored;
        const char *path = "tmp/tjfy-status-separation.npes";
        assert(npe_world_save(file, &world, path) == 0);
        assert(npe_world_load(file, &restored, path) == 0);
        assert_exit_status(&restored, 1);
        assert(remove(path) == 0);
    }

    /* Exit copies physical battle records. A numeric-only fixture has
       no entered battle bank and cannot test map-record preservation. */
    assert(npe_world_init(file, &world, 9, 29, 13) == 0);
    set_entry_status(&world);
    world.game_mode = 3;
    world.story_event_id = 1;
    assert(npe_world_prepare_scripted_battle(file, &world) == 1);
    assert_entry_status(&world);
    world.story_event_id = 0;
    world.random_state = 1; /* The original first draw allows escape. */
    assert(npe_world_battle_flee(&world) == NPE_BATTLE_FLED);
    assert_exit_status(&world, 0);

    assert(npe_world_init(file, &world, 9, 29, 13) == 0);
    set_entry_status(&world);
    world.game_mode = 3;
    world.story_event_id = 1;
    assert(npe_world_prepare_scripted_battle(file, &world) == 1);
    assert_entry_status(&world);
    world.hp[0] = 1;
    world.random_state = 1;
    assert(npe_world_battle_flee(&world) == NPE_BATTLE_LOST);
    assert_exit_status(&world, 0);

    assert(npe_world_init(file, &world, 105, 9, 8) == 0);
    world.story_progress = 94;
    set_entry_status(&world);
    assert(npe_world_update_chase(file, &world) == 0);
    assert(npe_world_update_chase(file, &world) == 1);
    assert_entry_status(&world);
    assert(world.battle_enemy_status[0][1] == 0 && world.battle_enemy_status[1][1] == 0);
}

static void probe_native_enemy_position(const NpeFile *file) {
    NpeWorld world = fixture(file, 51, 1);
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    char header[40];
    unsigned differences = 0;
    FILE *reference = fopen("tmp/native-revive-target.pgm", "rb");
    assert(reference && fgets(header, sizeof(header), reference) &&
           fgets(header, sizeof(header), reference) && fgets(header, sizeof(header), reference));
    assert(npe_render_battle_world_scene(file, &world, pixels) == 0);
    for (unsigned y = 0; y < NPE_HEIGHT; ++y)
        for (unsigned x = 0; x < NPE_WIDTH; ++x) {
            int value = fgetc(reference);
            assert(value >= 0);
            if (x >= 42 && x < 82 && y >= 38 && y < 88)
                differences += value != 255 - pixels[y * NPE_WIDTH + x] * 17;
        }
    assert(fgetc(reference) == EOF && fclose(reference) == 0);
    fprintf(stderr, "Native middle boar 40x50 area: %u pixel differences\n", differences);
    assert(!differences);
}

static NpeWorld magic_fixture(const NpeFile *file, unsigned skill_id) {
    NpeWorld world = fixture(file, 51, 1);
    world.level[0] = 99;
    world.attack[0] = 60;
    world.other_stats[0][1] = 999;
    world.qi[0] = world.max_qi[0] = 999;
    memset(world.skill_slots[0], 0, sizeof(world.skill_slots[0]));
    world.skill_slots[0][0] = (uint8_t)skill_id;
    world.battle_enemy_status[1][1] = -2;
    world.random_state = 1;
    return world;
}

static void probe_native_party_formation(const NpeFile *file) {
    const char *labels[] = {"entry", "target", "down", "up"};
    for (unsigned count = 1; count <= 3; ++count) {
        NpeWorld world = fixture(file, 51, 1);
        uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
        for (unsigned member = 0; member < count; ++member) {
            world.party_active[member] = 1;
            world.hp[member] = world.max_hp[member] = 130;
        }
        for (unsigned page = 0; page < (count == 1 ? 1u : 4u); ++page) {
            char path[80], header[40];
            unsigned differences = 0, target = page == 2 ? count - 1 : 0;
            FILE *reference;
            snprintf(path, sizeof(path), "tmp/native-party-%u-%s.pgm", count, labels[page]);
            reference = fopen(path, "rb");
            assert(reference && fgets(header, sizeof(header), reference) &&
                   fgets(header, sizeof(header), reference) && fgets(header, sizeof(header), reference));
            assert((page ? npe_render_battle_party_target(file, &world, target, pixels) :
                           npe_render_battle_world_scene(file, &world, pixels)) == 0);
            for (unsigned y = 0; y < NPE_HEIGHT; ++y)
                for (unsigned x = 0; x < NPE_WIDTH; ++x) {
                    int value = fgetc(reference);
                    assert(value >= 0);
                    if (x >= 130) differences += value != 255 - pixels[y * NPE_WIDTH + x] * 17;
                }
            assert(fgetc(reference) == EOF && fclose(reference) == 0);
            fprintf(stderr, "Native party %u %s right110x120: %u differences\n", count, labels[page], differences);
            if (differences) {
                FILE *dump = fopen("tmp/party-render-c.pgm", "wb");
                assert(dump); fputs("P5\n240 120\n255\n", dump);
                for (unsigned i = 0; i < NPE_WIDTH * NPE_HEIGHT; ++i) fputc(255 - pixels[i] * 17, dump);
                fclose(dump);
            }
            assert(!differences);
        }
    }
}

static void probe_party_magic(const NpeFile *file) {
    NpeWorld world = magic_fixture(file, 5);
    /* Two unmodified enemy51 records in an isolated formation. Group
       HP240 + currentATK60/3 hits both, charging QI and preparation once. */
    world.battle_enemy_ids[2] = world.battle_enemy_ids[1];
    world.battle_enemy_hps[2] = world.battle_enemy_max_hps[2] = 500;
    world.battle_enemy_status[2][1] = -2;
    assert(npe_world_battle_cast_magic(file, &world, 5, 1) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_hps[1] == 240 && world.battle_enemy_hps[2] == 240);
    assert(world.qi[0] == 974 && world.random_state == advance(1, 2));

    for (unsigned spell = 9; spell <= 22; spell += 13) {
        world = magic_fixture(file, spell);
        world.hp[0] = 10000;
        world.party_active[1] = world.party_active[2] = 1;
        world.hp[1] = 50; world.max_hp[1] = 120;
        world.hp[2] = 0;
        world.other_stats[1][1] = 998;
        assert(npe_world_battle_cast_magic(file, &world, spell, 0) == NPE_BATTLE_CONTINUE);
        assert(world.battle_actor == 1 && world.qi[0] == 999 && world.hp[0] == 10000);
        assert(npe_world_battle_defend(&world) == NPE_BATTLE_CONTINUE);
        assert(world.hp[0] == 10000 + (spell == 9 ? 200 : 150));
        assert(world.hp[1] == 120 && world.hp[2] == 0);
        assert(world.qi[0] == 999 - (spell == 9 ? 20 : 10));
        assert(world.random_state == advance(1, 4));
    }
    world = magic_fixture(file, 14);
    world.party_active[1] = world.party_active[2] = 1;
    world.hp[1] = 100; world.hp[2] = 0;
    world.other_stats[1][1] = 998;
    assert(npe_world_battle_cast_magic(file, &world, 14, 0) == NPE_BATTLE_CONTINUE);
    assert(npe_world_battle_attack(&world, 1) == NPE_BATTLE_CONTINUE);
    for (unsigned member = 0; member < 2; ++member) {
        assert(world.battle_skill_attack_bonus[member] == 200);
        assert(world.battle_skill_defense_bonus[member] == 50);
        assert(world.battle_attack_turns[member] == 3);
    }
    /* Original native fallen fixture: positive group ATK/DEF also writes
       fallen records; both word counters still decrement during setup. */
    assert(world.battle_attack_turns[2] == 3 && world.battle_defense_turns[2] == 3 &&
           world.battle_skill_attack_bonus[2] == 200 && world.battle_skill_defense_bonus[2] == 50 &&
           world.qi[0] == 981);

    world = magic_fixture(file, 30);
    world.battle_enemy_status[1][1] = 0;
    assert(npe_world_battle_cast_magic(file, &world, 30, 1) == NPE_BATTLE_CONTINUE);
    for (unsigned enemy = 0; enemy < 3; ++enemy) {
        int step = enemy == 1 ? 1 : 0;
        assert(world.battle_enemy_status[enemy][0] == -2 + step);
        assert(world.battle_enemy_status[enemy][1] == (enemy == 1 ? 10 : -1));
        assert(world.battle_enemy_status[enemy][2] == -2 + step);
        assert(world.battle_enemy_status[enemy][3] == -2 + step);
    }
    assert(world.battle_enemy_hps[1] == 500 && world.qi[0] == 979);
    assert(world.status_guard[0][0] == 0);
    world = magic_fixture(file, 30);
    world.battle_enemy_status[1][1] = 10;
    assert(npe_world_battle_cast_magic(file, &world, 30, 1) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_hps[1] == 500 && world.battle_enemy_status[1][1] == 10);
    assert(world.battle_enemy_status[1][0] == -1 && world.battle_enemy_status[1][2] == -1);

    world = magic_fixture(file, 40);
    assert(npe_world_battle_cast_magic(file, &world, 40, 0) == NPE_BATTLE_CONTINUE);
    for (unsigned status = 0; status < 4; ++status)
        assert(world.battle_party_status[0][status] == 3);
    assert(world.qi[0] == 983);

    /* Original42 contains spirit/luck/speed100. Source battle commit
       ignores those fields, so it charges25 with no stat change. */
    world = magic_fixture(file, 42);
    NpeWorld before = world;
    assert(npe_world_battle_cast_magic(file, &world, 42, 0) == NPE_BATTLE_CONTINUE);
    assert(world.qi[0] == 974 && world.hp[0] == before.hp[0]);
    assert(world.attack[0] == before.attack[0] && world.defense[0] == before.defense[0]);
    assert(memcmp(world.other_stats, before.other_stats, sizeof(world.other_stats)) == 0);
    assert(!world.battle_attack_turns[0]);
    puts("Party group damage/heal/buff/status, single QI charge and original42 probes passed");
}

static void probe_revival(const NpeFile *file) {
    NpeWorld world = magic_fixture(file, 1), before;
    NpeSkill skill;
    assert(npe_skill(file, 0, &skill) == 0 && npe_skill_battle_effect(&skill) == 19);
    assert(skill.qi_cost == 80 && skill.raw[37] == 0 && skill.raw[38] == 1);
    /* Actual list entry already loads this shared record. A direct API
       call with an empty cache also changes the cache on refusal. */
    assert(!npe_world_skill(file,&world,1,&skill));
    before = world;
    assert(npe_world_battle_cast_magic(file, &world, 1, 0) == -1);
    assert(memcmp(&before, &world, sizeof(world)) == 0);
    world.party_active[1] = 1; world.hp[1] = 0; world.max_hp[1] = 30000;
    world.qi[1] = 73; world.other_stats[1][1] = 998;
    world.battle_party_status[1][1] = 4;
    world.battle_party_status[1][2] = 5;
    before = world;
    world.qi[0] = 79;
    assert(npe_world_battle_cast_magic(file, &world, 1, 1) == -3);
    assert(world.hp[1] == 0 && world.random_state == 1 && !world.battle_pending_mask);
    world = before;
    assert(npe_world_battle_cast_magic(file, &world, 1, 2) == -1);
    assert(npe_world_battle_cast_magic(file, &world, 1, 1) == NPE_BATTLE_CONTINUE);
    assert(world.hp[1] == 30000 && world.qi[0] == 919 && world.qi[1] == 73);
    assert(world.battle_party_status[1][1] == 4 && world.battle_party_status[1][2] == 5);
    assert(world.battle_last_order_count == 1 && world.battle_last_order[0] == 0);
    assert(world.random_state == advance(1, 2));
    /* With the original zero statuses, revived member joins the next
       command collection, never the action queue that revived it. */
    world = before;
    memset(world.battle_party_status[1], 0, sizeof(world.battle_party_status[1]));
    assert(npe_world_battle_cast_magic(file, &world, 1, 1) == NPE_BATTLE_CONTINUE);
    assert(world.battle_actor == 0 && world.battle_last_order_count == 1);
    assert(npe_world_battle_defend(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_actor == 1 && world.battle_pending_mask == 1);
    /* Two queued revivals retain the selected record: the second pays
       its cost but does not redirect or heal an already revived target. */
    world = magic_fixture(file, 1);
    world.party_active[1] = world.party_active[2] = 1;
    world.hp[1] = 100; world.hp[2] = 0; world.max_hp[2] = 30000;
    world.qi[1] = 999; world.level[1] = 99; world.other_stats[1][1] = 998;
    memset(world.skill_slots[1], 0, sizeof(world.skill_slots[1]));
    world.skill_slots[1][0] = 1;
    assert(npe_world_battle_cast_magic(file, &world, 1, 2) == NPE_BATTLE_CONTINUE);
    assert(world.battle_actor == 1 && world.qi[0] == 999 && world.hp[2] == 0);
    assert(npe_world_battle_cast_magic(file, &world, 1, 2) == NPE_BATTLE_CONTINUE);
    assert(world.qi[0] == 919 && world.qi[1] == 919);
    assert(world.hp[1] == 100 && world.hp[2] == 30000);
    assert(world.battle_last_order_count == 2 && world.random_state == advance(1, 4));
    /* Native C2 DIV truncates the MUL result to signed R7. Type18 at
       max999 therefore wraps 49950 to -15586 before division. */
    assert(npe_battle_revival_hp(16, 999) == 99);
    assert(npe_battle_revival_hp(17, 999) == 299);
    assert(npe_battle_revival_hp(18, 999) == (uint16_t)-155);
    assert(npe_battle_revival_hp(19, 30000) == 30000);
    {
        /* In-memory metadata variants exercise source types16..19 and
           the group flag absent from original spell1. The input file
           and continuous replay remain untouched. */
        NpeFile variant;
        char error[120];
        uint8_t *copy = malloc(file->size);
        assert(copy);
        memcpy(copy, file->bytes, file->size);
        assert(npe_parse(&variant, copy, file->size, error, sizeof(error)) == 0);
        assert(npe_skill(&variant, 0, &skill) == 0);
        size_t offset = (size_t)(skill.raw - copy);
        const uint16_t expected[4] = {12, 36, 60, 120};
        for (unsigned type = 16; type <= 19; ++type) {
            copy[offset + 14] = (uint8_t)type;
            copy[offset + 37] = 1;
            world = magic_fixture(&variant, 1);
            world.party_active[2] = 1; world.hp[2] = 0; world.max_hp[2] = 120;
            world.hp[1] = 0; world.max_hp[1] = 80;
            assert(npe_world_battle_cast_magic(&variant, &world, 1, 0) == NPE_BATTLE_CONTINUE);
            assert(world.hp[0] == 30000 && world.hp[2] == expected[type - 16]);
            assert(world.hp[1] == 0 && world.qi[0] == 919);
            assert(world.battle_enemy_hps[1] == 500 && world.random_state == advance(1, 2));
        }
        free(copy);
    }
    puts("Revival original1, fallen target, QI, statuses, round eligibility and signed division passed");
}

static void probe(const NpeFile *file) {
    NpeWorld world;
    uint32_t seed;
    {
        NpeSkill skill;
        assert(npe_skill(file, 29, &skill) == 0);
        assert(npe_skill_status_effect(&skill, 0) == -2);
        assert(npe_skill_status_effect(&skill, 1) == -1);
        assert(npe_skill_status_effect(&skill, 2) == -2);
        assert(npe_skill_status_effect(&skill, 3) == -2);
        assert(npe_skill(file, 32, &skill) == 0);
        assert(npe_skill_status_effect(&skill, 0) == -5);
        assert(npe_skill_status_effect(&skill, 1) == 0);
    }
    for (unsigned attack = 60; attack <= 120; attack += 60) {
        world = fixture(file, 51, 1);
        memset(world.battle_enemy_skill_ids, 0, sizeof(world.battle_enemy_skill_ids));
        world.level[0] = 3;
        world.attack[0] = (uint16_t)attack;
        world.other_stats[0][1] = 999;
        world.random_state = 1;
        assert(npe_world_battle_cast_attack_skill(file, &world, 4, 1) == NPE_BATTLE_CONTINUE);
        assert(world.battle_enemy_hps[1] == (attack == 60 ? 420 : 400));
        assert(world.qi[0] == 50 && world.random_state == advance(1, 5));
    }
    /* Seed16's third draw is80, fourth89, target56. A skipped party
       action draws nothing, so start at its second state. */
    seed = advance(16, 2);
    world = fixture(file, 51, 1);
    world.battle_enemy_hps[1] = 450;
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_last_skill[1] == 6 && world.battle_enemy_last_target[1] == 4);
    assert(world.battle_enemy_hps[1] == 500 && world.hp[0] == 30000);
    assert(world.random_state == advance(seed, 5)); /* policy, slot, target, two preparation */

    world = fixture(file, 51, 0);
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_last_skill[0] == 0 && world.hp[0] < 30000);
    assert(world.random_state == advance(seed, 4)); /* threshold90 */

    world = fixture(file, 51, 1);
    world.battle_enemy_status[1][3] = -2;
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_last_skill[1] == 0 && world.battle_enemy_status[1][3] == -1);
    assert(world.random_state == advance(seed, 4)); /* blocked after policy draw */

    /* Slot2 is empty: scan backwards to slot0, never reroll. */
    world = fixture(file, 51, 1);
    seed = advance(57, 2);
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_last_skill[1] == 6 && world.random_state == advance(seed, 5));

    /* Original enemy62 supplies skills40/9/0. Group heal reaches another
       living but sealed record and leaves a fallen record at zero. */
    world = fixture(file, 62, 1);
    world.battle_enemy_hps[0] = 790;
    world.battle_enemy_max_hps[0] = 800;
    world.battle_enemy_status[0][2] = -2;
    world.battle_enemy_hps[1] = 1500;
    seed = advance(40, 2);
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_last_skill[1] == 9);
    assert(world.battle_enemy_hps[0] == 800 && world.battle_enemy_hps[1] == 1700 && !world.battle_enemy_hps[2]);
    assert(world.battle_enemy_status[0][2] == -1 && world.random_state == advance(seed, 5));

    world = fixture(file, 62, 1);
    seed = advance(38, 2);
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_last_skill[1] == 40);
    for (unsigned i = 0; i < 4; ++i) assert(world.battle_enemy_status[1][i] == (i == 1 ? 10 : 3));
    assert(world.random_state == advance(seed, 5)); /* positive immunity does not decay */

    /* Original enemy119's slot1 is skill4: defense64B2 / own64A6,
       then variance. An attacker has no player level/QI gate. */
    world = fixture(file, 119, 1);
    world.battle_party_defenses[0] = 200;
    seed = advance(40, 2);
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_last_skill[1] == 4 && world.hp[0] == 29943); /*60-200/100-1=57*/
    assert(world.random_state == advance(seed, 5));

    world = fixture(file, 119, 1);
    world.battle_party_defenses[0] = 999;
    world.battle_enemy_spell_stats[1] = 1; /* isolated fallback boundary */
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.hp[0] == 29988); /* nonpositive result ->12 */

    world = fixture(file, 124, 1); /* original slot2 is fourth-state skill20 */
    seed = advance(57, 2);
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_last_skill[1] == 20 && world.battle_party_status[0][3] == -1);
    assert(world.hp[0] == 30000 && world.random_state == advance(seed, 5));
    world = fixture(file, 124, 1);
    world.battle_party_status[0][3] = 1;
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_party_status[0][3] == 0); /* signed addition, not immune rejection */

    /* Poison33 is an original group spell; a fallen active member still
       receives its word. File31 maps to poison, not sleep. */
    world = fixture(file, 119, 1);
    world.party_active[1] = 1;
    world.hp[1] = 0;
    world.status_guard[0][1] = 7;
    world.status_guard[1][1] = 9;
    memset(world.battle_enemy_skill_ids[1], 0, 3);
    world.battle_enemy_skill_ids[1][0] = 33;
    seed = advance(38, 2);
    world.random_state = seed;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert((int16_t)world.battle_party_status[0][1] == 0 && (int16_t)world.battle_party_status[1][1] == 0);
    assert(world.battle_party_status[0][0] == -4 && world.battle_party_status[1][0] == -4);
    assert(world.status_guard[0][1] == 7 && world.status_guard[1][1] == 9);
    assert(world.hp[0] > 0 && world.battle_actor == 0);
    /* A genuinely sleeping party still advances empty rounds. */
    world = fixture(file, 51, 1);
    world.battle_party_status[0][1] = -3;
    world.battle_enemy_status[1][1] = -4;
    assert(npe_world_battle_flee(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_party_status[0][1] == 0 && world.battle_actor == 0);

    /* Poison has a shared damage local: poisoned enemies overwrite it
       with twice the first party level before applying to both sides. */
    world = fixture(file, 51, 1);
    memset(world.battle_enemy_skill_ids, 0, sizeof(world.battle_enemy_skill_ids));
    world.battle_party_status[0][0] = (uint16_t)-2;
    world.battle_enemy_status[1][0] = -2;
    world.battle_enemy_status[1][1] = -2; /* isolate poison from attacks */
    assert(npe_world_battle_defend(&world) == NPE_BATTLE_CONTINUE);
    assert(world.hp[0] == 29998 && world.battle_enemy_hps[1] == 498);
    assert((int16_t)world.battle_party_status[0][0] == -1 && world.battle_enemy_status[1][0] == -1);

    world = fixture(file, 51, 1);
    world.battle_party_status[0][0] = (uint16_t)-2;
    world.battle_enemy_status[1][1] = -2;
    assert(npe_world_battle_defend(&world) == NPE_BATTLE_CONTINUE);
    assert(world.hp[0] == 29982); /* no poisoned enemy: original enemy level9*2 */

    world = fixture(file, 51, 1);
    world.party_active[0] = 0;
    world.party_active[1] = 1;
    world.battle_actor = 1;
    world.hp[1] = 100;
    world.level[1] = 20;
    world.battle_enemy_status[1][0] = world.battle_enemy_status[1][1] = -2;
    assert(npe_world_battle_defend(&world) == NPE_BATTLE_CONTINUE);
    assert(world.battle_enemy_hps[1] == 460); /* compact first party record is member1, level20 */

    world = fixture(file, 51, 1);
    world.battle_enemy_hps[1] = 1;
    world.battle_enemy_status[1][0] = world.battle_enemy_status[1][1] = -2;
    world.battle_win_mode = 1;
    assert(npe_world_battle_defend(&world) == NPE_BATTLE_WON && world.game_mode == 1);
    world = fixture(file, 51, 1);
    world.battle_party_status[0][0] = -2;
    world.battle_enemy_status[1][0] = world.battle_enemy_status[1][1] = -2;
    assert(run_enemy_only_round(&world) == NPE_BATTLE_CONTINUE);
    assert(world.hp[0] == 30000 && world.battle_enemy_hps[1] == 500);
    puts("Enemy AI, original skill metadata, healing, damage, immunity, sleep and poison probes passed");
}

int main(int argc, char **argv) {
    FILE *input;
    long size;
    uint8_t *bytes;
    NpeFile file;
    char error[120];
    int magic_only=argc==3 && !strcmp(argv[2],"--magic-only");
    int entry_only=argc==3 && !strcmp(argv[2],"--entry-only");
    if ((argc != 2 && !magic_only && !entry_only) || !(input = fopen(argv[1], "rb"))) return 2;
    assert(fseek(input, 0, SEEK_END) == 0);
    size = ftell(input);
    assert(size > 0 && fseek(input, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)size);
    assert(bytes && fread(bytes, 1, (size_t)size, input) == (size_t)size);
    fclose(input);
    assert(npe_parse(&file, bytes, (size_t)size, error, sizeof(error)) == 0);
    assert(npe_game_fingerprint(&file) == UINT64_C(0x114861a601117d94));
    if(entry_only){
        probe_entry(&file);
        puts("Native entry, victory save/load, flee and defeat map-state preservation passed");
        free(bytes);
        return 0;
    }
    if(!magic_only){
        probe_entry(&file);
        probe_native_enemy_position(&file);
        probe_native_party_formation(&file);
    }
    probe_party_magic(&file);
    probe_revival(&file);
    if(!magic_only)probe(&file);
    free(bytes);
    return 0;
}
