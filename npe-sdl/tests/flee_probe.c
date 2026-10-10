#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flee_native.h"

static uint32_t advance(uint32_t state) {
    return state * UINT32_C(0x514f111f) + 1u;
}

static NpeWorld fixture(const NpeFile *file, unsigned party) {
    NpeWorld world;
    assert(npe_world_init(file, &world, 9, 29, 13) == 0);
    world.game_mode = 3; world.story_event_id = 1;
    assert(npe_world_prepare_scripted_battle(file, &world) == 1);
#define COPY_ENEMY(field) world.field[0] = world.field[1]
    COPY_ENEMY(battle_enemy_ids); COPY_ENEMY(battle_enemy_hps);
    COPY_ENEMY(battle_enemy_max_hps); COPY_ENEMY(battle_enemy_attacks);
    COPY_ENEMY(battle_enemy_defenses); COPY_ENEMY(battle_enemy_speeds); COPY_ENEMY(battle_enemy_lucks);
    COPY_ENEMY(battle_enemy_damage_floor_stats); COPY_ENEMY(battle_enemy_spell_stats);
#undef COPY_ENEMY
    memcpy(world.battle_enemy_skill_ids[0], world.battle_enemy_skill_ids[1], 3);
    for (unsigned i = 0; i < party; ++i) {
        world.party_active[i] = 1;
        world.hp[i] = world.max_hp[i] = 10000;
        world.qi[i] = world.max_qi[i] = world.qi[0];
        world.attack[i] = world.attack[0]; world.defense[i] = world.defense[0];
        world.battle_party_defenses[i] = world.battle_party_defenses[0];
        world.level[i] = world.level[0];
        world.other_stats[i][1] = 999;
    }
    npe_world_seed(&world, 1);
    return world;
}

static int can_act(const NpeWorld *world, unsigned i) {
    return world->party_active[i] && (int16_t)world->hp[i] > 0 &&
        (int16_t)world->battle_party_status[i][1] >= 0 &&
        (int16_t)world->battle_party_status[i][2] >= 0;
}

static void same_round(const NpeWorld *actual, const NpeWorld *reference) {
    assert(actual->game_mode == reference->game_mode &&
           actual->battle_actor == reference->battle_actor &&
           actual->random_state == reference->random_state);
    assert(!memcmp(actual->hp, reference->hp, sizeof(actual->hp)));
    assert(!memcmp(actual->qi, reference->qi, sizeof(actual->qi)));
    assert(!memcmp(actual->condition_words, reference->condition_words, sizeof(actual->condition_words)));
    assert(!memcmp(actual->battle_enemy_hps, reference->battle_enemy_hps, sizeof(actual->battle_enemy_hps)));
    assert(!memcmp(actual->battle_party_status, reference->battle_party_status, sizeof(actual->battle_party_status)));
    assert(!memcmp(actual->battle_enemy_status, reference->battle_enemy_status, sizeof(actual->battle_enemy_status)));
    assert(!memcmp(actual->battle_party_defenses, reference->battle_party_defenses, sizeof(actual->battle_party_defenses)));
    assert(!memcmp(actual->battle_defense_turns, reference->battle_defense_turns, sizeof(actual->battle_defense_turns)));
    assert(actual->battle_last_order_count == reference->battle_last_order_count &&
           !memcmp(actual->battle_last_order, reference->battle_last_order, actual->battle_last_order_count));
    assert(!actual->battle_pending_mask && !actual->battle_executing);
    for (unsigned i = 0; i < 3; ++i) assert(!actual->battle_item_reservations[i]);
}

static NpeWorld failed_round(NpeWorld world, unsigned target) {
    NpeWorld reference = world;
    /* Reconstruct the source-assigned ordinary commands through the public
       attack API. Earlier selected skills/items/defense are overwritten. */
    reference.battle_pending_mask = 0;
    memset(reference.battle_commands, 0, sizeof(reference.battle_commands));
    memset(reference.battle_item_reservations, 0, sizeof(reference.battle_item_reservations));
    if (!reference.story_event_id) reference.random_state = advance(reference.random_state);
    unsigned eligible = 0;
    for (unsigned i = 0; i < 3; ++i) if (can_act(&reference, i)) ++eligible;
    for (unsigned i = 0; i < 3; ++i) if (can_act(&reference, i)) {
        reference.battle_actor = (uint8_t)i;
        assert(npe_world_battle_attack(&reference, target) == NPE_BATTLE_CONTINUE);
        if (!--eligible) break;
    }
    assert(npe_world_battle_flee(&world) == NPE_BATTLE_CONTINUE);
    same_round(&world, &reference);
    return world;
}

