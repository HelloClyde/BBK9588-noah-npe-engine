#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"

static SDL_atomic_t event_loop_ready;
static unsigned scenario, prepare_calls, close_calls, story_calls;
static unsigned desktop_dialogs;
static int probe_new_game(const NpeFile *file, NpeWorld *world);
static int probe_prepare(const NpeFile *file, NpeWorld *world);
static int probe_close(NpeWorld *world);
static int probe_story(const NpeFile *file, NpeWorld *world);
static int probe_message_box(Uint32 flags, const char *title,
                              const char *message, SDL_Window *window);
static int probe_wait_event(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&event_loop_ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}

#define npe_world_new_game probe_new_game
#define npe_world_restart_game probe_new_game
#define npe_world_prepare_mode2 probe_prepare
#define npe_world_finish_mode2 probe_close
#define npe_world_finish_story probe_story
#define SDL_ShowSimpleMessageBox probe_message_box
#define SDL_WaitEventTimeout probe_wait_event
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_ShowSimpleMessageBox
#undef npe_world_finish_story
#undef npe_world_finish_mode2
#undef npe_world_prepare_mode2
#undef npe_world_new_game
#undef npe_world_restart_game

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    if (npe_world_init(file, world, scenario ? 1 : 36,
                       scenario ? 24 : 51, scenario ? 9 : 14)) return -1;
    world->story_progress = scenario ? 0 : 79;
    world->direction = scenario ? 0 : 2;
    world->money = 100;
    return 0;
}

static int probe_prepare(const NpeFile *file, NpeWorld *world) {
    unsigned npc_id = scenario ? 60 : 93;
    assert(world->game_mode == 2 && world->story_event_id == npc_id);
    if (!prepare_calls) {
        /* First Space now enters the native mode2 controller too, rather
           than displaying the offer through an inline host dialogue. */
        assert(!world->mode2_ready && !world->npc_state[npc_id] && world->money==100);
        assert(npe_world_prepare_mode2(file,world)==1);
        assert(world->mode2_ready && world->npc_state[npc_id]==1 &&
               world->mode2_dialogue_offset==(scenario?33256u:44258u));
        ++prepare_calls;return 1;
    }
    fprintf(stderr,"second NPC load%u state%u ready%u money%u mode%u event%u\n",
            prepare_calls,world->npc_state[npc_id],world->mode2_ready,(unsigned)world->money,world->game_mode,world->story_event_id);
    assert(!world->mode2_ready && world->npc_state[npc_id] == 1);
    assert(world->money == 0);
    assert(npe_world_prepare_mode2(file, world) == 1);
    assert(world->mode2_ready && world->npc_state[npc_id] == 2);
    assert(world->mode2_dialogue_offset == (scenario ? 33412u : 44360u));
    ++prepare_calls;
    return 1;
}

static int probe_close(NpeWorld *world) {
    assert(npe_world_finish_mode2(world) == 1);
    if(!close_calls){
        assert(!world->mode2_ready && world->money==100 && world->game_mode==17 &&
               world->story_event_id==(scenario?2u:5u) && world->npc_state[scenario?60:93]==1);
        ++close_calls;return 1;
    }
    assert(!world->mode2_ready && world->money == 0);
    if (scenario)
        assert(world->game_mode == 1 && world->npc_state[60] == 2);
    else
        /* This display caller returns to the dispatcher before mode14
           draws progress79, then writes80 and enters story96. */
        assert(world->game_mode == 14 && world->story_event_id == 2 &&
               world->story_progress == 79 && world->npc_state[93] == 2);
    ++close_calls;
    return 1;
}

static int probe_story(const NpeFile *file, NpeWorld *world) {
    assert(!scenario && world->story_event_id == 96);
    assert(npe_world_finish_story(file, world) == 1);
    assert(world->game_mode == 1 && world->story_progress == 80 &&
           world->npc_state[93] == 2 && world->money == 0);
    ++story_calls;
    return 1;
}

static int probe_message_box(Uint32 flags, const char *title,
                              const char *message, SDL_Window *window) {
    (void)flags; (void)title; (void)message; (void)window;
    ++desktop_dialogs;
    return 0;
}

static int send_keys(void *unused) {
    static const SDL_Keycode keys[] = {
        SDLK_RETURN, /* title -> the isolated map scenario */
        SDLK_SPACE,  /* talk to the facing NPC */
        SDLK_SPACE, SDLK_SPACE, /* original offer pages */
        SDLK_RIGHT, /* native default is No; move to Yes */
        SDLK_RETURN, /* native confirmation: pay once and enter mode 2 */
        SDLK_SPACE, SDLK_SPACE, SDLK_SPACE /* new text and story pages */
    };
    unsigned waited = 0;
    (void)unused;
    while (!SDL_AtomicGet(&event_loop_ready) && waited < 5000) {
        SDL_Delay(10);
        waited += 10;
    }
    if (!SDL_AtomicGet(&event_loop_ready)) return -1;
    size_t key_count = sizeof(keys) / sizeof(keys[0]) - (scenario ? 1u : 0u);
    for (size_t i = 0; i < key_count; ++i) {
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
    assert(argc == 2);
    arguments[1] = argv[1];
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    for (scenario = 0; scenario < 2; ++scenario) {
        SDL_Thread *thread;
        int thread_result = -1;
        prepare_calls = close_calls = story_calls = desktop_dialogs = 0;
        SDL_AtomicSet(&event_loop_ready, 0);
        thread = SDL_CreateThread(send_keys, "mode2-keys", NULL);
        assert(thread);
        assert(run(2, arguments) == 0);
        SDL_WaitThread(thread, &thread_result);
        fprintf(stderr,"mode2 scenario%u: thread%d loads%u closes%u stories%u dialogs%u\n",
                scenario,thread_result,prepare_calls,close_calls,story_calls,desktop_dialogs);
        assert(thread_result == 0 && prepare_calls == 2 && close_calls == 2);
        assert(story_calls == (scenario ? 0u : 1u) && desktop_dialogs == 0);
    }
    puts("SDL paid NPC dialogue reload, single payment and beggar progression passed");
    return 0;
}
