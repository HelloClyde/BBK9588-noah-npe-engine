#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint16_t word(const uint8_t *p) {
    return (uint16_t)((unsigned)p[0] | (unsigned)p[1] << 8);
}
static uint8_t *read_bytes(const char *path, size_t *size) {
    FILE *input = fopen(path, "rb");
    assert(input && !fseek(input, 0, SEEK_END));
    long length = ftell(input);
    assert(length > 0 && !fseek(input, 0, SEEK_SET));
    uint8_t *bytes = malloc((size_t)length);
    assert(bytes && fread(bytes, 1, (size_t)length, input) == (size_t)length);
    fclose(input); *size = (size_t)length; return bytes;
}
int main(int argc, char **argv) {
    assert(argc == 3);
    size_t input_size, trace_size;
    uint8_t *input = read_bytes(argv[1], &input_size);
    uint8_t *trace = read_bytes(argv[2], &trace_size);
    assert(input_size % 8 == 0 && trace_size == input_size / 8 * 12);
    NpeMap map = {0}; NpeViewport view;
    assert(npe_map_spawn_viewport(NULL, 0, 0, &view) == -1);
    assert(npe_map_spawn_viewport(&map, 0, 0, NULL) == -1);
    for (size_t i = 0; i < input_size / 8; ++i) {
        const uint8_t *cfg = input + i * 8, *native = trace + i * 12;
        map.width = (uint8_t)word(cfg); map.height = (uint8_t)word(cfg + 2);
        assert(!npe_map_spawn_viewport(&map, word(cfg + 4), word(cfg + 6), &view));
        assert(view.offset_x == word(native + 4) >> 8);
        assert(view.offset_y == word(native + 6) >> 8);
        assert(view.screen_x == (word(native + 8) & 255));
        assert(view.screen_y == word(native + 10) >> 8);
    }
    printf("%zu original map spawn coordinate cases matched\n", input_size / 8);
    free(input); free(trace); return 0;
}
