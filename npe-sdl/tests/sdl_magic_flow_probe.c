#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"

static unsigned scenario, calls, selected_id;
static unsigned prompts;
static unsigned cast_target, single_cost;
static uint16_t first_enemy_after;
static int cast_result;
static uint16_t qi_after, hp_after, enemy_after, revived_after;
static SDL_atomic_t ready;
static int probe_new_game(const NpeFile *, NpeWorld *);
static int probe_magic(const NpeFile *, NpeWorld *, unsigned, unsigned);
static int probe_qi_prompt(const NpeFont *, uint8_t *);
static void probe_seed(NpeWorld *, uint32_t);
static int probe_wait(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}
#define npe_world_new_game probe_new_game
#define npe_world_restart_game probe_new_game
#define npe_world_seed probe_seed
#define npe_world_battle_cast_magic probe_magic
#define npe_render_battle_insufficient_qi probe_qi_prompt
#define SDL_WaitEventTimeout probe_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef npe_world_battle_cast_magic
#undef npe_render_battle_insufficient_qi
#undef npe_world_new_game
#undef npe_world_restart_game
#undef npe_world_seed

/* The application seeds again after New Game. Keep the isolated fixture's
   intended seed instead of allowing the host clock to change its outcome. */
static void probe_seed(NpeWorld *world, uint32_t ignored) {
    (void)ignored; npe_world_seed(world, 1);
}

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    if (npe_world_init(file, world, 9, 29, 13)) return -1;
    world->game_mode = 3; world->story_event_id = 1;
    if (npe_world_prepare_scripted_battle(file, world) != 1) return -1;
    /* Character attributes only; original NPE party/skill/enemy data. */
    world->level[0] = 99;
    world->hp[0] = 10000; world->max_hp[0] = 30000;
    world->qi[0] = scenario == 2 || scenario == 5 ? 0 : 999; world->max_qi[0] = 999;
    world->attack[0] = 60; world->other_stats[0][1] = 999;
    if (scenario == 3 || scenario == 4) {
        /* Isolated revival fixture: a fallen second party member and a
           learned prefix containing original spell1. This is not a save
           from the continuous character-attribute-only story replay. */
        world->party_active[1] = 1;
        world->party_ids[1] = 2;
        world->hp[1] = 0; world->max_hp[1] = 30000;
        memset(world->skill_slots[0], 0, sizeof(world->skill_slots[0]));
        world->skill_slots[0][0] = 1;
        if (scenario == 4) {
            world->party_active[2] = 1;
            world->hp[2] = 0; world->max_hp[2] = 30000;
        }
    }
    if (scenario == 5) {
        memset(world->skill_slots[0],0,sizeof(world->skill_slots[0]));
        world->skill_slots[0][0] = 4;
    }
    if (scenario == 6) {
        NpeSkill skill;
        assert(npe_skill(file,3,&skill)==0);single_cost=skill.qi_cost;
        memset(world->skill_slots[0],0,sizeof(world->skill_slots[0]));
        world->skill_slots[0][0]=4;
        /* Isolated formation fixture: second original enemy51 record. */
#define COPY_ENEMY(field) world->field[0]=world->field[1]
        COPY_ENEMY(battle_enemy_ids);COPY_ENEMY(battle_enemy_hps);
        COPY_ENEMY(battle_enemy_max_hps);COPY_ENEMY(battle_enemy_attacks);
        COPY_ENEMY(battle_enemy_defenses);COPY_ENEMY(battle_enemy_speeds); COPY_ENEMY(battle_enemy_lucks);
        COPY_ENEMY(battle_enemy_base_attacks);COPY_ENEMY(battle_enemy_base_defenses);
        COPY_ENEMY(battle_enemy_qis);COPY_ENEMY(battle_enemy_max_qis);
        COPY_ENEMY(battle_enemy_damage_floor_stats);COPY_ENEMY(battle_enemy_spell_stats);
#undef COPY_ENEMY
        memcpy(world->battle_enemy_skill_ids[0],world->battle_enemy_skill_ids[1],3);
    }
    qi_after = world->qi[0]; hp_after = world->hp[0];
    enemy_after = world->battle_enemy_hps[1];
    npe_world_seed(world, 1);
    return 0;
}
static int probe_qi_prompt(const NpeFont *font, uint8_t *pixels) {
    ++prompts;
    return npe_render_battle_insufficient_qi(font,pixels);
}
static int probe_magic(const NpeFile *file, NpeWorld *world, unsigned id, unsigned target) {
    ++calls; selected_id = id; cast_target=target;
    cast_result = npe_world_battle_cast_magic(file, world, id, target);
    qi_after = world->qi[0]; hp_after = world->hp[0];
    enemy_after = world->battle_enemy_hps[1];
    first_enemy_after=world->battle_enemy_hps[0];
    revived_after = world->hp[scenario == 4 ? 2 : 1];
    return cast_result;
}
static int send_keys(void *unused) {
    SDL_Keycode keys[] = {SDLK_RETURN, SDLK_LEFT, SDLK_RETURN,
        SDLK_DOWN, scenario == 1 ? SDLK_RIGHT : SDLK_DOWN,
        SDLK_RETURN, SDLK_F4, SDLK_F4, SDLK_F4};
    if (scenario == 3 || scenario == 4) {
        keys[3] = SDLK_RETURN; /* original spell1 -> target selection */
        keys[4] = SDLK_DOWN;   /* include the fallen member */
    }
    if (scenario == 5) {
        keys[3] = SDLK_RETURN; /* insufficient single-target spell */
        /* scene_wait_input supplies the notice's accepted key immediately.
           Another queued Return would select the spell a second time. */
        keys[4] = SDLK_F4;     /* restored list -> command controls */
        keys[5] = SDLK_F4;
    }
    if (scenario == 6) {
        keys[3]=SDLK_RETURN;keys[4]=SDLK_RIGHT; /* right has no effect */
        keys[5]=SDLK_DOWN;keys[6]=SDLK_RETURN;
    }
    unsigned waited = 0;
    (void)unused;
    while (!SDL_AtomicGet(&ready) && waited < 5000) {
        SDL_Delay(10); waited += 10;
    }
    if (!SDL_AtomicGet(&ready)) return -1;
    for (unsigned i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) {
        SDL_Event event = {0};
        event.type = SDL_KEYDOWN; event.key.keysym.sym = keys[i];
        if (SDL_PushEvent(&event) != 1) return -1;
        SDL_Delay(70);
    }
    SDL_Event event = {0}; event.type = SDL_QUIT;
    return SDL_PushEvent(&event) == 1 ? 0 : -1;
}
int main(int argc, char **argv) {
    char *arguments[] = {"npe-sdl", NULL, NULL};
    assert(argc == 2); arguments[1] = argv[1];
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    for (scenario = 0; scenario < 7; ++scenario) {
        int thread_result = -1;
        calls = selected_id = prompts = 0; SDL_AtomicSet(&ready, 0);
        SDL_Thread *thread = SDL_CreateThread(send_keys, "magic-keys", NULL);
        assert(thread && run(2, arguments) == 0);
        SDL_WaitThread(thread, &thread_result);
        assert(thread_result == 0);
        fprintf(stderr,"Magic scenario %u calls=%u prompts=%u selected=%u\n",scenario,calls,prompts,selected_id);
        if (scenario == 2 || scenario == 5) {
            assert(calls == 0 && prompts == 1);
            assert(!qi_after && hp_after == 10000 && enemy_after == 500);
            continue;
        }
        assert(calls == 1 && prompts == 0);
        if (scenario==6) {
            assert(selected_id==4 && cast_target==1 && first_enemy_after==500);
            assert(qi_after==999-single_cost && enemy_after<500);
            continue;
        }
        assert(selected_id == (scenario >= 3 ? 1 : scenario == 1 ? 9 : 5));
        if (scenario == 2) {
            assert(cast_result == -3 && !qi_after && hp_after == 10000 && enemy_after == 500);
        } else {
            assert(cast_result == NPE_BATTLE_CONTINUE);
            assert(qi_after == (scenario >= 3 ? 919 : scenario == 1 ? 979 : 974));
            if (scenario >= 3) assert(revived_after > 0 && revived_after <= 30000);
            if (scenario == 1) assert(hp_after > 10000);
            else if (scenario < 3) assert(enemy_after <= 340);
        }
    }
    puts("SDL native magic list, group casting, fallen revival target and insufficient QI return passed");
    return 0;
}
