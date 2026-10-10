#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"

static SDL_atomic_t event_loop_ready;
static unsigned moves, desktop_dialogs, open_frames;
static int probe_new_game(const NpeFile *file, NpeWorld *world);
static int probe_render_world(const NpeFile *file, const NpeWorld *world, uint8_t *pixels);
static int probe_render_load(const NpeFile *file,const NpeWorld *world,uint8_t *pixels);
static int probe_step(const NpeFile *file, NpeWorld *world, unsigned direction,
                        uint8_t *pixels,NpeMapDisplayCommit commit,void *context);
static int probe_render(const NpeFile *file, const NpeWorld *world,
                          unsigned frame, uint8_t *pixels);
static int probe_message_box(Uint32 flags, const char *title,
                              const char *message, SDL_Window *window);
static int probe_wait_event(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&event_loop_ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}
#define npe_world_new_game probe_new_game
#define npe_world_step_with_display probe_step
#define npe_render_world_frame probe_render
#define npe_render_world probe_render_world
#define npe_render_world_load probe_render_load
#define SDL_ShowSimpleMessageBox probe_message_box
#define SDL_WaitEventTimeout probe_wait_event
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_ShowSimpleMessageBox
#undef npe_render_world_frame
#undef npe_render_world
#undef npe_render_world_load
#undef npe_world_step_with_display
#undef npe_world_new_game

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    if (npe_world_init(file, world, 108, 4, 13)) return -1;
    world->story_progress = 115;
    return 0;
}
static int probe_step(const NpeFile *file, NpeWorld *world, unsigned direction,
                        uint8_t *pixels,NpeMapDisplayCommit commit,void *context) {
    int result = npe_world_step_with_display(file, world, direction,pixels,commit,context);
    ++moves;
    fprintf(stderr,"Tile route move%u direction%u result%d mode%u event%u progress%u position%u,%u\n",moves,direction,result,world->game_mode,world->story_event_id,world->story_progress,world->x,world->y);
    if (moves == 1)
        assert(direction == 2 && result == NPE_MOVE_EVENT &&
               world->game_mode == 5 && world->story_event_id == 206);
    else {
        assert(result == NPE_MOVE_OK && world->story_progress == 116 &&
               world->tile_change_applied == 1);
        /* Retained walking avoids whole-map redraws. Verify the open-door
           tile state on all16 moves; the initial fresh frame is checked
           below and retained-LCD pixels have their own original fixtures. */
        NpeMap map;assert(!npe_map(file,107,&map));
        assert(npe_world_tile(world,&map,12,8)==10 &&
               npe_world_tile(world,&map,13,8)==10);
        if (moves == 17) assert(world->x == 12 && world->y == 5);
    }
    return result;
}
static int probe_render(const NpeFile *file, const NpeWorld *world,
                          unsigned frame, uint8_t *pixels) {
    int result = npe_render_world_frame(file, world, frame, pixels);
    if (world->tile_change_applied) {
        NpeMap map;
        assert(npe_map(file, 107, &map) == 0 &&
               npe_world_tile(world, &map, 12, 8) == 10 &&
               npe_world_tile(world, &map, 13, 8) == 10 &&
               npe_map_tile(&map, 12, 8) == 196);
        ++open_frames;
    }
    return result;
}
static int probe_render_world(const NpeFile *file, const NpeWorld *world, uint8_t *pixels) {
    int result = npe_render_world(file, world, pixels);
    if (world->tile_change_applied) ++open_frames;
    return result;
}
static int probe_render_load(const NpeFile *file,const NpeWorld *world,uint8_t *pixels) {
    int result=npe_render_world_load(file,world,pixels);
    if(world->tile_change_applied){
        NpeMap map;assert(!npe_map(file,107,&map)&&npe_world_tile(world,&map,12,8)==10&&npe_map_tile(&map,12,8)==196);
        ++open_frames;
    }
    return result;
}
static int probe_message_box(Uint32 flags, const char *title,
                              const char *message, SDL_Window *window) {
    (void)flags; (void)title; (void)message; (void)window;
    ++desktop_dialogs; return 0;
}
static int send_keys(void *unused) {
    const char *route = "RRRRRRUUURURUUUU";
    unsigned waited = 0;
    (void)unused;
    while (!SDL_AtomicGet(&event_loop_ready) && waited < 5000) {
        SDL_Delay(10); waited += 10;
    }
    if (!SDL_AtomicGet(&event_loop_ready)) return -1;
    for (unsigned i = 0; i < 3 + strlen(route); ++i) {
        SDL_Event event = {0};
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = i == 0 || i == 2 ? SDLK_RETURN :
                              i == 1 ? SDLK_UP :
                              route[i - 3] == 'R' ? SDLK_RIGHT : SDLK_UP;
        if (SDL_PushEvent(&event) != 1) return -1;
        SDL_Delay(90);
    }
    {
        SDL_Event event = {0}; event.type = SDL_QUIT;
        return SDL_PushEvent(&event) == 1 ? 0 : -1;
    }
}
int main(int argc, char **argv) {
    SDL_Thread *thread;
    int thread_result = -1;
    char *arguments[] = {"npe-sdl", "--new-game", NULL, NULL};
    assert(argc == 2); arguments[2] = argv[1];
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    thread = SDL_CreateThread(send_keys, "tile-change-keys", NULL);
    assert(thread && run(3, arguments) == 0);
    SDL_WaitThread(thread, &thread_result);
    assert(thread_result == 0 && moves == 17 && open_frames >= 1 && desktop_dialogs == 0);
    puts("SDL original switch story, door rendering and passage passed");
    return 0;
}
