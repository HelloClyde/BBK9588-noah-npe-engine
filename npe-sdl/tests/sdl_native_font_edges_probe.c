#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
#include "../text.c"

static unsigned fixture_word(const uint8_t *bytes) {
    return (unsigned)bytes[0] | (unsigned)bytes[1] << 8;
}

int main(int argc, char **argv) {
    assert(argc == 4);
    NpeFont font = {0};
    assert(!npe_font_load(&font, argv[1]) && !npe_font_load16(&font, argv[2]));
    FILE *stream = fopen(argv[3], "rb");
    assert(stream);
    uint8_t header[16];
    assert(fread(header, 1, 12, stream) == 12);
    assert(!memcmp(header, "NPEFED01", 8));
    unsigned count = fixture_word(header + 8) | fixture_word(header + 10) << 16;
    assert(count == 60);
    assert(!SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) && !SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("original font drawing boundaries", 0, 0,
                                         NPE_WIDTH, NPE_HEIGHT, 0);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, NPE_WIDTH, NPE_HEIGHT);
    assert(texture);
    for (unsigned i = 0; i < count; ++i) {
        uint8_t input[64], expected[NPE_WIDTH * NPE_HEIGHT], pixels[NPE_WIDTH * NPE_HEIGHT];
        uint32_t rgba[NPE_WIDTH * NPE_HEIGHT];
        assert(fread(header, 1, sizeof(header), stream) == sizeof(header));
        unsigned height = fixture_word(header), pitch = fixture_word(header + 2);
        unsigned left = fixture_word(header + 4), top = fixture_word(header + 6);
        unsigned color = fixture_word(header + 8), size = fixture_word(header + 10);
        unsigned end_x = fixture_word(header + 12), background = fixture_word(header + 14);
        assert(size < sizeof(input) && color < 16 && background < 16);
        assert((height == 12 && (pitch == 6 || pitch == 8)) || (height == 16 && pitch == 8));
        assert(fread(input, 1, size, stream) == size);
        assert(fread(expected, 1, sizeof(expected), stream) == sizeof(expected));
        memset(pixels, (int)background, sizeof(pixels));
        int result = height == 16 ?
            npe_draw_gbk_text16_color(&font, input, size, left, top, (uint8_t)color, pixels) :
            pitch == 6 ? draw_native12_text_color(&font, input, size, left, top,
                                                pitch, (uint8_t)color, pixels) :
            npe_draw_gbk_text_color(&font, input, size, left, top, (uint8_t)color, pixels);
        size_t mismatch = 0;
        while (mismatch < sizeof(pixels) && pixels[mismatch] == expected[mismatch]) ++mismatch;
        if (result || mismatch != sizeof(pixels)) {
            fprintf(stderr, "Original font edge %u height%u pitch%u x%u y%u end%u: result=%d",
                    i, height, pitch, left, top, end_x, result);
            if (mismatch != sizeof(pixels))
                fprintf(stderr, ", first pixel %zu,%zu C=%u original=%u",
                        mismatch % NPE_WIDTH, mismatch / NPE_WIDTH,
                        pixels[mismatch], expected[mismatch]);
            fputc('\n', stderr);
            return 1;
        }
        draw_pixels(renderer, texture, pixels);
        assert(!SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, rgba, NPE_WIDTH * 4));
        for (size_t p = 0; p < sizeof(expected); ++p) {
            unsigned v = expected[p];
            uint32_t wanted = UINT32_C(0xff000000) | ((183 - v * 142 / 15) << 16) |
                              ((200 - v * 146 / 15) << 8) | (161 - v * 119 / 15);
            assert(rgba[p] == wanted);
        }
    }
    assert(fgetc(stream) == EOF && !fclose(stream));
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    npe_font_free(&font);
    printf("Original font edges: %u complete C/actual SDL framebuffer comparisons passed\n", count);
    return 0;
}
