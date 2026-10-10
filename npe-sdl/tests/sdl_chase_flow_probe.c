#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"

static SDL_atomic_t event_loop_ready, walking, walk_finished;
static unsigned scenario, moves, attacks, victories, desktop_dialogs;
static SDL_atomic_t timer_updates;
static int probe_new_game(const NpeFile *file, NpeWorld *world);
static int probe_step(const NpeFile *file, NpeWorld *world, unsigned direction,
                        uint8_t *pixels,NpeMapDisplayCommit commit,void *context);
static int probe_attack(NpeWorld *world, unsigned target);
static int probe_chase_update(const NpeFile *file, NpeWorld *world);
static int probe_message_box(Uint32 flags, const char *title,
                              const char *message, SDL_Window *window);
static int probe_wait_event(SDL_Event *event, int timeout) {
    if(!SDL_AtomicGet(&walking))SDL_AtomicSet(&event_loop_ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}

#define npe_world_new_game probe_new_game
#define npe_world_step_with_display probe_step
#define npe_world_battle_attack probe_attack
#define npe_world_update_chase probe_chase_update
#define SDL_ShowSimpleMessageBox probe_message_box
#define SDL_WaitEventTimeout probe_wait_event
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_ShowSimpleMessageBox
#undef npe_world_battle_attack
#undef npe_world_update_chase
#undef npe_world_step_with_display
#undef npe_world_new_game

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    if (npe_world_init(file, world, 105, 9, 8)) return -1;
    world->story_progress = 94;
    world->hp[0] = world->max_hp[0] = 30000;
    world->attack[0] = 999;
    return 0;
}

static int probe_step(const NpeFile *file, NpeWorld *world, unsigned direction,
                        uint8_t *pixels,NpeMapDisplayCommit commit,void *context) {
    SDL_AtomicSet(&walking,1);
    int result = npe_world_step_with_display(file, world, direction,pixels,commit,context);
    SDL_AtomicSet(&walking,0);
    ++moves;
    assert(direction == 0);
    if (scenario) {
        fprintf(stderr,"timed chase return: moves=%u result=%d victories=%u cooldown=%u mode=%u XY=%u,%u view=%u,%u screen=%u,%u timer=%d\n",
            moves,result,victories,world->chase_cooldown[0],world->game_mode,world->x,world->y,
            world->view.offset_x,world->view.offset_y,world->view.screen_x,world->view.screen_y,
            SDL_AtomicGet(&timer_updates));
        assert(moves == 1 && result == NPE_MOVE_OK && victories == 1 &&
               world->chase_cooldown[0] == 4 && world->game_mode == 1 &&
               world->x == 9 && world->y == 9);
        SDL_AtomicSet(&walk_finished,1);
        return result;
    }
    if (moves == 1)
        assert(result == NPE_MOVE_OK && world->chase_x[0] == 10 &&
               world->chase_y[0] == 11);
    else if (moves == 2)
        assert(result == NPE_MOVE_EVENT && world->game_mode == 3 &&
               world->story_event_id == 0 && world->battle_chase_slot == 1 &&
               world->battle_enemy_count == 1 && world->battle_enemy_ids[0] == 89 &&
               world->battle_enemy_hps[0] == 5000);
    else {
        fprintf(stderr,"walk after chase: moves=%u result=%d victories=%u cooldown=%u mode=%u progress=%u XY=%u,%u\n",
            moves,result,victories,world->chase_cooldown[0],world->game_mode,world->story_progress,world->x,world->y);
        assert(moves == 3 && result == NPE_MOVE_OK && victories == 1 &&
               world->chase_cooldown[0] == 4 && world->game_mode == 1 &&
               world->story_progress == 94 && world->x == 9 && world->y == 11);
        SDL_AtomicSet(&walk_finished,1);
    }
    return result;
}

static int probe_chase_update(const NpeFile *file, NpeWorld *world) {
    int result = npe_world_update_chase(file, world);
    int count = SDL_AtomicAdd(&timer_updates, 1) + 1;
    if (scenario == 1) {
        assert(world->x == 9 && world->y == 8 && moves == 0);
        if (count == 1)
            assert(result == 0 && world->chase_x[0] == 10 && world->chase_y[0] == 11);
        else
            assert(count == 2 && result == 1 && world->game_mode == 3 &&
                   world->battle_chase_slot == 1 && world->battle_enemy_hps[0] == 5000);
    }
    return result;
}

static int probe_attack(NpeWorld *world, unsigned target) {
    int result = npe_world_battle_attack(world, target);
    ++attacks;
    assert(result == NPE_BATTLE_CONTINUE || result == NPE_BATTLE_WON);
    if (result == NPE_BATTLE_WON) {
        assert(world->game_mode == 1 && world->chase_cooldown[0] == 5 &&
               world->battle_chase_slot == 0 && world->story_progress == 94);
        ++victories;
    }
    return result;
}

static int probe_message_box(Uint32 flags, const char *title,
                              const char *message, SDL_Window *window) {
    (void)flags; (void)title; (void)message; (void)window;
    ++desktop_dialogs;
    return 0;
}

