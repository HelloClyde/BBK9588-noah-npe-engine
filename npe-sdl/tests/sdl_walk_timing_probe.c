#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
static Uint32 clock_ms, expected_commit;
static unsigned commits, waits, ordinary_keys;
static int quit_wait;
static Uint32 test_ticks(void) { return clock_ms; }
static int test_wait(SDL_Event *event,int timeout) {
    assert(timeout>0 && timeout<=16);
    assert(!commits); /* The next pose must not be visible during its wait. */
    ++waits;
    memset(event,0,sizeof(*event));
    if(quit_wait) { event->type=SDL_QUIT;return 1; }
    if(ordinary_keys) {
        --ordinary_keys;event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RIGHT;
        ++clock_ms;return 1;
    }
    clock_ms+=(Uint32)timeout;return 0;
}
static void test_present(SDL_Renderer *renderer) {
    (void)renderer;assert(clock_ms==expected_commit);++commits;
}
#define SDL_GetTicks test_ticks
#define SDL_WaitEventTimeout test_wait
#define SDL_RenderPresent test_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent

static void reset(Uint32 start,Uint32 expected) {
    clock_ms=start;expected_commit=expected;commits=waits=ordinary_keys=0;quit_wait=0;
    SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
}
int main(void) {
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1));
    assert(!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("walk",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    WalkDisplay display={renderer,texture};uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]={0};
    const Uint32 starts[]={0,1,15,16,31,1234,0xfffffff8u};
    const Uint32 ends[]={16,16,16,32,32,1235,1};
    for(unsigned i=0;i<sizeof(starts)/sizeof(starts[0]);++i) {
        reset(starts[i],ends[i]);assert(!commit_world_walk(&display,pixels,10));
        assert(commits==1 && waits && clock_ms==ends[i]);
    }
    reset(0,16);ordinary_keys=2;
    assert(!commit_world_walk(&display,pixels,10));assert(commits==1 && !ordinary_keys);
    reset(1234,1235);quit_wait=1;
    assert(commit_world_walk(&display,pixels,10)==1 && commits==0 && clock_ms==1234);
    SDL_Event event;assert(SDL_PollEvent(&event) && event.type==SDL_QUIT);
    reset(1234,1234);assert(!commit_world_walk(&display,pixels,0));assert(commits==1 && !waits);
    /* Four poses remain on the shared phase: 1,15,16,16 ms at this start. */
    Uint32 current=1234;const Uint32 pose_ends[]={1235,1250,1266,1282};
    for(unsigned i=0;i<4;++i) {
        reset(current,pose_ends[i]);assert(!commit_world_walk(&display,pixels,10));
        assert(commits==1);current=clock_ms;
    }
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    puts("PASS walk: shared 64-Hz phase, wait-before-commit, ordinary input consumption, quit, zero wait and Uint32 wrap");
    return 0;
}
