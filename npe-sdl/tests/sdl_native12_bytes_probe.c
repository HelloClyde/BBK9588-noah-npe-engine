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

static unsigned little_word(const uint8_t *bytes) {
    return (unsigned)bytes[0] | (unsigned)bytes[1] << 8;
}

int main(int argc, char **argv) {
    assert(argc == 4);
    NpeFont font = {0};
    assert(!npe_font_load(&font, argv[1]) && !npe_font_load16(&font, argv[2]));
    FILE *stream = fopen(argv[3], "rb");
    assert(stream);
    uint8_t header[12];
    assert(fread(header, 1, sizeof(header), stream) == sizeof(header));
    assert(!memcmp(header, "NPE12B01", 8));
    unsigned count = little_word(header + 8) | little_word(header + 10) << 16;
    assert(count == 36);
    assert(!SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) && !SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("original C-font strings", 0, 0, NPE_WIDTH, NPE_HEIGHT, 0);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, NPE_WIDTH, NPE_HEIGHT);
    assert(texture);
    unsigned glyphs = 0;
    for (unsigned i = 0; i < count; ++i) {
        uint8_t input[64], expected[NPE_WIDTH * NPE_HEIGHT], pixels[NPE_WIDTH * NPE_HEIGHT] = {0};
        uint32_t rgba[NPE_WIDTH * NPE_HEIGHT];
        assert(fread(header, 1, sizeof(header), stream) == sizeof(header));
        unsigned left = little_word(header), top = little_word(header + 2);
        unsigned pitch = little_word(header + 4), size = little_word(header + 6);
        unsigned expected_glyphs = little_word(header + 8), end_x = little_word(header + 10);
        assert(size < sizeof(input) && (pitch == 6 || pitch == 8));
        assert(fread(input, 1, size, stream) == size);
        assert(fread(expected, 1, sizeof(expected), stream) == sizeof(expected));
        int result = pitch == 6 ? draw_native12_compact(&font, input, size, left, top, pixels) :
            npe_draw_gbk_text_color(&font, input, size, left, top, 15, pixels);
        if (result || memcmp(pixels, expected, sizeof(pixels))) {
            fprintf(stderr, "Original C-font string %u pitch%u: result=%d, first-byte=%02X\n",
                    i, pitch, result, size ? input[0] : 0);
            return 1;
        }
        unsigned cursor = left, observed = 0;
        for (size_t pos = 0; pos < size && input[pos];) {
            size_t advance = 0;
            assert(glyph(&font, input + pos, size - pos, &advance));
            assert(advance == 1 || advance == 2);
            cursor += advance == 1 ? pitch : 12;
            pos += advance;
            ++observed;
        }
        assert(cursor == end_x && observed == expected_glyphs);
        glyphs += observed;
        draw_pixels(renderer, texture, pixels);
        assert(!SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, rgba, NPE_WIDTH * 4));
        for (size_t p = 0; p < sizeof(expected); ++p) {
            unsigned v = expected[p];
            uint32_t wanted = UINT32_C(0xff000000) | ((183 - v * 142 / 15) << 16) |
                              ((200 - v * 146 / 15) << 8) | (161 - v * 119 / 15);
            assert(rgba[p] == wanted);
        }
    }
    assert(glyphs == 128 && fgetc(stream) == EOF && !fclose(stream));
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    npe_font_free(&font);
    printf("Original C-font strings: %u C/actual SDL rasters, %u glyph positions passed\n", count, glyphs);
    return 0;
}
