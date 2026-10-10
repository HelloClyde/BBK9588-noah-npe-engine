#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void be32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
}
static void le16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
}
int main(void) {
    enum { TABLE_END = 0x35a, S0 = TABLE_END, S1 = S0 + 16,
           S2 = S1 + 30, S3 = S2 + 11, S4 = S3 + 1,
           S5 = S4 + 1, END = S5 + 48 };
    uint8_t *blob = calloc(END, 1);
    NpeFile file;
    NpeIndexedBitmap image;
    NpeNpcAction action;
    NpeStoryRecord story;
    NpeDialoguePage page;
    const uint8_t *record;
    size_t length;
    char error[120];
    assert(blob);
    memcpy(blob, "NP560 RPG2.1", 12);
    blob[0x38] = (uint8_t)(S1 >> 8); blob[0x39] = (uint8_t)S1;
    be32(blob + 0x3a, S0); be32(blob + 0x3e, 16);
    be32(blob + 0x42, S1); be32(blob + 0x46, 30);
    be32(blob + 0x4a, S2); be32(blob + 0x4e, 11);
    be32(blob + 0x52, S3); be32(blob + 0x56, 1);
    be32(blob + 0x5a, S4); be32(blob + 0x5e, 1);
    be32(blob + 0x62, S5); be32(blob + 0x66, 48);
    be32(blob + S0, 0); be32(blob + S0 + 4, 18);
    be32(blob + S0 + 8, 18); be32(blob + S0 + 12, 12);
    le16(blob + S1, 0x1001); le16(blob + S1 + 6, 5);
    le16(blob + S1 + 8, 2);
    blob[S1 + 10] = 0x12; blob[S1 + 11] = 0x34;
    blob[S1 + 12] = 0x50; blob[S1 + 14] = 0x67;
    le16(blob + S1 + 18, 0x1001); le16(blob + S1 + 24, 4);
    le16(blob + S1 + 26, 1);
    be32(blob + S2, 8); be32(blob + S2 + 4, UINT32_MAX);
    blob[S2 + 8] = 9; blob[S2 + 9] = 8; blob[S2 + 10] = 7;
    blob[S5] = 1; be32(blob + S5 + 1, 5);
    be32(blob + S5 + 5 + 26, 43);
    be32(blob + S5 + 5 + 30, 43);
    be32(blob + S5 + 5 + 34, 43);
    blob[S5 + 43] = '{'; blob[S5 + 44] = 0x81;
    blob[S5 + 45] = 0x40; blob[S5 + 46] = 'A';
    blob[S5 + 47] = '@';
    assert(npe_parse(&file, blob, END, error, sizeof(error)) == 0);
    assert(file.indexed_bitmap_count == 2);
    assert(file.animation_group_count == 0);
    assert(npe_animation_frames(&file, 0) == 0);
    assert(npe_animation_frame(&file, 0, 0, &image) == -1);
    assert(npe_indexed_bitmap(&file, 0, &image) == 0);
    assert(image.width == 5 && image.height == 2 && image.row_bytes == 4);
    assert(npe_indexed_pixel(&image, 0, 0) == 1);
    assert(npe_indexed_pixel(&image, 4, 0) == 5);
    assert(npe_indexed_pixel(&image, 0, 1) == 6);
    assert(npe_indexed_pixel(&image, 5, 0) == 0);
    assert(file.offset_table_records[2] == 1);
    assert(npe_record(&file, 2, 0, &record, &length) == 0);
    assert(length == 3 && record[0] == 9 && record[2] == 7);
    assert(npe_npc_action(&file, 1, 0, &action) == 0);
    assert(action.case_count == 1 && action.dialogue_offset[0] == 43);
    assert(npe_dialogue_page(&file, action.dialogue_offset[0], &page) == 0);
    assert(page.size == 4 && page.terminator == '@');
    assert(page.bytes[1] == 0x81 && page.bytes[2] == 0x40);
    assert(npe_npc_action(&file, 2, 0, &action) == -1);
    assert(npe_story_record(&file, 0, &story) == -1);
    npe_free(&file);
    puts("NPE indexed resource test passed");
    return 0;
}
