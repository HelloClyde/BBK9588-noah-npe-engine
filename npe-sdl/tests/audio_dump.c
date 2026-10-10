#include "../audio.h"
#include <stdio.h>
#include <stdlib.h>

/* Offline verification helper: decode one complete format-13 section to
   little-endian signed 16-bit PCM. The player uses the same decoder code. */
int main(int argc, char **argv) {
    FILE *input, *output;
    long length;
    uint8_t *bytes;
    NpeFile file;
    NpeAudioResource resource;
    NpeAudioMode5Decoder decoder;
    int16_t samples[NPE_AUDIO_MODE5_PCM_SAMPLES];
    char error[160], *end;
    unsigned section, frame, i;
    if (argc != 4) {
        fprintf(stderr, "usage: audio-dump game.npe section output.pcm\n");
        return 2;
    }
    section = (unsigned)strtoul(argv[2], &end, 10);
    if (*end || section < 16 || section > 37) return 2;
    input = fopen(argv[1], "rb");
    if (!input) return 2;
    if (fseek(input, 0, SEEK_END) || (length = ftell(input)) <= 0 ||
        fseek(input, 0, SEEK_SET)) { fclose(input); return 2; }
    bytes = malloc((size_t)length);
    if (!bytes || fread(bytes, 1, (size_t)length, input) != (size_t)length) {
        free(bytes); fclose(input); return 2;
    }
    fclose(input);
    if (npe_parse(&file, bytes, (size_t)length, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error); free(bytes); return 2;
    }
    if (npe_audio_resource(&file, section, &resource)) {
        npe_free(&file); return 2;
    }
    output = fopen(argv[3], "wb");
    if (!output) { npe_free(&file); return 2; }
    npe_audio_mode5_init(&decoder);
    for (frame = 0; frame < resource.header_count; ++frame) {
        if (npe_audio_mode5_decode(&decoder, &resource, frame, samples)) {
            fprintf(stderr, "decode failed at frame %u\n", frame);
            fclose(output); npe_free(&file); return 1;
        }
        for (i = 0; i < NPE_AUDIO_MODE5_PCM_SAMPLES; ++i) {
            uint16_t word = (uint16_t)samples[i];
            if (fputc((uint8_t)word, output) == EOF ||
                fputc((uint8_t)(word >> 8), output) == EOF) {
                fclose(output); npe_free(&file); return 1;
            }
        }
    }
    if (fclose(output)) { npe_free(&file); return 1; }
    npe_free(&file);
    return 0;
}
