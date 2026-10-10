#include "../npe.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const uint8_t text[] = {0xb2, 0xdd, 0xd2, 0xa9, ' ', 0x80};
    const uint8_t bad[] = {0xff, 'A'};
    char *utf8 = npe_gbk_to_utf8(text, sizeof(text));
    assert(utf8 && strcmp(utf8, "\xe8\x8d\x89\xe8\x8d\xaf \xe2\x82\xac") == 0);
    free(utf8);
    utf8 = npe_gbk_to_utf8(bad, sizeof(bad));
    assert(utf8 && strcmp(utf8, "\xef\xbf\xbd" "A") == 0);
    free(utf8);
    return 0;
}
