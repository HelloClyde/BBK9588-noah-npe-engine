#include "npe.h"
#include <stdlib.h>

#include "gbk_table.inc"

char *npe_gbk_to_utf8(const uint8_t *bytes, size_t size) {
    char *text;
    size_t i = 0, used = 0;
    if (!bytes || size > (SIZE_MAX - 1) / 3) return NULL;
    text = malloc(size * 3 + 1);
    if (!text) return NULL;
    while (i < size) {
        unsigned value = bytes[i++];
        if (value == 0x80) value = 0x20ac; /* Windows CP936 extension. */
        else if (value >= 0x81 && value <= 0xfe && i < size) {
            unsigned trail = bytes[i];
            if (trail >= 0x40 && trail <= 0xfe && trail != 0x7f) {
                unsigned column = trail < 0x7f ? trail - 0x40 : trail - 0x41;
                unsigned decoded = gbk_unicode[(value - 0x81) * 190 + column];
                if (decoded) { value = decoded; ++i; }
                else value = 0xfffd;
            } else value = 0xfffd;
        } else if (value >= 0x81) value = 0xfffd;
        if (value < 0x80) text[used++] = (char)value;
        else if (value < 0x800) {
            text[used++] = (char)(0xc0 | (value >> 6));
            text[used++] = (char)(0x80 | (value & 0x3f));
        } else {
            text[used++] = (char)(0xe0 | (value >> 12));
            text[used++] = (char)(0x80 | ((value >> 6) & 0x3f));
            text[used++] = (char)(0x80 | (value & 0x3f));
        }
    }
    text[used] = 0;
    return text;
}
