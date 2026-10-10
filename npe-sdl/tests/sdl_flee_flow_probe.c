#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"

static unsigned scenario, flee_calls, after_icon, icon_calls;
static SDL_atomic_t ready, finished;
static int probe_new_game(const NpeFile *, NpeWorld *);
static int probe_flee(NpeWorld *);
static int probe_icon(unsigned, uint8_t *);
static int probe_controls(const NpeFile *,unsigned,uint8_t *);
static int probe_wait(SDL_Event *event, int timeout) {
    if(flee_calls)SDL_AtomicSet(&finished,1);
    SDL_AtomicSet(&ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}
#define npe_world_new_game probe_new_game
#define npe_world_restart_game probe_new_game
#define npe_world_battle_flee probe_flee
#define npe_render_battle_icon_selection probe_icon
#define npe_render_battle_controls probe_controls
#define SDL_WaitEventTimeout probe_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef npe_render_battle_icon_selection
#undef npe_render_battle_controls
#undef npe_world_battle_flee
#undef npe_world_new_game
#undef npe_world_restart_game

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    assert(npe_world_init(file, world, 9, 29, 13) == 0);
    unsigned party = scenario == 0 ? 1 : scenario == 4 ? 3 : 2;
    for(unsigned i=0;i<3;++i){world->party_active[i]=(uint8_t)(i<party);world->party_ids[i]=1;
        world->party_characters[i]=world->party_characters[0];memcpy(world->party_names[i],world->party_names[0],12);}
    world->game_mode = 3; world->story_event_id = 1;
    assert(npe_world_prepare_scripted_battle(file, world) == 1);
    /* Same original enemy51 front/middle fixture as the native sampler. */
#define COPY_ENEMY(field) world->field[0] = world->field[1]
    COPY_ENEMY(battle_enemy_ids); COPY_ENEMY(battle_enemy_hps);
    COPY_ENEMY(battle_enemy_max_hps); COPY_ENEMY(battle_enemy_attacks);
    COPY_ENEMY(battle_enemy_defenses); COPY_ENEMY(battle_enemy_speeds); COPY_ENEMY(battle_enemy_lucks);
    COPY_ENEMY(battle_enemy_damage_floor_stats); COPY_ENEMY(battle_enemy_spell_stats);
#undef COPY_ENEMY
    memcpy(world->battle_enemy_skill_ids[0], world->battle_enemy_skill_ids[1], 3);
    memcpy(world->battle_record_residue[party]+46,world->battle_record_residue[party+1]+46,2);
    for (unsigned i = 0; i < party; ++i) {
        world->party_active[i] = 1; world->hp[i] = world->max_hp[i] = 10000;
        world->attack[i] = 8; world->other_stats[i][1] = 999;
    }
    if (scenario) {
        /* Isolated pending-item fixture, not a continuous replay save. */
        world->condition_words[159] = 1;
        assert(npe_world_battle_throw_item(file, world, 159, 1) == NPE_BATTLE_CONTINUE);
    }
    if (scenario == 4) world->hp[0] = 0;
    if (scenario == 2 || scenario == 3) world->story_event_id = 0;
    npe_world_seed(world, scenario == 2 ? 12 : 1);
    return 0;
}

static int probe_flee(NpeWorld *world) {
    assert(world->battle_actor == (scenario ? 1 : 0));
    /* The app seeds new games from the host clock; fix only this isolated
       command boundary so random failure/success and AI are reproducible. */
    npe_world_seed(world, scenario == 2 ? 12 : 1);
    unsigned hp = world->battle_enemy_hps[0];
    int result = npe_world_battle_flee(world);
    ++flee_calls;
    assert(world->condition_words[159] == 0 && !world->battle_pending_mask &&
           !world->battle_item_reservations[0]);
    if (scenario == 3) {
        assert(result == NPE_BATTLE_FLED && world->game_mode == 1 &&
               world->hp[0] == 10000 && world->hp[1] == 10000);
    } else {
        assert(result == NPE_BATTLE_CONTINUE && world->game_mode == 3 &&
               world->battle_enemy_hps[0] < hp);
        unsigned party_actions = 0;
        for (unsigned i = 0; i < world->battle_last_order_count; ++i)
            party_actions += world->battle_last_order[i] < 3;
        assert(party_actions == (scenario == 0 ? 1 : 2));
        if (scenario == 4) assert(world->battle_last_order[0] == 1 && world->battle_actor == 1);
    }
    return result;
}

static int probe_icon(unsigned icon, uint8_t *pixels) {
    if (flee_calls) { after_icon = icon; ++icon_calls; }
    return npe_render_battle_icon_selection(icon, pixels);
}
static int probe_controls(const NpeFile *file,unsigned icon,uint8_t *pixels){
    /* Cached rounds rebuild controls in world.c; that internal icon call
       does not pass through this translation unit's probe_icon macro. */
    if(flee_calls){after_icon=icon;++icon_calls;}
    return npe_render_battle_controls(file,icon,pixels);
}

static int send_keys(void *unused) {
    const SDL_Keycode keys[] = {SDLK_RETURN, SDLK_LEFT, SDLK_DOWN, SDLK_RETURN,
        SDLK_DOWN, SDLK_DOWN, SDLK_DOWN, SDLK_RETURN};
    (void)unused;
    for (unsigned i = 0; !SDL_AtomicGet(&ready) && i < 500; ++i) SDL_Delay(10);
    if (!SDL_AtomicGet(&ready)) return -1;
    for (unsigned i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) {
        SDL_Event event = {0}; event.type = SDL_KEYDOWN; event.key.keysym.sym = keys[i];
        if (SDL_PushEvent(&event) != 1) return -1;
        SDL_Delay(70);
    }
    unsigned waited=0;
    while(!SDL_AtomicGet(&finished)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&finished))return -1;
    SDL_Event event = {0}; event.type = SDL_QUIT;
    return SDL_PushEvent(&event) == 1 ? 0 : -1;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    char *args[] = {"npe-sdl", argv[1], NULL};
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    for (scenario = 0; scenario < 5; ++scenario) {
        flee_calls = icon_calls = 0; after_icon = 99; SDL_AtomicSet(&ready, 0);SDL_AtomicSet(&finished,0);
        SDL_Thread *thread = SDL_CreateThread(send_keys, "flee-keys", NULL); assert(thread);
        assert(run(2, args) == 0);
        int result; SDL_WaitThread(thread, &result); assert(result == 0 && flee_calls == 1);
        if (scenario != 3) assert(icon_calls && after_icon == (scenario == 2 ? 2 : 1));
        fprintf(stderr, "SDL flee scenario%u: escape call, pending item, actions and base icon passed\n", scenario);
    }
    puts("SDL flee flow: scripted/random failure, success, queued throws and fallen actor passed");
    return 0;
}