static void probe(const NpeFile *file) {
    for (unsigned c = 0; c < sizeof(native_flee_cases)/sizeof(native_flee_cases[0]); ++c) {
        const NativeFleeCase *native = &native_flee_cases[c];
        NpeWorld world = fixture(file, native->party);
        world.condition_words[159] = (uint16_t)native->flee_actor;
        for (unsigned i = 0; i < native->flee_actor; ++i)
            assert(npe_world_battle_throw_item(file, &world, 159, 1) == NPE_BATTLE_CONTINUE);
        if (native->blocked == 1) world.hp[0] = 0;
        if (native->blocked == 2) world.battle_party_status[0][1] = (uint16_t)-2;
        if (native->random) { world.story_event_id = 0; world.random_state = 12; }
        world = failed_round(world, 0);
        unsigned count = 0;
        for (unsigned i = 0; i < world.battle_last_order_count; ++i)
            if (world.battle_last_order[i] < 3) {
                assert(count < native->attack_count && world.battle_last_order[i] == native->attack_order[count]);
                ++count;
            }
        assert(count == native->attack_count && world.battle_actor == native->next_actor &&
               world.condition_words[159] == native->quantity);
        assert(world.battle_enemy_hps[0] < 500); /* Earlier throw targeted slot1. */
        printf("Native flee case %u: %u attacks, actor%u, quantity%u passed\n",
               c, count, world.battle_actor, world.condition_words[159]);
    }
    /* Failure at the first selector also supplies later party commands. */
    NpeWorld world = fixture(file, 3);
    world = failed_round(world, 0);
    /* Enemy acts first; a queued defense must lose its quarter reduction. */
    world = fixture(file, 3);
    assert(npe_world_battle_defend(&world) == NPE_BATTLE_CONTINUE);
    world.battle_enemy_speeds[0] = world.battle_enemy_speeds[1] = 1200;
    world = failed_round(world, 0);
    /* A queued attack spell is overwritten without charging its QI. */
    world = fixture(file, 2); world.level[0] = 3;
    uint16_t qi = world.qi[0];
    assert(npe_world_battle_cast_magic(file, &world, 4, 1) == NPE_BATTLE_CONTINUE);
    world = failed_round(world, 0);
    assert(world.qi[0] == qi);
    /* Signed-negative/dead front records are skipped by the source scan. */
    for (unsigned hp = 0; hp <= 1; ++hp) {
        world = fixture(file, 2);
        world.battle_enemy_hps[0] = hp ? UINT16_C(0xffff) : 0;
        world = failed_round(world, 1);
    }
    /* Successful escape discards pending commands without attacking/refund. */
    world = fixture(file, 2); world.condition_words[159] = 1;
    assert(npe_world_battle_throw_item(file, &world, 159, 0) == NPE_BATTLE_CONTINUE);
    world.story_event_id = 0; world.random_state = 1;
    assert(npe_world_battle_flee(&world) == NPE_BATTLE_FLED);
    assert(world.game_mode == 1 && world.hp[0] == 10000 && world.hp[1] == 10000 &&
           world.condition_words[159] == 0 && world.random_state == advance(1) &&
           !world.battle_pending_mask && !world.battle_item_reservations[0]);
    puts("Failed flee command overwrite, original attack order, skipped actors and successful escape passed");
}

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *input = fopen(argv[1], "rb"); assert(input && fseek(input, 0, SEEK_END) == 0);
    long size = ftell(input); assert(size > 0 && fseek(input, 0, SEEK_SET) == 0);
    uint8_t *bytes = malloc((size_t)size); assert(bytes && fread(bytes, 1, (size_t)size, input) == (size_t)size);
    assert(fclose(input) == 0);
    NpeFile file; char error[128];
    assert(npe_parse(&file, bytes, (size_t)size, error, sizeof(error)) == 0);
    assert(npe_game_fingerprint(&file) == UINT64_C(0x114861a601117d94));
    probe(&file); free(bytes); return 0;
}