static int send_keys(void *unused) {
    static const SDL_Keycode keys[] = {
        SDLK_RETURN, SDLK_DOWN, SDLK_DOWN,
        SDLK_RETURN, SDLK_RETURN, SDLK_RETURN,
        SDLK_RETURN, SDLK_RETURN, SDLK_RETURN,
        SDLK_DOWN
    };
    unsigned waited = 0;
    (void)unused;
    while (!SDL_AtomicGet(&event_loop_ready) && waited < 5000) {
        SDL_Delay(10); waited += 10;
    }
    if (!SDL_AtomicGet(&event_loop_ready)) return -1;
    if (scenario == 1) {
        SDL_Event event = {0};
        event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_RETURN;
        if (SDL_PushEvent(&event) != 1) return -1;
        /* Exercise the application's actual 4-second waits, with no
           movement keys before contact. Deadline checks below use a
           supplied clock and don't replace this integration scenario. */
        waited = 0;
        while (SDL_AtomicGet(&timer_updates) < 2 && waited < 10000) {
            SDL_Delay(10); waited += 10;
        }
        if (SDL_AtomicGet(&timer_updates) != 2) return -1;
    }
    for (size_t i = scenario ? 3 : 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        waited=0;
        while(!SDL_AtomicGet(&event_loop_ready) && waited<5000){SDL_Delay(10);waited+=10;}
        if(!SDL_AtomicGet(&event_loop_ready))return -1;
        SDL_AtomicSet(&event_loop_ready,0);
        SDL_Event event = {0};
        event.type = SDL_KEYDOWN; event.key.keysym.sym = keys[i];
        if (SDL_PushEvent(&event) != 1) return -1;
        SDL_Delay(70);
    }
    waited=0;
    /* A ready flag from the preceding battle wait can remain set while
       the final arrow is still being animated. Do not enqueue QUIT until
       that walking assertion completed; QUIT legitimately aborts a walk. */
    while((!SDL_AtomicGet(&walk_finished)||!SDL_AtomicGet(&event_loop_ready)) && waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&walk_finished)||!SDL_AtomicGet(&event_loop_ready))return -1;
    {
        SDL_Event event = {0}; event.type = SDL_QUIT;
        return SDL_PushEvent(&event) == 1 ? 0 : -1;
    }
}

int main(int argc, char **argv) {
    SDL_Thread *thread;
    int thread_result = -1;
    char *arguments[] = {"npe-sdl", NULL, NULL};
    assert(argc == 2); arguments[1] = argv[1];
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    {
        NpeFile file;
        NpeWorld world;
        uint8_t *bytes;
        size_t size;
        char error[160];
        Uint32 deadline = 4100;
        assert(read_file(argv[1], &bytes, &size) == 0 &&
               npe_parse(&file, bytes, size, error, sizeof(error)) == 0);
        assert(npe_world_init(&file, &world, 105, 9, 8) == 0);
        world.story_progress = 94;
        assert(game_world_timer(&file, &world, 4099, &deadline, 0) == 0 &&
               world.chase_x[0] == 9 && deadline == 4100);
        assert(game_world_timer(&file, &world, 4100, &deadline, 1) == 0 &&
               world.chase_x[0] == 9 && deadline == 8100);
        world.game_mode = 6;
        assert(game_world_timer(&file, &world, 8100, &deadline, 0) == 0 &&
               world.chase_x[0] == 9 && deadline == 12100);
        world.game_mode = 1;
        assert(game_world_timer(&file, &world, 12100, &deadline, 0) == 0 &&
               world.chase_x[0] == 10 && world.chase_y[0] == 11 && deadline == 16100);
        assert(game_world_timer(&file, &world, 30100, &deadline, 1) == 0 &&
               world.chase_x[0] == 10 && deadline == 34100);
        deadline = 0xFFFFFFF0u;
        assert(game_world_timer(&file, &world, 0xFFFFFFEFu, &deadline, 0) == 0);
        assert(game_world_timer(&file, &world, 0x10u, &deadline, 1) == 0 &&
               deadline == 4016u);
        npe_free(&file);
    }
    for (scenario = 0; scenario < 2; ++scenario) {
        moves = attacks = victories = desktop_dialogs = 0;
        SDL_AtomicSet(&timer_updates, 0); SDL_AtomicSet(&event_loop_ready, 0);
        SDL_AtomicSet(&walk_finished,0);
        thread = SDL_CreateThread(send_keys, "chase-keys", NULL);
        assert(thread && run(2, arguments) == 0);
        SDL_WaitThread(thread, &thread_result);
        assert(thread_result == 0 && moves == (scenario ? 1u : 3u) && attacks == 6 &&
               victories == 1 && desktop_dialogs == 0 &&
               SDL_AtomicGet(&timer_updates) == (scenario ? 2 : 0));
    }
    puts("SDL movement and stationary timed chase, battle and cooldown passed");
    return 0;
}
