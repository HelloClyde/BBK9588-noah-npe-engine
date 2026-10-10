#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"

static SDL_atomic_t ready, finished;
static unsigned moves, frames, walk_calls, story_calls, desktop_dialogs;
static int probe_new_game(const NpeFile *, NpeWorld *);
static int probe_restart(const NpeFile *, NpeWorld *);
static int probe_step(const NpeFile *, NpeWorld *, unsigned,
                        uint8_t *,NpeMapDisplayCommit,void *);
static int probe_walk(const NpeFile *, NpeWorld *,uint8_t *,NpeMapDisplayCommit,void *);
static int probe_story(const NpeFile *, NpeWorld *);
static int probe_message(Uint32, const char *, const char *, SDL_Window *);
static int probe_wait(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}
#define npe_world_restart_game probe_restart
#define npe_world_new_game probe_new_game
#define npe_world_step_with_display probe_step
#define npe_world_script_walk_with_display probe_walk
#define npe_world_finish_story probe_story
#define SDL_ShowSimpleMessageBox probe_message
#define SDL_WaitEventTimeout probe_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_ShowSimpleMessageBox
#undef npe_world_finish_story
#undef npe_world_script_walk_with_display
#undef npe_world_step_with_display
#undef npe_world_new_game
#undef npe_world_restart_game

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    /* Isolated SDL acceptance fixture; the continuous world probe reaches
       this same point through the two original temple guard battles. */
    if (npe_world_init(file, world, 126, 20, 28)) return -1;
    world->story_progress = 166;
    return 0;
}
static int probe_restart(const NpeFile *file,NpeWorld *world){return probe_new_game(file,world);}
static int probe_step(const NpeFile *file, NpeWorld *world, unsigned direction,
                        uint8_t *pixels,NpeMapDisplayCommit commit,void *context) {
    int result = npe_world_step_with_display(file, world, direction,pixels,commit,context);
    ++moves;
    fprintf(stderr,"temple move%u result%d mode%u progress%u pos%u,%u\n",moves,result,world->game_mode,world->story_progress,world->x,world->y);
    assert(direction == 2);
    if (moves <= 8)
        assert(result == NPE_MOVE_OK && world->story_progress == 166 && world->y == 28 - moves);
    else
        assert(moves == 9 && result == NPE_MOVE_EVENT && world->story_progress == 166 &&
               world->game_mode == 14 && world->x == 20 && world->y == 20);
    return result;
}
typedef struct {NpeWorld *world;NpeMapDisplayCommit commit;void *context;} TempleDisplay;
static int temple_commit(void *opaque,const uint8_t *pixels,unsigned parameter){
    TempleDisplay *d=opaque;
    if(pixels){
        ++frames;unsigned stage=frames>1?frames-2:0;
        unsigned y=frames==1?20:20-stage/5-(stage%5==4);
        assert(frames<=36&&d->world->x==20&&d->world->y==y);
    }
    return d->commit?d->commit(d->context,pixels,parameter):0;
}
static int probe_walk(const NpeFile *file,NpeWorld *world,uint8_t *pixels,NpeMapDisplayCommit commit,void *context){
    assert(!frames&&world->game_mode==19&&world->x==20&&world->y==20);
    TempleDisplay d={world,commit,context};
    int result=npe_world_script_walk_with_display(file,world,pixels,temple_commit,&d);++walk_calls;
    assert(result==1&&frames==36&&world->x==20&&world->y==13&&world->game_mode==5&&world->story_event_id==181);
    return result;
}

static int probe_story(const NpeFile *file, NpeWorld *world) {
    int result;
    assert(world->game_mode == 5 && world->story_event_id == 181 && world->story_progress == 168);
    result = npe_world_finish_story(file, world);
    ++story_calls;
    assert(result == 1 && world->game_mode == 1 && world->x == 20 && world->y == 13);
    SDL_AtomicSet(&finished, 1);
    return result;
}
static int probe_message(Uint32 flags, const char *title, const char *message, SDL_Window *window) {
    (void)flags; (void)title; (void)message; (void)window;
    ++desktop_dialogs;
    return 0;
}
static int send_keys(void *unused) {
    unsigned waited = 0;
    (void)unused;
    while (!SDL_AtomicGet(&ready) && waited < 5000) {
        SDL_Delay(10); waited += 10;
    }
    if (!SDL_AtomicGet(&ready)) return -1;
    for (unsigned key = 0; key < 80 && !SDL_AtomicGet(&finished); ++key) {
        SDL_Event event = {0};
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = key >= 1 && key <= 9 ? SDLK_UP : SDLK_RETURN;
        if (SDL_PushEvent(&event) != 1) return -1;
        SDL_Delay(100);
    }
    if (!SDL_AtomicGet(&finished)) return -1;
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
    thread = SDL_CreateThread(send_keys, "temple-walk-keys", NULL);
    assert(thread && run(2, arguments) == 0);
    SDL_WaitThread(thread, &thread_result);
    assert(thread_result == 0 && moves == 9 && frames == 36 && walk_calls == 1 &&
           story_calls == 1 && desktop_dialogs == 0);
    puts("SDL temple trigger, 36 retained walk commits and original story followup passed");
    return 0;
}
