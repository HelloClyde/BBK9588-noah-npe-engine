#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "../text.h"
#include "ending_native_frames.h"

static SDL_atomic_t ready, finished;
static unsigned npc_calls, story_calls, frames, animation_calls, ending_calls, title_calls, desktop_dialogs;
static unsigned pending_frame = UINT32_MAX, displayed_frames;
static int probe_restart(const NpeFile *, NpeWorld *);
static int probe_mode2(const NpeFile *,NpeWorld *);
static int probe_story(const NpeFile *, NpeWorld *);
static int probe_scene(const NpeFile *, unsigned, unsigned, unsigned, unsigned,
                        unsigned, uint8_t *);
static int probe_animation(const NpeFile *, NpeWorld *);
static int probe_ending(NpeWorld *);
static int probe_title(const NpeFile *, const NpeFont *, unsigned, uint8_t *);
static int probe_message(Uint32, const char *, const char *, SDL_Window *);
static int probe_texture(SDL_Texture *, const SDL_Rect *, const void *, int);
static int probe_wait(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}
#define npe_world_restart_game probe_restart
#define npe_world_prepare_mode2 probe_mode2
#define npe_world_finish_story probe_story
#define npe_render_scene_animation_frame probe_scene
#define npe_world_finish_animation_event probe_animation
#define npe_world_finish_game probe_ending
#define npe_render_native_title_screen probe_title
#define SDL_ShowSimpleMessageBox probe_message
#define SDL_WaitEventTimeout probe_wait
#define SDL_UpdateTexture probe_texture
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_UpdateTexture
#undef SDL_WaitEventTimeout
#undef SDL_ShowSimpleMessageBox
#undef npe_render_native_title_screen
#undef npe_world_finish_game
#undef npe_world_finish_animation_event
#undef npe_render_scene_animation_frame
#undef npe_world_finish_story
#undef npe_world_prepare_mode2
#undef npe_world_restart_game

static int probe_restart(const NpeFile *file, NpeWorld *world) {
    /* Isolated SDL fixture. The continuous world probe reaches this
       position via the original final battle and teleport, then save/load. */
    if(npe_world_restart_game(file,world))return -1;
    NpeWorld saved=*world;
    if (npe_world_init(file, world, 18, 12, 9)) return -1;
    world->sound_observer=saved.sound_observer;world->sound_context=saved.sound_context;
    world->battle_animation_observer=saved.battle_animation_observer;
    world->battle_animation_context=saved.battle_animation_context;
    world->story_progress = 173;
    return 0;
}
static int probe_mode2(const NpeFile *file,NpeWorld *world){
    assert(world->story_event_id==184 && world->x==12 && world->y==8 && world->story_progress==173);
    int result=npe_world_prepare_mode2(file,world);
    assert(result==1 && world->mode2_followup_mode==5 && world->mode2_followup_event_id==200);
    ++npc_calls;
    return result;
}
static int probe_story(const NpeFile *file, NpeWorld *world) {
    int result;
    assert(world->game_mode == 5 && world->story_event_id == 200 + story_calls);
    ++story_calls;
    result = npe_world_finish_story(file, world);
    assert(result == 1 && story_calls <= 4);
    if (story_calls == 4) assert(world->game_mode == 15 && world->story_event_id == 1);
    return result;
}
static int probe_scene(const NpeFile *file, unsigned group, unsigned frame,
                        unsigned x, unsigned y, unsigned mode, uint8_t *pixels) {
    NpeIndexedBitmap bitmap;
    int result;
    if (!group) return npe_render_scene_animation_frame(file,group,frame,x,y,mode,pixels);
    assert(group == 2 && frame == frames);
    assert(pending_frame == UINT32_MAX);
    assert(!npe_scene_animation_frame(file, group, frame, &bitmap));
    result = npe_render_scene_animation_frame(file,group,frame,x,y,mode,pixels);
    ++frames;
    assert(result == 0);
    assert(bitmap.width == NPE_WIDTH && bitmap.height == NPE_HEIGHT);
    pending_frame = frame;
    return result;
}
static int probe_texture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch) {
    if (pending_frame != UINT32_MAX) {
        const uint32_t *rgba = (const uint32_t *)pixels;
        uint64_t hash = UINT64_C(14695981039346656037);
        assert(!rect && pitch == NPE_WIDTH * (int)sizeof(uint32_t));
        assert(pending_frame == displayed_frames && displayed_frames < 37);
        for (unsigned i = 0; i < NPE_WIDTH * NPE_HEIGHT; ++i) {
            unsigned value;
            for (value = 0; value < 16; ++value)
                if (((rgba[i] >> 16) & 255) == 183 - value * 142 / 15) break;
            assert(value < 16);
            hash = (hash ^ value) * UINT64_C(1099511628211);
        }
        /* Check the texture upload after composition, not the source bitmap:
           white pixels must overwrite the underlying temple map too. */
        assert(hash == native_ending_frame_hashes[displayed_frames]);
        ++displayed_frames;
        pending_frame = UINT32_MAX;
    }
    return SDL_UpdateTexture(texture, rect, pixels, pitch);
}
static int probe_animation(const NpeFile *file, NpeWorld *world) {
    int result;
    assert(frames == npe_scene_animation_frames(file, 2) && frames == 37);
    assert(displayed_frames == frames && pending_frame == UINT32_MAX);
    result = npe_world_finish_animation_event(file, world);
    ++animation_calls;
    assert(result == 1 && world->game_mode == 21 && world->story_event_id == 0);
    return result;
}
static int probe_ending(NpeWorld *world) {
    int result = npe_world_finish_game(world);
    ++ending_calls;
    assert(result == 1 && world->game_mode == 7);
    return result;
}
static int probe_title(const NpeFile *file, const NpeFont *font, unsigned selection, uint8_t *pixels) {
    int result = npe_render_native_title_screen(file, font, selection, pixels);
    ++title_calls;
    assert(result == 0);
    if (ending_calls) SDL_AtomicSet(&finished, 1);
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
    for (unsigned key = 0; key < 160 && !SDL_AtomicGet(&finished); ++key) {
        SDL_Event event = {0};
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = key == 0 ? SDLK_RETURN : key == 1 ? SDLK_UP : SDLK_SPACE;
        if (SDL_PushEvent(&event) != 1) return -1;
        SDL_Delay(100);
    }
    {
        SDL_Event event = {0}; event.type = SDL_QUIT;
        if(SDL_PushEvent(&event)!=1)return -1;
        return SDL_AtomicGet(&finished)?0:-1;
    }
}
int main(int argc, char **argv) {
    SDL_Thread *thread;
    int thread_result = -1;
    char *arguments[] = {"npe-sdl", NULL, NULL};
    assert(argc == 2); arguments[1] = argv[1];
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    thread = SDL_CreateThread(send_keys, "ending-keys", NULL);
    assert(thread && run(2, arguments) == 0);
    SDL_WaitThread(thread, &thread_result);
    assert(thread_result == 0 && npc_calls == 1 && story_calls == 4 &&
           animation_calls == 1 && ending_calls == 1 && title_calls >= 2 && desktop_dialogs == 0);
    printf("SDL original farewell, %u full-screen animation frames matching native pixels and return to NPE title passed\n", displayed_frames);
    return 0;
}

