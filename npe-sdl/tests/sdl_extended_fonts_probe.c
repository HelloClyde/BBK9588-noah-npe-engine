#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main

static unsigned word_le(const uint8_t *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static FILE *fixture_open(const char *directory, const char *name) {
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/%s", directory, name);
    assert(n > 0 && (size_t)n < sizeof(path));
    FILE *stream = fopen(path, "rb");
    assert(stream);
    return stream;
}

int main(int argc, char **argv) {
    assert(argc == 4);
    NpeFont font = {0};
    assert(!npe_font_load(&font, argv[1]) && !npe_font_load16(&font, argv[2]));
    FILE *stream = fixture_open(argv[3], "unpacked12.bin");
    unsigned unpacked = 0;
    for (unsigned group = 0; group < 2; ++group) {
        unsigned first = group ? 0xa8 : 0x81, end = group ? 0xfe : 0xa0;
        unsigned last_trail = group ? 0xa0 : 0xfe;
        for (unsigned lead = first; lead <= end; ++lead) {
            for (unsigned trail = 0x40; trail <= last_trail; ++trail) {
                uint8_t original[24];
                assert(fread(original, 1, sizeof(original), stream) == sizeof(original));
                size_t slot = 95 + (lead - 0x81) * 191 + trail - 0x40;
                assert(!memcmp(original, font.data + 8 + slot * 24, sizeof(original)));
                ++unpacked;
            }
        }
    }
    assert(unpacked == 14551 && fgetc(stream) == EOF && !fclose(stream));
    stream = fixture_open(argv[3], "extended16.bin");
    for (size_t i = 0; i < 14551u * 32; ++i)
        assert(fgetc(stream) == font.data16[8 + (95 + 94 * 94 + 33) * 32 + i]);
    assert(fgetc(stream) == EOF && !fclose(stream));

    assert(!SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) && !SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("original extended glyphs", 0, 0,
                                          NPE_WIDTH, NPE_HEIGHT, 0);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, NPE_WIDTH, NPE_HEIGHT);
    assert(texture);
    stream = fixture_open(argv[3], "glyphs.bin");
    uint8_t header[12];
    assert(fread(header, 1, sizeof(header), stream) == sizeof(header));
    assert(!memcmp(header, "NPEXT001", 8));
    unsigned cases = word_le(header + 8) | word_le(header + 10) << 16;
    assert(cases == 150);
    for (unsigned i = 0; i < cases; ++i) {
        uint8_t record[40], pixels[NPE_WIDTH * NPE_HEIGHT], expected[NPE_WIDTH * NPE_HEIGHT];
        uint32_t rgba[NPE_WIDTH * NPE_HEIGHT];
        assert(fread(record, 1, sizeof(record), stream) == sizeof(record));
        unsigned height = word_le(record), left = word_le(record + 2);
        unsigned top = word_le(record + 4), code = word_le(record + 6);
        uint8_t text[] = {(uint8_t)(code >> 8), (uint8_t)code};
        assert(height == 12 || height == 16);
        memset(pixels, 7, sizeof(pixels));
        memset(expected, 7, sizeof(expected));
        if (height == 12) {
            assert(npe_gbk_page_end(text, sizeof(text), 0) == sizeof(text));
            assert(!npe_draw_gbk_text_color(&font, text, sizeof(text), left, top, 15, pixels));
        } else {
            assert(!npe_draw_gbk_text16_color(&font, text, sizeof(text), left, top, 15, pixels));
        }
        for (unsigned y = 0; y < height; ++y) {
            unsigned mask = (unsigned)record[8 + 2 * y] << 8 | record[9 + 2 * y];
            for (unsigned x = 0; x < height; ++x)
                if (mask & (0x8000u >> x)) expected[(top + y) * NPE_WIDTH + left + x] = 15;
        }
        assert(!memcmp(pixels, expected, sizeof(pixels)));
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
    printf("%u original glyphs: C and actual SDL raster passed; all %u original unpacks and 14551 flash-16 glyphs matched\n",
           cases, unpacked);
    return 0;
}
