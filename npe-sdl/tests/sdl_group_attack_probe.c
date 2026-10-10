#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"

static unsigned attack_count, enemy_target, enemy_hp_before, enemy_hp_after;
static unsigned other_hp_before, other_hp_after;
static unsigned scenario;
static unsigned round_count;
static uint8_t round_order[6];
static SDL_atomic_t event_loop_ready;
static int probe_new_game(const NpeFile *file, NpeWorld *world);
static int probe_group(NpeWorld *world);
static int probe_wait_event(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&event_loop_ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}

#define npe_world_new_game probe_new_game
#define npe_world_restart_game probe_new_game
#define npe_world_battle_group_attack probe_group
#define SDL_WaitEventTimeout probe_wait_event
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef npe_world_battle_group_attack
#undef npe_world_new_game
#undef npe_world_restart_game

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    int prepared;
    if (scenario == 2) {
        if (npe_world_new_game(file, world)) return -1;
        world->game_mode = 10; world->story_event_id = 1;
        if (npe_world_apply_party_event(file, world) != 1) return -1;
        /* This two-target diagnostic used the old compact enemy view.
           Keep actual physical slots: duplicate the NPE center ID into
           the left formation byte before entry, leaving the right empty. */
        uint8_t center;
        if(npe_world_resident_read(world,0x527625,&center,1) || !center ||
           npe_world_resident_write(world,0x527626,&center,1) ||
           npe_world_prepare_scripted_battle(file, world) != 1)return -1;
        assert(world->party_active[0] && world->party_active[1] && !world->party_active[2]);
        assert(world->battle_enemy_hps[0] && world->battle_enemy_hps[1]);
        /* Isolated character speed/attack fixture; NPE formation and
           enemy attributes retain their original values. */
        world->other_stats[0][1] = 999;
        world->other_stats[1][1] = 998;
        /* Enough damage remains after the original enemy healing AI;
           otherwise final HP alone cannot prove the group target. */
        world->attack[0] = world->attack[1] = 250;
        npe_world_seed(world, 1);
        enemy_target = 0;
        enemy_hp_before = world->battle_enemy_hps[0];
        other_hp_before = world->battle_enemy_hps[1];
        return 0;
    }
    if (scenario == 1) {
        if (npe_world_init(file, world, 20, 9, 2)) return -1;
        world->encounter_steps = 999;
        npe_world_seed(world, 3);
        prepared = npe_world_step(file, world, 1);
        if (prepared != NPE_MOVE_EVENT || world->game_mode != 3 ||
            world->battle_enemy_count != 2 ||
            !world->battle_enemy_hps[0] ||
            !world->battle_enemy_hps[1] ||
            world->battle_target != 0) {
            fprintf(stderr, "group probe: two-enemy encounter failed\n");
            return -1;
        }
        /* Tab will select enemy1, but native group assignment resets to
           the first living enemy (source 44:8B94..8C3A). */
        enemy_target = 0;
        enemy_hp_before = world->battle_enemy_hps[enemy_target];
        other_hp_before = world->battle_enemy_hps[1];
        return 0;
    }
    if (npe_world_init(file, world, 9, 3, 21)) {
        fprintf(stderr, "group probe: map init failed\n");
        return -1;
    }
    world->game_mode = 3;
    world->story_event_id = 1;
    prepared = npe_world_prepare_scripted_battle(file, world);
    if (prepared != 1 ||
        world->battle_enemy_count != 3 || world->battle_actor != 0 ||
        !world->party_active[0] || world->party_active[1] ||
        world->party_active[2] || world->battle_enemy_hps[0] ||
        !world->battle_enemy_hps[1] || world->battle_enemy_hps[2]) {
        fprintf(stderr, "group probe: prepare=%d enemies=%u actor=%u party=%u/%u/%u ids=%u/%u/%u hp=%u/%u/%u\n",
                prepared, world->battle_enemy_count, world->battle_actor,
                world->party_active[0], world->party_active[1],
                world->party_active[2], world->battle_enemy_ids[0],
                world->battle_enemy_ids[1], world->battle_enemy_ids[2],
                world->battle_enemy_hps[0], world->battle_enemy_hps[1],
                world->battle_enemy_hps[2]);
        return -1;
    }
    enemy_target = world->battle_target;
    enemy_hp_before = world->battle_enemy_hps[enemy_target];
    world->attack[0] = 250;
    world->other_stats[0][1] = 999;
    npe_world_seed(world, 1);
    return 0;
}

static int probe_group(NpeWorld *world) {
    if (scenario) assert(world->battle_target == 1);
    int result = npe_world_battle_group_attack(world);
    ++attack_count;
    enemy_hp_after = world->battle_enemy_hps[enemy_target];
    if (scenario) other_hp_after = world->battle_enemy_hps[1];
    round_count = world->battle_last_order_count;
    memcpy(round_order, world->battle_last_order, sizeof(round_order));
    return result;
}

static int send_keys(void *unused) {
    static const SDL_Keycode keys[] = {
        SDLK_RETURN, /* title: new game, intercepted as diagnostic battle */
        SDLK_TAB, /* select the second living enemy when there are two */
        SDLK_LEFT, SDLK_DOWN, SDLK_RETURN, /* native flag path; fourth icon is disabled for one living member */
        SDLK_RETURN /* first row: group attack */
    };
    unsigned waited = 0;
    (void)unused;
    while (!SDL_AtomicGet(&event_loop_ready) && waited < 5000) {
        SDL_Delay(10);
        waited += 10;
    }
    if (!SDL_AtomicGet(&event_loop_ready)) return -1;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        /* Scripted entry already selects physical enemy1. Tab would move
           back to enemy0, defeating this non-first-target reset check. */
        if(scenario==2 && keys[i]==SDLK_TAB)continue;
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
    for (scenario = 0; scenario < 3; ++scenario) {
        attack_count = 0;
        enemy_hp_before = enemy_hp_after = 0;
        other_hp_before = other_hp_after = 0;
        SDL_AtomicSet(&event_loop_ready, 0);
        thread = SDL_CreateThread(send_keys, "group-keys", NULL);
        assert(thread);
        assert(run(2, arguments) == 0);
        SDL_WaitThread(thread, &thread_result);
        assert(thread_result == 0);
        fprintf(stderr, "SDL group scenario%u: calls%u enemy%u->%u other%u->%u\n",
                scenario, attack_count, enemy_hp_before, enemy_hp_after, other_hp_before, other_hp_after);
        assert(attack_count == 1 && enemy_hp_after < enemy_hp_before);
        if (scenario) assert(other_hp_after == other_hp_before);
        if (scenario == 2) assert(round_count >= 2 && round_order[0] == 0 && round_order[1] == 1);
    }
    puts("SDL group attack passed for single and two-party battles, including native speed order");
    return 0;
}
