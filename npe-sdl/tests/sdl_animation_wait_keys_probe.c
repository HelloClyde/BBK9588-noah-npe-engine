#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif

static Uint32 logical_clock;
static Uint32 test_ticks(void) { return logical_clock; }
static void test_delay(Uint32 ms) { logical_clock += ms; }
#define SDL_GetTicks test_ticks
#define SDL_Delay test_delay
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay

static unsigned fixture_word(const uint8_t *bytes) {
    return (unsigned)bytes[0] | (unsigned)bytes[1] << 8;
}

static void queue_key(Uint32 type, SDL_Keycode key) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.keysym.sym = key;
    assert(SDL_PushEvent(&event) == 1);
}

static int check_key(unsigned parameter, SDL_Keycode key, int accepted) {
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    logical_clock = 0;
    queue_key(SDL_KEYDOWN, key);
    int result = scene_frame_wait_parameter(parameter);
    /* This is the existing SDL logical deadline, not a device duration. */
    unsigned ticks = parameter * 10u * 64u / 1000u;
    unsigned expected = accepted ? 0 : (ticks * 1000u + 63u) / 64u;
    if (result || logical_clock != expected) {
        fprintf(stderr, "Animation wait parameter%u SDL key%d: original accepts=%d, result=%d elapsed=%u expected=%u\n",
                parameter, (int)key, accepted, result, logical_clock, expected);
        return 1;
    }
    SDL_Event event;
    assert(!SDL_PollEvent(&event));
    return 0;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *stream = fopen(argv[1], "rb");
    assert(stream);
    uint8_t header[12], rows[512 * 8];
    assert(fread(header, 1, sizeof(header), stream) == sizeof(header));
    assert(!memcmp(header, "NPEWKEY1", 8) && fixture_word(header + 8) == 512 && !fixture_word(header + 10));
    assert(fread(rows, 1, sizeof(rows), stream) == sizeof(rows) && fgetc(stream) == EOF && !fclose(stream));
    assert(!SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) && !SDL_Init(SDL_INIT_VIDEO));
    unsigned comparisons = 0, ignored = 0, accepted_count = 0;
    const SDL_Keycode extra[] = {SDLK_BACKSPACE, SDLK_UP, SDLK_DOWN, SDLK_LEFT,
        SDLK_RIGHT, SDLK_RETURN, SDLK_KP_ENTER, SDLK_F4, SDLK_ESCAPE};
    const unsigned native_extra[] = {0x89, 0x82, 0x83, 0x84, 0x85, 0x86, 0x86, 0x8e, 0x8e};
    for (unsigned p = 0; p < 2; ++p) {
        unsigned parameter = p ? 20 : 15;
        int accepted[256];
        for (unsigned key = 0; key < 256; ++key) {
            const uint8_t *row = rows + (p * 256 + key) * 8;
            unsigned result = fixture_word(row), reads = fixture_word(row + 2);
            unsigned ms = fixture_word(row + 4), flag = fixture_word(row + 6);
            assert(ms == parameter * 10 && (reads == 1 || reads == 2));
            accepted[key] = reads == 1;
            assert(result == (accepted[key] ? key : 0xf7) && flag == (accepted[key] ? 0 : 0x0400));
        }
        for (SDL_Keycode key = 32; key <= 126; ++key) {
            unsigned native_key = key == SDLK_SPACE ? 0x8d : (unsigned)key;
            if (check_key(parameter, key, accepted[native_key])) return 1;
            accepted_count += accepted[native_key];
            ignored += !accepted[native_key];
            ++comparisons;
        }
        for (unsigned i = 0; i < sizeof(extra) / sizeof(extra[0]); ++i) {
            if (check_key(parameter, extra[i], accepted[native_extra[i]])) return 1;
            accepted_count += accepted[native_extra[i]];
            ignored += !accepted[native_extra[i]];
            ++comparisons;
        }
        /* An ignored key must not leave the accepted key for the next frame. */
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        logical_clock = 0;
        queue_key(SDL_KEYDOWN, SDLK_BACKSPACE);
        queue_key(SDL_KEYDOWN, SDLK_RETURN);
        queue_key(SDL_KEYDOWN, SDLK_SPACE);
        assert(!scene_frame_wait_parameter(parameter) && !logical_clock);
        SDL_Event event;
        assert(SDL_PollEvent(&event) && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE);
        assert(!SDL_PollEvent(&event));
        /* Host release events wait for the existing logical deadline. */
        queue_key(SDL_KEYUP, SDLK_RETURN);
        assert(!scene_frame_wait_parameter(parameter));
        assert(logical_clock == (parameter * 10u * 64u / 1000u * 1000u + 63u) / 64u);
        assert(!SDL_PollEvent(&event));
        /* Closing the host window aborts and preserves the quit event. */
        logical_clock = 0;
        queue_key(SDL_QUIT, SDLK_UNKNOWN);
        assert(scene_frame_wait_parameter(parameter) == -1 && !logical_clock);
        /* PollEvent's internal poll sentinel can precede a requeued event. */
        assert(SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUIT, SDL_QUIT) == 1 && event.type == SDL_QUIT);
        assert(SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUIT, SDL_QUIT) == 0);
    }
    SDL_Quit();
    assert(comparisons == 208 && ignored == 190 && accepted_count == 18);
    printf("Animation wait keys:512 original cases,208 actual SDL key checks,190 ignored,18 accepted,6 queue/release/quit checks passed\n");
    return 0;
}
