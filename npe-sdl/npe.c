#include "npe.h"
#include "resident_heap.inc"
#include "resident_heap_owner.inc"
#include "battle_bitmap_service.inc"
#include "story_text_service.inc"
#include "npc_event_service.inc"
#include "text_substring_service.inc"
#include "indexed_bitmap_service.inc"
#include "enemy_record_service.inc"
#include "party_battle_descriptors.inc"
#include "battle_sound.inc"
#include "map_sound.inc"
#include "background_sound.inc"
#include "music_controller.inc"
#include "map_tile_records.inc"
#include "map_viewport_cache.inc"
#include "map_grid_stream.inc"
#include "map_tile_update.inc"
#include "map_object_clear.inc"
#include "animation_stream.inc"
#include "guest_bitmap_draw.inc"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "rpg_runtime.inc"

enum { TABLE_OFFSET = 0x3a, TABLE_END = TABLE_OFFSET + 100 * 8 };

static uint16_t be16(const uint8_t *p) {
    return (uint16_t)((unsigned)p[0] << 8 | p[1]);
}
static int16_t be16_signed(const uint8_t *p) {
    unsigned value = be16(p);
    return (int16_t)(value <= 0x7fffu ? (int)value : (int)value - 0x10000);
}
static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}
static uint16_t le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (unsigned)p[1] << 8);
}
static uint32_t le24(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16;
}
static int fail(char *error, size_t cap, const char *message) {
    if (cap) snprintf(error, cap, "%s", message);
    return -1;
}
static unsigned bitmap_count(const uint8_t *p, uint32_t size) {
    size_t position = 0;
    unsigned count = 0;
    if (!size) return 0;
    while (position < size) {
        const uint8_t *h = p + position;
        unsigned x, y, width, height;
        size_t payload;
        if (size - position < 10 || le16(h) != 0x1001) return 0;
        x = le16(h + 2); y = le16(h + 4);
        width = le16(h + 6); height = le16(h + 8);
        if (!width || !height || x + width > NPE_WIDTH ||
            y + height > NPE_HEIGHT) return 0;
        payload = ((size_t)width * height + 1) / 2;
        if (payload > size - position - 10) return 0;
        position += 10 + payload;
        count++;
    }
    return count;
}
static unsigned indexed_bitmap_count(const NpeFile *file) {
    const NpeSection *table, *images;
    unsigned i, count;
    uint32_t end = 0;
    if (file->section_count < 2) return 0;
    table = &file->sections[0]; images = &file->sections[1];
    if (!table->size || table->size % 8) return 0;
    count = table->size / 8;
    for (i = 0; i < count; ++i) {
        const uint8_t *record = file->bytes + table->offset + i * 8;
        uint32_t offset = be32(record), size = be32(record + 4);
        const uint8_t *header;
        uint32_t width, height, row_bytes;
        if (offset != end || size < 10 || offset > images->size ||
            size > images->size - offset) return 0;
        header = file->bytes + images->offset + offset;
        width = le16(header + 6); height = le16(header + 8);
        row_bytes = ((width + 3u) / 4u) * 2u;
        if (le16(header) != 0x1001 || !width || !height ||
            width > NPE_WIDTH || height > NPE_HEIGHT ||
            10u + row_bytes * height != size) return 0;
        end += size;
    }
    return end == images->size ? count : 0;
}
static unsigned battle_bitmap_count(const NpeFile *file) {
    const NpeSection *section;
    const uint8_t *data;
    uint32_t table_size, start, end;
    unsigned count, i;
    if (file->section_count <= 9) return 0;
    section = &file->sections[9];
    if (section->size < 13) return 0;
    data = file->bytes + section->offset;
    table_size = le24(data);
    if (!table_size || table_size % 3 || table_size > section->size)
        return 0;
    count = table_size / 3;
    for (i = 0; i < count; ++i) {
        const uint8_t *header;
        uint32_t width, height, row_bytes, payload;
        start = le24(data + i * 3);
        end = i + 1 < count ? le24(data + (i + 1) * 3) : section->size;
        if (start < table_size || start >= end || end > section->size ||
            end - start < 10) return 0;
        header = data + start;
        if (le16(header) != 0x1001) return 0;
        width = le16(header + 6); height = le16(header + 8);
        if (!width || !height || width > NPE_WIDTH || height > NPE_HEIGHT ||
            le16(header + 2) + width > NPE_WIDTH ||
            le16(header + 4) + height > NPE_HEIGHT) return 0;
        row_bytes = ((width + 3) / 4) * 2;
        payload = row_bytes * height;
        /* The final Tianjiang record has a one-byte section terminator. */
        if (payload > end - start - 10 ||
            (i + 1 < count && payload != end - start - 10) ||
            (i + 1 == count && end - start - 10 - payload > 1)) return 0;
    }
    return count;
}
static unsigned animation_group_count(const NpeFile *file,
                                      unsigned table_section, unsigned image_section,
                                      unsigned *frame_count) {
    const NpeSection *table, *images;
    const uint8_t *data;
    uint32_t records_end, image_end = 0;
    unsigned groups, frames = 0, i, j;
    if (file->section_count <= table_section || file->section_count <= image_section) return 0;
    table = &file->sections[table_section]; images = &file->sections[image_section];
    data = file->bytes + table->offset;
    if (table->size < 5) return 0;
    records_end = be32(data);
    if (records_end < 5 || records_end > table->size ||
        records_end % 5) return 0;
    groups = records_end / 5;
    for (i = 0; i < groups; ++i) {
        uint32_t offset = be32(data + i * 5);
        unsigned count = data[i * 5 + 4];
        if (offset != records_end || (size_t)count * 8 >
            table->size - records_end) return 0;
        for (j = 0; j < count; ++j) {
            const uint8_t *entry = data + records_end + j * 8;
            uint32_t position = be32(entry), size = be32(entry + 4);
            const uint8_t *header;
            unsigned x, y, width, height, row_bytes;
            if (position != image_end || size < 10 ||
                position > images->size || size > images->size - position)
                return 0;
            header = file->bytes + images->offset + position;
            x = le16(header + 2); y = le16(header + 4);
            width = le16(header + 6); height = le16(header + 8);
            row_bytes = ((width + 3u) / 4u) * 2u;
            if (le16(header) != 0x1001 || !width || !height ||
                x + width > NPE_WIDTH || y + height > NPE_HEIGHT ||
                10u + (size_t)row_bytes * height != size) return 0;
            image_end += size;
        }
        records_end += count * 8;
        frames += count;
    }
    if (records_end != table->size || image_end != images->size) return 0;
    *frame_count = frames;
    return groups;
}
static unsigned offset_table_records(const NpeFile *file, unsigned section) {
    const NpeSection *entry = &file->sections[section];
    const uint8_t *data = file->bytes + entry->offset;
    uint32_t first, previous;
    unsigned entries, i;
    if (entry->size < 8) return 0;
    first = be32(data);
    if (first < 8 || first > entry->size || first % 4) return 0;
    entries = first / 4;
    previous = first;
    for (i = 1; i < entries; ++i) {
        uint32_t current = be32(data + i * 4);
        if (i == entries - 1 && current == UINT32_MAX) return entries - 1;
        if (current <= previous || current >= entry->size) return 0;
        previous = current;
    }
    return entries;
}
int npe_parse(NpeFile *out, uint8_t *bytes, size_t size,
              char *error, size_t cap) {
    unsigned i;
    uint64_t expected = TABLE_END;
    int unused = 0;
    if (!out || !bytes) return fail(error, cap, "null input");
    memset(out, 0, sizeof(*out));
    if (size < TABLE_END) return fail(error, cap, "file shorter than NPE header");
    if (memcmp(bytes, "NP560 RPG2.1", 12) != 0)
        return fail(error, cap, "unsupported NPE signature");
    memcpy(out->title, bytes + 0x0c, 12);
    out->title[12] = 0;
    for (i = 0; i < 9; ++i) out->vectors[i] = be16(bytes + 0x18 + i * 2);
    for (i = 0; i < NPE_MAX_SECTIONS; ++i) {
        const uint8_t *entry = bytes + TABLE_OFFSET + i * 8;
        uint32_t offset = be32(entry), length = be32(entry + 4);
        if (!offset) {
            /* Real files retain nonzero size words in unused slots. */
            unused = 1;
            continue;
        }
        if (unused) return fail(error, cap, "active section follows empty slot");
        if (!length || (uint64_t)offset != expected ||
            offset > size || length > size - offset)
            return fail(error, cap, "section range or contiguity invalid");
        out->sections[i].offset = offset;
        out->sections[i].size = length;
        out->sections[i].bitmap_count = bitmap_count(bytes + offset, length);
        out->section_count++;
        expected += length;
    }
    if (!out->section_count || expected != size)
        return fail(error, cap, "sections do not cover file");
    if (expected > SIZE_MAX) return fail(error, cap, "file too large");
    if (be16(bytes + 0x38) !=
        (uint64_t)out->sections[0].offset + out->sections[0].size)
        return fail(error, cap, "first section end marker mismatch");
    for (i = 0; i < 9; ++i) {
        if (out->vectors[i] && out->vectors[i] >= be16(bytes + 0x38))
            return fail(error, cap, "header vector outside first section");
    }
    out->bytes = bytes;
    out->size = size;
    out->indexed_bitmap_count = indexed_bitmap_count(out);
    out->battle_bitmap_count = battle_bitmap_count(out);
    out->animation_group_count = animation_group_count(
        out, 6, 7, &out->animation_frame_count);
    out->scene_animation_group_count = animation_group_count(
        out, 13, 14, &out->scene_animation_frame_count);
    for (i = 2; i < out->section_count; ++i)
        out->offset_table_records[i] = offset_table_records(out, i);
    return 0;
}
void npe_free(NpeFile *file) {
    if (file) { npe_rpg_runtime_close(file); free(file->bytes); memset(file, 0, sizeof(*file)); }
}
int npe_bitmap(const NpeFile *file, unsigned section, unsigned index,
               uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    const uint8_t *p;
    size_t position = 0, n;
    unsigned width, height, x, y, i;
    if (!file || !pixels || section >= file->section_count ||
        index >= file->sections[section].bitmap_count) return -1;
    p = file->bytes + file->sections[section].offset;
    for (i = 0; i < index; ++i) {
        const uint8_t *h = p + position;
        position += 10 + ((size_t)le16(h + 6) * le16(h + 8) + 1) / 2;
    }
    x = le16(p + position + 2); y = le16(p + position + 4);
    width = le16(p + position + 6); height = le16(p + position + 8);
    p += position + 10;
    memset(pixels, 0, NPE_WIDTH * NPE_HEIGHT);
    for (n = 0; n < (size_t)width * height; ++n) {
        unsigned px = (unsigned)(n % width) + x;
        unsigned py = (unsigned)(n / width) + y;
        pixels[py * NPE_WIDTH + px] =
            n & 1 ? p[n / 2] & 15 : p[n / 2] >> 4;
    }
    return 0;
}
int npe_indexed_bitmap(const NpeFile *file, unsigned index,
                       NpeIndexedBitmap *bitmap) {
    const uint8_t *record, *header;
    uint32_t offset;
    if (!file || !bitmap || index >= file->indexed_bitmap_count) return -1;
    record = file->bytes + file->sections[0].offset + (size_t)index * 8;
    offset = be32(record);
    header = file->bytes + file->sections[1].offset + offset;
    bitmap->x = le16(header + 2);
    bitmap->y = le16(header + 4);
    bitmap->width = le16(header + 6);
    bitmap->height = le16(header + 8);
    bitmap->row_bytes = ((bitmap->width + 3u) / 4u) * 2u;
    bitmap->pixels = header + 10;
    bitmap->resource_index=index;bitmap->indexed_resource=1;
    return 0;
}
int npe_render_indexed_resource(const NpeFile *file,uint16_t index,int16_t left,int16_t top,
                                 uint8_t kind,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!file||!pixels||kind>2)return -1;
    if(file->rpg_runtime){
        int result=npe_rpg_runtime_draw_indexed(file,(int16_t)index,left,top,kind,pixels);
        return result==-2?-1:0;
    }
    NpeIndexedBitmap bitmap;
    if(npe_indexed_bitmap(file,index,&bitmap))return -1;
    /* Metadata-only views are also usable without a live RPG context. */
    for(unsigned y=0;y<bitmap.height;++y)for(unsigned x=0;x<bitmap.width;++x){
        int px=left+(int)x,py=top+(int)y;
        if(px<0||py<0||px>=NPE_WIDTH||py>=NPE_HEIGHT)continue;
        uint8_t value=npe_indexed_pixel(&bitmap,x,y),*dst=&pixels[(unsigned)py*NPE_WIDTH+(unsigned)px];
        if(!kind || (kind==1?!*dst:value!=0))*dst=value;
    }
    return 0;
}
int npe_battle_bitmap(const NpeFile *file, unsigned index,
                      NpeIndexedBitmap *bitmap) {
    const uint8_t *data, *header;
    if(bitmap)bitmap->indexed_resource=0;
    if (!file || !bitmap || !file->battle_bitmap_count) return -1;
    if(index<29){
        if(index>=file->battle_bitmap_count)return -1;
        data = file->bytes + file->sections[9].offset;
        header = data + le24(data + index * 3);
    }else{
        /*45:8FD6..9102 uses a separate file branch above one-based29.
          The6-byte entries are read as3+3 bytes into overlapping word
          locals FFEC/FFEE. Only their first BE16 words are consumed:
          signed offset, unsigned read length. This is not the8-byte
          global bitmap lookup and must not use vectors[6]. */
        uint16_t displacement=(uint16_t)(6u*((uint16_t)(index+1)-29u));
        uint32_t directory=file->sections[0].offset+displacement;
        if(directory>file->size || file->size-directory<6)return -1;
        data=file->bytes+directory;
        int16_t relative=be16_signed(data);
        unsigned length=be16(data+3);
        uint32_t position=file->sections[1].offset+(uint32_t)(int32_t)relative;
        if(!length){
            /* A zero-byte payload produces no drawing, while the caller
               continues its LCD commits (original512/1024 diagnostics). */
            memset(bitmap,0,sizeof(*bitmap));return 0;
        }
        if(position>file->size || file->size-position<length)return -1;
        header=file->bytes+position;
        if(length<10){
            /* Original short-header34 reads only the two signature bytes;
               the empty geometry still permits subsequent LCD commits.
               Decode the supplied prefix without borrowing file bytes
               that the original read did not supply. */
            uint8_t prefix[10]={0};memcpy(prefix,header,length);
            if(le16(prefix)==0x1001 &&
               (!le16(prefix+6) || !le16(prefix+8))){
                memset(bitmap,0,sizeof(*bitmap));return 0;
            }
            return -1;
        }
        unsigned width=le16(header+6),height=le16(header+8);
        size_t payload=(size_t)((width+3u)/4u)*2u*height;
        if(le16(header)!=0x1001 || payload>length-10)return -1;
    }
    bitmap->x = le16(header + 2);
    bitmap->y = le16(header + 4);
    bitmap->width = le16(header + 6);
    bitmap->height = le16(header + 8);
    bitmap->row_bytes = ((bitmap->width + 3u) / 4u) * 2u;
    bitmap->pixels = header + 10;
    return 0;
}
uint8_t npe_indexed_pixel(const NpeIndexedBitmap *bitmap,
                          unsigned x, unsigned y) {
    uint8_t byte;
    if (!bitmap || !bitmap->pixels || x >= bitmap->width || y >= bitmap->height)
        return 0;
    byte = bitmap->pixels[(size_t)y * bitmap->row_bytes + x / 2u];
    return x & 1u ? byte & 15u : byte >> 4;
}
unsigned npe_animation_frames(const NpeFile *file, unsigned group) {
    const uint8_t *table;
    if (!file || group >= file->animation_group_count) return 0;
    table = file->bytes + file->sections[6].offset;
    return table[group * 5 + 4];
}
static int animation_frame(const NpeFile *file, unsigned table_section,
                            unsigned image_section, unsigned frames,
                            unsigned group, unsigned frame, NpeIndexedBitmap *bitmap) {
    const uint8_t *table, *entry, *header;
    if(bitmap)bitmap->indexed_resource=0;
    uint32_t offset;
    if (!file || !bitmap || frame >= frames) return -1;
    table = file->bytes + file->sections[table_section].offset;
    entry = table + be32(table + group * 5) + frame * 8;
    offset = be32(entry);
    header = file->bytes + file->sections[image_section].offset + offset;
    bitmap->x = le16(header + 2);
    bitmap->y = le16(header + 4);
    bitmap->width = le16(header + 6);
    bitmap->height = le16(header + 8);
    bitmap->row_bytes = ((bitmap->width + 3u) / 4u) * 2u;
    bitmap->pixels = header + 10;
    return 0;
}
int npe_animation_frame(const NpeFile *file, unsigned group, unsigned frame,
                        NpeIndexedBitmap *bitmap) {
    return animation_frame(file, 6, 7, npe_animation_frames(file, group), group, frame, bitmap);
}
unsigned npe_scene_animation_frames(const NpeFile *file, unsigned group) {
    if (!file || group >= file->scene_animation_group_count) return 0;
    return file->bytes[file->sections[13].offset + group * 5 + 4];
}
int npe_scene_animation_frame(const NpeFile *file, unsigned group, unsigned frame,
                              NpeIndexedBitmap *bitmap) {
    return animation_frame(file, 13, 14, npe_scene_animation_frames(file, group), group, frame, bitmap);
}
int npe_render_scene_animation_frame(const NpeFile *file, unsigned group,
                                     unsigned frame, unsigned x, unsigned y,
                                     unsigned draw_mode, uint8_t *pixels) {
    NpeIndexedBitmap bitmap;
    unsigned ix, iy;
    if(file && file->rpg_runtime){
        if(group>32766 || draw_mode>2 || !pixels ||
           file->rpg_runtime->scene.group!=group+1 ||
           file->rpg_runtime->scene.position!=(uint8_t)frame)return -1;
        return npe_rpg_runtime_scene_frame(file,(int16_t)x,(int16_t)y,
                                           (uint8_t)draw_mode,pixels);
    }
    if (!pixels || draw_mode > 2 ||
        npe_scene_animation_frame(file, group, frame, &bitmap)) return -1;
    /* Source2A:7C34/7ECE/8232 uses width/height from the image header,
       and the explicit X/Y arguments, not the header's X/Y fields. */
    if (x >= NPE_WIDTH || y >= NPE_HEIGHT) return 0;
    for (iy = 0; iy < bitmap.height && iy < NPE_HEIGHT - y; ++iy)
        for (ix = 0; ix < bitmap.width && ix < NPE_WIDTH - x; ++ix) {
            uint8_t value = npe_indexed_pixel(&bitmap, ix, iy);
            uint8_t *dest = pixels + (y + iy) * NPE_WIDTH + x + ix;
            if (!draw_mode || (draw_mode == 1 ? !*dest : value != 0))
                *dest = value;
        }
    return 0;
}
int npe_record(const NpeFile *file, unsigned section, unsigned index,
               const uint8_t **data, size_t *size) {
    const NpeSection *entry;
    const uint8_t *table;
    uint32_t start, end;
    if (!file || !data || !size || section >= file->section_count ||
        index >= file->offset_table_records[section]) return -1;
    entry = &file->sections[section];
    table = file->bytes + entry->offset;
    start = be32(table + index * 4);
    end = index + 1 == file->offset_table_records[section] ?
          entry->size : be32(table + (index + 1) * 4);
    if (end == UINT32_MAX) end = entry->size;
    *data = table + start;
    *size = end - start;
    return 0;
}
int npe_audio_header(const uint8_t *bytes,size_t size,NpeAudioHeader *header) {
    /* Original PRG data tables at32:B788/B7A8, verified on the data bus. */
    static const uint16_t source_rates[16]={8192,8192,8192,8192,12227,11703,
        10240,9102,8192,7447,6827,6302,5851,5461,5120,8192};
    static const uint16_t bitrates[25]={0,800,1200,2000,2400,2800,0,0,0,
        3600,4800,6000,7200,8400,9600,10800,12000,13200,14400,
        6500,9100,11800,14500,17200,19800};
    NpeAudioHeader next={0};
    if(!bytes||!header||size<6)return -1;
    next.type_revision=(uint16_t)((unsigned)(bytes[0]>>4)<<8|(bytes[0]&15));
    next.format=bytes[1];next.count=be16(bytes+2);
    if(next.format<9){
        int flag=bytes[2]>>5,nibble=bytes[5]&15;
        if(flag&4)flag-=8;
        if(nibble&8)nibble-=16;
        next.count&=0x1fff;next.control36=(uint16_t)flag;
        /* SROSB keeps the sign bit of the preceding count flag in its
           low bit; this differs from an isolated three-bit extraction. */
        next.control32=(bytes[4]>>5)|(flag<0);next.control34=(bytes[4]>>2)&7;
        next.control2e=(be16(bytes+4)>>4)&63;
        next.source_rate=source_rates[nibble+8];next.output_rate=8000;
    }else{
        uint16_t lookahead=le16(bytes+4);
        next.source_rate=next.output_rate=next.format<19?8000:11025;
        next.control36=lookahead>>13;next.control2e=(lookahead>>4)&63;
        /* The original compares the WHOLE2828 word to1; first byte01,
           not first byte10, selects this extra count mask. */
        if(next.type_revision==1)next.count&=0x1fff;
    }
    next.bitrate=bitrates[next.format<=24?next.format:0];
    *header=next;
    return !next.bitrate;
}

int npe_audio_resource(const NpeFile *file, unsigned section,
                       NpeAudioResource *resource) {
    if(!file || section<16 || section>37 || section>=file->section_count ||
       file->sections[section].size>INT32_MAX)return -1;
    return npe_audio_resource_range(file,file->sections[section].offset,
        (int32_t)file->sections[section].size,resource);
}

int npe_audio_resource_range(const NpeFile *file,uint32_t offset,int32_t length,NpeAudioResource *resource){
    const uint8_t *bytes;
    size_t expected;
    uint16_t count;
    NpeAudioHeader header;
    if(!file || !file->bytes || !resource || length<4 || offset>file->size ||
       (size_t)length>file->size-offset)return -1;
    bytes=file->bytes+offset;
    if (bytes[0] != 0x20 || bytes[1] != 0x0d) return -1;
    if(npe_audio_header(bytes,(size_t)length,&header))return -1;
    count = header.count;
    if (!count) return -1;
    expected = 4u + (size_t)count * 21u;
    expected += expected & 1u;
    if ((size_t)length != expected) return -1;
    resource->bytes = bytes;
    resource->size = (size_t)length;
    resource->header_count = count;
    resource->header = header;
    return 0;
}
/* 0C:BDBE reads the grid and all map tables sequentially from the file.
   The following directory entry is not a stream end. These sizes are the
   existing file record layouts; group0 contains count-prefixed tile pairs. */
static int map_stream_layout(const uint8_t *tail, size_t available,
                             size_t offsets[17], uint8_t counts[17],
                             size_t *size) {
    static const uint8_t record_sizes[16] = {
        0, 13, 10, 7, 8, 10, 7, 13, 11, 9, 10, 7, 8, 8, 11, 7
    };
    /* Original loop bounds in0C:D24E..F214. The two-byte directory
       offsets are discarded, and excess counts do not skip extra records. */
    static const uint8_t record_limits[16] = {
        3, 5, 5, 1, 5, 2, 3, 5, 4, 4, 2, 15, 5, 5, 10, 2
    };
    size_t used = 703;
    if (available < used) return -1;
    for (unsigned i = 0; i < 16; ++i) {
        counts[i] = tail[654 + i * 3];
        if (counts[i] > record_limits[i]) counts[i] = record_limits[i];
    }
    counts[16] = tail[702];
    offsets[0] = 0;
    for (unsigned i = 0; i < counts[0]; ++i) {
        size_t bytes;
        if (available - used < 3) return -1;
        bytes = 3u + (size_t)tail[used + 2] * 4u;
        if (bytes > available - used) return -1;
        used += bytes;
    }
    for (unsigned i = 1; i < 16; ++i) {
        offsets[i] = used - 703;
        size_t bytes = (size_t)counts[i] * record_sizes[i];
        if (bytes > available - used) return -1;
        used += bytes;
    }
    offsets[16] = used - 703;
    *size = used;
    return 0;
}
int npe_map(const NpeFile *file, unsigned index, NpeMap *map) {
    const uint8_t *record;
    size_t nominal_size, size, grid_bytes, tail_bytes, offsets[17];
    uint8_t counts[17];
    if (!map || npe_record(file, 8, index, &record, &nominal_size))
        return -1;
    size = file->size - (size_t)(record - file->bytes);
    if (size < 17) return -1;
    grid_bytes = (size_t)record[2] * record[3] * 2u;
    /* Original0C:BDBE accepts zero dimensions and still loads the tail. */
    if (grid_bytes > size - 17) return -1;
    map->number = index + 1;
    map->header_word = be16(record);
    map->width = record[2]; map->height = record[3];
    memcpy(map->name, record + 4, sizeof(map->name));
    map->tile_words = record + 17;
    map->tail = map->tile_words + grid_bytes;
    map->tail_size = size - 17 - grid_bytes;
    if (!map_stream_layout(map->tail, map->tail_size, offsets, counts, &tail_bytes))
        map->tail_size = tail_bytes;
    map->snapshot_tiles=NULL;map->snapshot_count=0;
    map->override_indices = map->override_tiles = NULL;
    map->override_count = 0;
    map->resident_file = NULL;
    return 0;
}
uint16_t npe_map_tile(const NpeMap *map, unsigned x, unsigned y) {
    if (!map || !map->tile_words || x >= map->width || y >= map->height)
        return UINT16_MAX;
    if(map->snapshot_tiles){
        size_t index=(size_t)y*map->width+x;
        return index<map->snapshot_count?map->snapshot_tiles[index]:UINT16_MAX;
    }
    if (map->override_count > NPE_TILE_OVERRIDES || (map->override_count &&
        (!map->override_indices || !map->override_tiles))) return UINT16_MAX;
    for (unsigned i = 0; i < map->override_count; ++i)
        if (map->override_indices[i] == y * map->width + x)
            return map->override_tiles[i];
    return be16(map->tile_words + ((size_t)y * map->width + x) * 2u);
}
int npe_tile_base_passable(uint16_t tile) {
    if (tile <= 0x000b || tile == 0x017f || tile == 0x0182 ||
        tile == 0x0183 || tile == 0x0288) return 1;
    if ((tile >= 0x026b && tile <= 0x0275) ||
        (tile >= 0x0210 && tile <= 0x0215) ||
        (tile >= 0x028e && tile <= 0x0290) ||
        (tile >= 0x0405 && tile <= 0x0416) ||
        (tile >= 0x0426 && tile <= 0x0446) ||
        (tile >= 0x03b0 && tile <= 0x03b4)) return 1;
    return 0;
}
int npe_map_object(const NpeMap *map, unsigned index, const uint8_t **raw) {
    if (!map || !raw || index >= 8 || map->tail_size < 80) return -1;
    *raw = map->tail + index * 10;
    return 0;
}
int npe_map_object_info(const NpeMap *map, unsigned index,
                        NpeMapObject *object) {
    const uint8_t *raw;
    if (!object || npe_map_object(map, index, &raw)) return -1;
    object->id = be16(raw);
    object->item_index = be16(raw + 2);
    object->x = raw[6]; object->y = raw[7];
    object->raw = raw;
    return 0;
}
static int map_indexed_tail(const NpeMap *map, size_t offsets[17],
                            uint8_t counts[17]) {
    size_t size;
    return !map || !map->tail ? -1 :
        map_stream_layout(map->tail, map->tail_size, offsets, counts, &size);
}
int npe_map_tile_changes(const NpeMap *map, NpeMapTileChange changes[3],
                          unsigned *count) {
    size_t offsets[17], used = 0, size;
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !changes || !count) return -1;
    *count = 0;
    memset(changes, 0, 3 * sizeof(*changes));
    if (map->tail_size < 703) return 0;
    if (map_indexed_tail(map, offsets, counts) || counts[0] > 3) return -1;
    raw = map->tail + 703 + offsets[0];
    size = offsets[1] - offsets[0];
    /* Source 29:D24E..D456 reads threshold, byte count, then index/tile
       word pairs into the 64-byte records at 52:724E. */
    for (unsigned i = 0; i < counts[0]; ++i) {
        unsigned n;
        if (size - used < 3) return -1;
        changes[i].progress = be16(raw + used);
        n = raw[used + 2]; used += 3;
        if (n > 15 || (size_t)n * 4 > size - used) return -1;
        changes[i].count = (uint8_t)n;
        for (unsigned j = 0; j < n; ++j, used += 4) {
            unsigned index = be16(raw + used);
            /* The original interprets this as a signed address offset;
               it does not validate against the logical grid dimensions. */
            changes[i].indices[j] = (uint16_t)index;
            changes[i].tiles[j] = be16(raw + used + 2);
        }
    }
    if (used != size) return -1;
    *count = counts[0];
    return 0;
}
int npe_map_group_count(const NpeMap *map, unsigned group, unsigned *count) {
    size_t offsets[17];
    uint8_t counts[17];
    if (!count || group >= 17 || map_indexed_tail(map, offsets, counts))
        return -1;
    *count = counts[group];
    return 0;
}
int npe_map_quest_item(const NpeMap *map, unsigned slot,
                        NpeMapQuestItem *event) {
    size_t offsets[17];uint8_t counts[17];const uint8_t *raw;
    if(!map || !event || slot>=5)return -1;
    if(map->tail_size<703)return 0;
    if(map_indexed_tail(map,offsets,counts) || counts[7]>5 ||
       offsets[8]-offsets[7]!=(size_t)counts[7]*13)return -1;
    if(slot>=counts[7])return 0;
    raw=map->tail+703+offsets[7]+slot*13;
    /* Source29:E148..E37C discards the record number and expands the
       remaining bytes into the twelve-byte service row at 75DE. */
    event->record_id=be16(raw);event->min_progress=be16(raw+2);
    event->item_id=be16(raw+4);event->next_mode=raw[6];event->next_id=be16(raw+7);
    event->x=raw[9];event->y=raw[10];event->npc_id=be16(raw+11);event->raw=raw;
    return 1;
}
int npe_map_chase(const NpeMap *map, unsigned slot, NpeMapChase *chase) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !chase || slot >= 2) return -1;
    if (map->tail_size < 703) return 0;
    if (map_indexed_tail(map, offsets, counts) || counts[10] > 2 ||
        offsets[11] - offsets[10] != (size_t)counts[10] * 10) return -1;
    if (slot >= counts[10]) return 0;
    raw = map->tail + 703 + offsets[10] + slot * 10;
    /* Source 29:E808..EA34 expands these ten bytes into a 14-byte
       record at 52:783A, with word X/Y and a zero cooldown byte. */
    chase->graphic = be16(raw);
    chase->min_progress = be16(raw + 2);
    chase->max_progress = be16(raw + 4);
    chase->enemy_id = be16(raw + 6);
    chase->x = raw[8]; chase->y = raw[9]; chase->raw = raw;
    return 1;
}
int npe_map_choice_event(const NpeMap *map, unsigned event_id,
                         NpeMapChoiceEvent *choice) {
    const uint8_t *raw;
    size_t offsets[17];
    uint8_t counts[17];
    if (!map || !choice || !event_id || event_id > 5 ||
        map_indexed_tail(map, offsets, counts)) return -1;
    if (counts[4] > 5 ||
        offsets[5] - offsets[4] != (size_t)counts[4] * 8) return -1;
    if (event_id > counts[4]) return 0;
    raw = map->tail + 703 + offsets[4] + (event_id - 1) * 8;
    /* Source 45:D3B2..D4E0 indexes the RAM table by (event_id - 1),
       then reads two modes and two event numbers. Map group 4 carries
       the same values as {id, mode0, event0, mode1, event1}. */
    choice->id = be16(raw);
    choice->mode[0] = raw[2];
    choice->next_id[0] = be16(raw + 3);
    choice->mode[1] = raw[5];
    choice->next_id[1] = be16(raw + 6);
    choice->raw = raw;
    return 1;
}
int npe_map_money_event(const NpeMap *map, unsigned event_id,
                        NpeMapMoneyEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[12] > 5 ||
        offsets[13] - offsets[12] != (size_t)counts[12] * 8)
        return -1;
    if (!event_id || event_id > counts[12]) return 0;
    raw = map->tail + 703 + offsets[12] + (event_id - 1) * 8;
    event->id = be16(raw);
    event->add_money = raw[2];
    event->amount = be16(raw + 3);
    event->next_mode = raw[5];
    event->next_id = be16(raw + 6);
    event->raw = raw;
    return 1;
}
int npe_map_item_event(const NpeMap *map, unsigned event_id,
                       NpeMapItemEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[13] > 5 ||
        offsets[14] - offsets[13] != (size_t)counts[13] * 8)
        return -1;
    if (!event_id || event_id > counts[13]) return 0;
    raw = map->tail + 703 + offsets[13] + (event_id - 1) * 8;
    event->quantity = be16(raw);
    event->add_item = raw[2];
    event->item_index = be16(raw + 3);
    event->next_mode = raw[5];
    event->next_id = be16(raw + 6);
    event->raw = raw;
    return 1;
}
int npe_map_animation_event(const NpeMap *map, unsigned event_id,
                            NpeMapAnimationEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[1] > 5 ||
        offsets[2] - offsets[1] != (size_t)counts[1] * 13)
        return -1;
    if (!event_id || event_id > counts[1]) return 0;
    raw = map->tail + 703 + offsets[1] + (event_id - 1) * 13;
    /* Loader 29:D59A..D8A2 skips the record number, then reads the
       group and marker bytes, four individual coordinate bytes, and
       draw/erase bytes. Mode 15 passes group from RAM 7550 to 41:ACBA. */
    event->record_id = be16(raw);
    event->id = raw[2];
    event->marker = raw[3];
    event->x = raw[4];
    event->y = raw[5];
    event->x2 = raw[6];
    event->y2 = raw[7];
    event->draw_mode = raw[8];
    event->erase_mode = raw[9];
    event->next_mode = raw[10];
    event->next_id = be16(raw + 11);
    event->raw = raw;
    return 1;
}
int npe_map_npc_move_event(const NpeMap *map, unsigned event_id,
                           NpeMapNpcMoveEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[2] > 5 ||
        offsets[3] - offsets[2] != (size_t)counts[2] * 10)
        return -1;
    if (!event_id || event_id > counts[2]) return 0;
    raw = map->tail + 703 + offsets[2] + (event_id - 1) * 10;
    event->id = be16(raw);
    event->npc_id = be16(raw + 2);
    event->raw_word = be16(raw + 4);
    /* Source 29:D944..DA34 consumes a reserved byte at +4, then loads
       +5/+6 into the coordinate words 52:7658/765A. Source 29:DA36..DA82
       loads +7 into 52:7654; it is the next mode, not a coordinate. */
    event->x = raw[5];
    event->y = raw[6];
    event->next_mode = raw[7];
    event->next_id = be16(raw + 8);
    event->raw = raw;
    return 1;
}
int npe_map_teleport_event(const NpeMap *map, unsigned event_id,
                            NpeMapTeleportEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[5] > 2 ||
        offsets[6] - offsets[5] != (size_t)counts[5] * 10)
        return -1;
    if (!event_id || event_id > counts[5]) return 0;
    raw = map->tail + 703 + offsets[5] + (event_id - 1) * 10;
    event->id = be16(raw);
    event->destination_map = be16(raw + 2);
    event->x = raw[4];
    event->y = raw[5];
    event->marker = raw[6];
    event->next_mode = raw[7];
    event->next_id = be16(raw + 8);
    event->raw = raw;
    return 1;
}
int npe_map_script_walk_event(const NpeMap *map, unsigned event_id,
                               NpeMapScriptWalkEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[9] > 4 ||
        offsets[10] - offsets[9] != (size_t)counts[9] * 9)
        return -1;
    if (!event_id || event_id > counts[9]) return 0;
    raw = map->tail + 703 + offsets[9] + (event_id - 1) * 9;
    event->id = be16(raw);
    event->marker = raw[2];
    /*29:E652 discards byte+3; E678 reads only byte+4 into772E. */
    event->x = raw[4];
    event->y = raw[5];
    event->next_mode = raw[6];
    event->next_id = be16(raw + 7);
    event->raw = raw;
    return 1;
}
int npe_map_party_event(const NpeMap *map, unsigned event_id,
                        NpeMapPartyEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[6] > 3 ||
        offsets[7] - offsets[6] != (size_t)counts[6] * 7)
        return -1;
    if (!event_id || event_id > counts[6]) return 0;
    raw = map->tail + 703 + offsets[6] + (event_id - 1) * 7;
    event->id = be16(raw);
    event->add_member = raw[2];
    event->character_id = raw[3];
    event->next_mode = raw[4];
    event->next_id = be16(raw + 5);
    event->raw = raw;
    return 1;
}
int npe_map_party_adjust_event(const NpeMap *map, unsigned event_id,
                               NpeMapPartyAdjustEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[3] > 1 ||
        offsets[4] - offsets[3] != (size_t)counts[3] * 7)
        return -1;
    if (!event_id || event_id > counts[3]) return 0;
    raw = map->tail + 703 + offsets[3] + (event_id - 1) * 7;
    event->character_id = raw[3];
    event->next_mode = raw[4];
    event->next_id = be16(raw + 5);
    event->raw = raw;
    return 1;
}
int npe_map_skill_event(const NpeMap *map, unsigned event_id,
                        NpeMapSkillEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[15]>2 || offsets[16]-offsets[15]!=(size_t)counts[15]*7) return -1;
    if (!event_id || event_id > counts[15]) return 0;
    raw = map->tail + 703 + offsets[15] + (event_id - 1) * 7;
    event->id = be16(raw);
    event->character_id = raw[2];
    event->skill_id = raw[3];
    event->next_mode = raw[4];
    event->next_id = be16(raw + 5);
    event->raw = raw;
    return 1;
}
int npe_map_battle_event(const NpeMap *map, unsigned event_id,
                         NpeMapBattleEvent *event) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !event || map_indexed_tail(map, offsets, counts) ||
        counts[8] > 4 ||
        offsets[9] - offsets[8] != (size_t)counts[8] * 11)
        return -1;
    if (!event_id || event_id > counts[8]) return 0;
    raw = map->tail + 703 + offsets[8] + (event_id - 1) * 11;
    event->formation_key = raw[4];
    event->win_mode = raw[5];
    event->win_event_id = be16(raw + 6);
    event->lose_mode = raw[8];
    event->lose_event_id = be16(raw + 9);
    event->raw = raw;
    return 1;
}
int npe_quiz_category_range(const NpeFile *file, unsigned category,
                            unsigned *first, unsigned *count) {
    const NpeSection *section;
    const uint8_t *data;
    unsigned total, sum = 0, i;
    if (!file || !first || !count || category >= 10 ||
        file->section_count <= 15) return -1;
    section = &file->sections[15];
    if (section->size < 26) return -1;
    data = file->bytes + section->offset;
    total = be16(data);
    if (!total || 26u + 4u * total > section->size) return -1;
    for (i = 0; i < 10; ++i) {
        unsigned value = be16(data + 2 + i * 2);
        if (sum + value > total) return -1;
        if (i == category) { *first = sum; *count = value; }
        sum += value;
    }
    return sum == total ? 0 : -1;
}
int npe_quiz_question(const NpeFile *file, unsigned index,
                      NpeQuizQuestion *question) {
    const NpeSection *section;
    const uint8_t *data, *cursor, *end, *nul;
    uint32_t first, last;
    unsigned total, i;
    if (!file || !question || file->section_count <= 15) return -1;
    section = &file->sections[15];
    data = file->bytes + section->offset;
    if (section->size < 26) return -1;
    total = be16(data);
    if (!total || index >= total || 26u + 4u * total > section->size)
        return -1;
    first = be32(data + 22 + 4 * index);
    last = index + 1 < total ? be32(data + 22 + 4 * (index + 1)) :
                              section->size;
    if (first < 26u + 4u * total || first >= last ||
        last > section->size) return -1;
    cursor = data + first;
    end = data + last;
    for (i = 0; i < 5; ++i) {
        nul = memchr(cursor, 0, (size_t)(end - cursor));
        if (!nul || nul == cursor) return -1;
        question->text[i] = cursor;
        question->text_size[i] = (size_t)(nul - cursor);
        cursor = nul + 1;
    }
    if (end - cursor < 10 || cursor[0] > 3 ||
        cursor[1] > 24 || cursor[4] > 24) return -1;
    /* Source 2C:C5D4..C67C reads the answer and both followup
       mode/event pairs immediately after the five NUL strings. */
    question->correct_answer = cursor[0];
    question->input_type = cursor[7];
    question->followup_mode[0] = cursor[1];
    question->followup_event_id[0] = be16(cursor + 2);
    question->followup_mode[1] = cursor[4];
    question->followup_event_id[1] = be16(cursor + 5);
    return 0;
}
int npe_map_story_events(const NpeMap *map,NpeMapStoryEvent events[15],unsigned *count) {
    size_t offsets[17];uint8_t counts[17];
    if(!map || !events || !count || map_indexed_tail(map,offsets,counts))return -1;
    /*29:EAA2 reads group11 directly; scanning unrelated bytes for a
      plausible record sequence invents a table that the firmware lacks. */
    for(unsigned i=0;i<counts[11];++i){
        const uint8_t *raw=map->tail+703+offsets[11]+i*7;
        events[i]=(NpeMapStoryEvent){.id=be16(raw),.story_progress=be16(raw+2),
            .mode=raw[4],.next_id=be16(raw+5),.raw=raw};
    }
    *count=counts[11];
    return 0;
}

int npe_map_move_trigger(const NpeMap *map, unsigned x, unsigned y,
                         unsigned progress, NpeMapMoveTrigger *trigger) {
    const uint8_t *data;
    size_t group_offsets[17];
    uint8_t group_counts[17];
    unsigned target, count, i;
    if (!map || !trigger || x >= map->width || y >= map->height) return -1;
    if (map_indexed_tail(map, group_offsets, group_counts)) return 0;
    data = map->tail + 703;
    target = y * map->width + x;
    /* The 17 {BE16 offset,u8 count} entries at tail+652 index the variable
       tables at tail+703. Group 14 has 11-byte file records: a 2-byte
       prefix followed by the 9 fields loaded at source 29:EE94..F096 into
       52:7490 RAM slots. Group 15 begins immediately after group 14. */
    count = group_counts[14];
    for (i = 0; i < count; ++i) {
        const uint8_t *raw = data + group_offsets[14] + i * 11 + 2;
        /* Collision3A:062A..0748 checks position and progress only;
           a menu-mode trigger can legitimately carry event ID0. */
        if (be16(raw) != target ||
            be16(raw + 5) > be16(raw + 7) ||
            progress < be16(raw + 5) || progress > be16(raw + 7))
            continue;
        trigger->tile_index = be16(raw);
        trigger->mode = raw[2];
        trigger->event_id = be16(raw + 3);
        trigger->min_progress = be16(raw + 5);
        trigger->max_progress = be16(raw + 7);
        trigger->raw = raw;
        return 1;
    }
    return 0;
}
int npe_map_move_event(const NpeMap *map, unsigned event_id,
                       NpeMapMoveTrigger *trigger) {
    size_t offsets[17];
    uint8_t counts[17];
    const uint8_t *raw;
    if (!map || !trigger || map_indexed_tail(map, offsets, counts) ||
        counts[14] > 10 ||
        offsets[15] - offsets[14] != (size_t)counts[14] * 11)
        return -1;
    if (!event_id || event_id > counts[14]) return 0;
    raw = map->tail + 703 + offsets[14] + (event_id - 1) * 11 + 2;
    trigger->tile_index = be16(raw);
    trigger->mode = raw[2];
    trigger->event_id = be16(raw + 3);
    trigger->min_progress = be16(raw + 5);
    trigger->max_progress = be16(raw + 7);
    trigger->raw = raw;
    return 1;
}
static int decode_item(const uint8_t *raw, size_t size, NpeItem *item,
                         int native_name) {
    const uint8_t *end;
    if (!item || !raw || size < 41) return -1;
    end = memchr(raw, 0, 12);
    if (!end) end = raw + 12;
    if (end == raw && !native_name) return -1;
    item->name = raw;
    item->name_size = (size_t)(end - raw);
    end = memchr(raw + 40, 0, size - 40);
    if (!end) end = raw + size;
    item->description = raw + 40;
    item->description_size = (size_t)(end - item->description);
    item->raw = raw;
    item->effect_type = raw[12];
    item->flags = raw[35];
    item->group = raw[36];
    item->target_side = raw[37];
    item->hp_restore = be16(raw + 13);
    item->qi_restore = be16(raw + 15);
    item->price = be16(raw + 38);
    item->attack_bonus = be16_signed(raw + 17);
    item->defense_bonus = be16_signed(raw + 19);
    item->status_effects[0] = be16_signed(raw + 29);
    item->status_effects[1] = be16_signed(raw + 31);
    item->status_effects[2] = be16_signed(raw + 27);
    item->status_effects[3] = be16_signed(raw + 33);
    return 0;
}
int npe_item_record(const uint8_t *raw,size_t size,NpeItem *item) {
    return decode_item(raw,size,item,1);
}
int npe_item(const NpeFile *file, unsigned index, NpeItem *item) {
    const uint8_t *raw;size_t size;
    if (npe_record(file, 2, index, &raw, &size)) return -1;
    return decode_item(raw, size, item, 0);
}
int npe_shop_catalog(const NpeFile *file,unsigned context,NpeShopCatalog *shop) {
    const uint8_t *raw;size_t size;
    if(!shop || context<11 || context>0x7fff ||
       npe_record(file,11,context-11,&raw,&size) || size<2 || size-2<raw[1])return -1;
    shop->raw=raw;shop->id=raw[0];shop->count=raw[1];shop->items=raw+2;
    return 0;
}
int npe_shop_items(const NpeFile *file,unsigned context,uint8_t ids[254],unsigned *count) {
    NpeShopCatalog shop;uint8_t present[255]={0};unsigned n=0;
    if(!ids || !count || npe_shop_catalog(file,context,&shop))return -1;
    /*3F:B6BE..B702 writes presence bytes for1..254. B798..B842
      scans those bytes in ascending ID order, rather than catalog order. */
    for(unsigned i=0;i<shop.count;++i)
        if(shop.items[i] && shop.items[i]<255)present[shop.items[i]]=1;
    for(unsigned id=1;id<255;++id)if(present[id])ids[n++]=(uint8_t)id;
    *count=n;return 0;
}
int npe_native_item(const NpeFile *file, unsigned item_id, NpeItem *item) {
    size_t position;uint32_t offset,target;
    if (!file || !file->bytes || file->section_count <= 2 ||
        !item_id || item_id > 255) return -1;
    position = (size_t)file->sections[2].offset + (item_id - 1u) * 4u;
    if (position > file->size || file->size - position < 4) return -1;
    offset = be32(file->bytes + position);
    target = file->sections[2].offset + offset; /* Native two-word ADD/ADDC wraps. */
    /* The native seek rejects positions beyond EOF and leaves the
       current position after the four-byte offset read. The caller
       ignores that failure and reads the following bytes as an item. */
    if (target > file->size) position += 4;
    else position = target;
    /* Source3F:C370 seeks the selected offset, then reads fixed fields and
       NUL-terminated text. It does not require a nonempty item name. */
    return decode_item(file->bytes + position, file->size - position, item, 1);
}
int npe_battle_item(const NpeFile *file, unsigned item_id, NpeItem *item) {
    return npe_native_item(file,item_id,item);
}
static int item_bitmap(const NpeFile *file, unsigned index,
                         NpeIndexedBitmap *bitmap, int native_index) {
    if (!file || !bitmap || file->section_count <= 2) return -1;
    if(!native_index && index>=file->offset_table_records[2])return -1;
    /* Original2F:54BC and3F:0E1A use7AE4 + the item ID, without
       inferring icon runs or rejecting nonstandard image dimensions. */
    return npe_indexed_bitmap(file,npe_image_group_index(file,5,(uint16_t)(index+1u)),bitmap);
}
int npe_item_bitmap(const NpeFile *file, unsigned index, NpeIndexedBitmap *bitmap) {
    return item_bitmap(file, index, bitmap, 0);
}
int npe_native_item_bitmap(const NpeFile *file, unsigned item_id, NpeIndexedBitmap *bitmap) {
    if (!item_id || item_id > 255) return -1;
    return item_bitmap(file, item_id - 1, bitmap, 1);
}
int npe_native_item_cursor_bitmap(const NpeFile *file,uint16_t item_id,NpeIndexedBitmap *bitmap) {
    /*40:C418..C424/42:5CEC..5CF8 adds the full selected word to
      7AE4, the item icon run start minus one, with word wrap. */
    return item_bitmap(file,(unsigned)item_id-1u,bitmap,1);
}
int npe_battle_item_bitmap(const NpeFile *file, unsigned item_id, NpeIndexedBitmap *bitmap) {
    return npe_native_item_bitmap(file,item_id,bitmap);
}
static int decode_skill(const uint8_t *raw,size_t size,NpeSkill *skill,int native) {
    const uint8_t *end, *description_end;
    if(!raw || !skill || size < (native?41u:42u))return -1;
    end = memchr(raw, 0, 12);
    if ((!end || end == raw) && !native) return -1;
    if(!end)end=raw+12;
    description_end = memchr(raw + 41, 0, size - 41);
    if (!description_end && !native) return -1;
    if(!description_end)description_end=raw+size;
    skill->name = raw;
    skill->name_size = (size_t)(end - raw);
    skill->description = raw + 41;
    skill->description_size = (size_t)(description_end - skill->description);
    skill->raw = raw;
    skill->effect_type = raw[12];
    /* Source 49:E554..E5A2 compares 52:7381 (record +13) with the
       acting character's level at 52:69DE. */
    skill->required_level = raw[13];
    /* Runtime22:23C4 maps to source3F:23C4 and loads one-based
       section-3 records. For the observed
       Heart Calming spell, +19/+21 become 52:738A/738C and +39 becomes
       52:738E, the QI charge checked before casting. */
    /* Source 2D:3CF2..3D36 copies skill +15/+17/+19/+21 into
       the HP/QI/attack/defense effect words at 52:7926..792C. */
    skill->hp_effect = be16(raw + 15);
    skill->qi_effect = be16(raw + 17);
    skill->parameter_a = be16(raw + 19);
    skill->parameter_b = be16(raw + 21);
    skill->qi_cost = be16(raw + 39);
    return 0;
}
int npe_skill(const NpeFile *file,unsigned index,NpeSkill *skill) {
    const uint8_t *raw;size_t size;
    if(npe_record(file,3,index,&raw,&size))return -1;
    return decode_skill(raw,size,skill,0);
}
int npe_skill_record(const uint8_t *raw,size_t size,NpeSkill *skill) {
    return decode_skill(raw,size,skill,1);
}
int npe_native_skill(const NpeFile *file,unsigned id,NpeSkill *skill) {
    size_t position;uint32_t target;
    if(!file || !file->bytes || file->section_count<=3 || !id || id>255)return -1;
    position=(size_t)file->sections[3].offset+(id-1u)*4u;
    if(position>file->size || file->size-position<4)return -1;
    target=file->sections[3].offset+be32(file->bytes+position);
    /*3F:2436..24EA uses the ordinal offset without a record-count gate.
      Failed target seek leaves the stream after the offset word. */
    position=target>file->size?position+4:target;
    return decode_skill(file->bytes+position,file->size-position,skill,1);
}
int npe_skill_is_simple_enemy_attack(const NpeSkill *skill) {
    size_t offset;
    if (!skill || !skill->raw || !skill->hp_effect ||
        skill->raw[14] != 1 || skill->raw[37] || skill->raw[38])
        return 0;
    /* The source copies +15 to the battle HP effect. Restrict this
       shared direct-damage path to records with no other stat or status
       effect, and with the same target metadata as observed skill 36.
       Composite and multiple-target records need their own handlers. */
    for (offset = 17; offset < 37; ++offset)
        if (skill->raw[offset]) return 0;
    return 1;
}
int16_t npe_skill_status_effect(const NpeSkill *skill, unsigned status) {
    static const uint8_t offsets[4] = {31, 33, 29, 35};
    /* Source3F:2706..27C4 loads file29 ->739A,31 ->7396,
       33 ->7398,35 ->739C. Battle order is poison/sleep/seal/fourth. */
    if (!skill || !skill->raw || status >= 4) return 0;
    return be16_signed(skill->raw + offsets[status]);
}
int npe_skill_battle_effect(const NpeSkill *skill) {
    if (!skill || !skill->raw || skill->raw[37] > 1 || skill->raw[38] > 1)
        return 0;
    if (skill->raw[14] >= 16 && skill->raw[14] <= 19)
        return skill->raw[14];
    if (skill->raw[14] == 2) return 2;
    /* The original selector accepts other types. They retain casting,
       animation and QI payment, but skip the scalar/status commit. */
    if (skill->raw[14] != 1) return NPE_BATTLE_EFFECT_ANIMATION_ONLY;
    /* Source2D:3CF2 copies HP/QI/ATK/DEF only. File23..28 are ignored
       in this battle path, confirmed with original spell42. */
    return 1;
}
int npe_character(const NpeFile *file, unsigned index,
                  NpeCharacter *character) {
    const uint8_t *raw;
    size_t size;
    if (!character || index >= 3 ||
        npe_record(file, 12, index, &raw, &size) || size != 94)
        return -1;
    /* PRG source 2E:7378 reads the 12-byte name. Source 2E:746E..
       764C reads six consecutive bytes into 52:69D8..69DD. */
    character->name = raw;
    character->equipped_items = raw + 13;
    character->raw = raw;
    character->size = size;
    /* Hero record 0 has 130/119 and 60/59 here; the native battle record
       holds max/current HP and QI in that same order at +4E..+54. */
    character->max_hp = be16(raw + 40);
    character->hp = be16(raw + 42);
    character->max_qi = be16(raw + 44);
    character->qi = be16(raw + 46);
    character->attack = be16(raw + 54);
    character->defense = be16(raw + 56);
    character->experience_to_next = be16(raw + 36);
    character->experience = be16(raw + 38);
    /*2E:7404 writes the one-based record ordinal as runtime ID69D2;
      the subsequent file byte12 is sex69D7, not a stored character ID.
      The loader later writes file byte35 as runtime level69DE. */
    character->level = raw[35];
    character->other_stats[0] = be16(raw + 48);
    character->other_stats[1] = be16(raw + 50);
    character->other_stats[2] = be16(raw + 52);
    memcpy(character->growth, raw + 87, 7);
    /* Battle byte6468 is the current level, not the first skill +58. */
    character->damage_floor_stat = character->level;
    return 0;
}
int npe_character_record_seed(const NpeFile *file,unsigned index,uint8_t record[148]){
    NpeCharacter c;if(!record||npe_character(file,index,&c))return -1;
    const uint8_t *r=c.raw;
    /*2E:7378..872C streams scattered fields; do not clear untouched gaps. */
    memcpy(record,r,12);record[12]=(uint8_t)(index+1);
    record[17]=r[12];memcpy(record+18,r+13,6);
    memcpy(record+46,r+19,16);record[24]=r[35];
    memcpy(record+74,r+36,22);memcpy(record+25,r+58,18);
    record[43]=r[76];memcpy(record+13,r+77,2);record[44]=r[79];
    memcpy(record+124,r+80,2);
    /*83BE reads two bytes at+135; the next animation read overwrites+136. */
    memcpy(record+135,r+82,2);memcpy(record+136,r+84,3);
    memcpy(record+139,r+87,7);return 0;
}
int npe_character_metadata(const NpeFile *file, unsigned index, NpeCharacterMetadata *metadata) {
    NpeCharacter character;
    if(!metadata || npe_character(file,index,&character))return -1;
    memset(metadata,0,sizeof(*metadata));
    metadata->sex=character.raw[12];
    for(unsigned i=0;i<8;++i)metadata->pose_ids[i]=be16(character.raw+19+2*i);
    memcpy(metadata->animation_ids,character.raw+84,3);
    return 0;
}
int npe_enemy(const NpeFile *file, unsigned index, NpeEnemy *enemy) {
    const uint8_t *raw, *end;
    size_t size;
    if (!enemy || npe_record(file, 4, index, &raw, &size) || size != 39)
        return -1;
    end = memchr(raw, 0, 12);
    if (!end || end == raw) return -1;
    enemy->name = raw;
    enemy->name_size = (size_t)(end - raw);
    enemy->graphic = raw[13];
    /* Hero enemy 2 has 40/35/15/6 in these words; the native battle record
       repeats them at +4E/+52/+5C/+5E respectively. */
    enemy->max_hp = be16(raw + 15);
    enemy->max_qi = be16(raw + 17);
    enemy->attack = be16(raw + 19);
    enemy->defense = be16(raw + 21);
    /* Source 31:CDDA..CE24 streams enemy +25 into current speed64A8. */
    enemy->speed = be16(raw + 25);
    /* Source31:CE26..CE72 streams the following word into64AA. */
    enemy->luck = be16(raw + 27);
    /* Source31:CA80..CAEE reads +14 into battle byte6468; source
       31:CE7A..CF20 reads +29..31 into skill bytes6469..646B. */
    enemy->damage_floor_stat = raw[14];
    memcpy(enemy->skill_ids, raw + 29, sizeof(enemy->skill_ids));
    /* Tianjiang thief (enemy 53) has BE16 3/4 at +34/+36. The native
       victory trace awards 3 gold and 4 experience for this encounter. */
    enemy->gold_reward = be16(raw + 34);
    enemy->experience_reward = be16(raw + 36);
    /* Source 31:CF4C..CFBE streams raw +32/+33 into battle bytes
       64CA/64CB, consumed by post-victory source 39:40AE. */
    enemy->drop_item_ids[0] = raw[32];
    enemy->drop_item_ids[1] = raw[33];
    enemy->raw = raw;
    return 0;
}
int npe_enemy_bitmap(const NpeFile *file, const NpeEnemy *enemy,
                     NpeIndexedBitmap *bitmap) {
    unsigned start, end;
    if (!file || !enemy || !bitmap || !enemy->graphic) return -1;
    start = file->vectors[4];
    end = file->vectors[5];
    if (start >= end || enemy->graphic > end - start ||
        npe_indexed_bitmap(file, start + enemy->graphic, bitmap) ||
        bitmap->width != 40 || bitmap->height != 50) return -1;
    return 0;
}
int npe_map_npc(const NpeMap *map, unsigned index, const uint8_t **raw) {
    if (!map || !raw || index >= 20 || map->tail_size < 340) return -1;
    *raw = map->tail + 80 + index * 13;
    return 0;
}
int npe_map_npc_active(const uint8_t *raw, unsigned story_progress) {
    unsigned first, last;
    if (!raw || (!raw[0] && !raw[1])) return 0;
    first = be16(raw + 8);
    last = be16(raw + 10);
    /* Source 3A:0272..0348 accepts inclusive bounds, with a separate
       zero/zero branch for characters shown at every story stage. */
    return (!first && !last) ||
           (story_progress >= first && story_progress <= last);
}
int npe_map_monster(const NpeMap *map, unsigned index,
                    NpeMapMonster *monster) {
    const uint8_t *raw;
    if (!map || !monster || index >= 4 || map->tail_size < 644) return -1;
    raw = map->tail + 592 + index * 13;
    monster->enemy_id = raw[0];
    monster->x = raw[1]; monster->y = raw[2];
    monster->min_progress = be16(raw + 3);
    monster->max_progress = be16(raw + 5);
    monster->graphic = be16(raw + 7);
    monster->npc_id = be16(raw + 9);
    monster->left_enemy_id = raw[11];
    monster->right_enemy_id = raw[12];
    monster->raw = raw;
    return 0;
}
int npe_map_monster_active(const NpeMapMonster *monster,
                           unsigned story_progress) {
    /* The three tower elders use 0/0 as the always-visible range. */
    return monster && monster->enemy_id && monster->graphic &&
           monster->npc_id &&
           ((monster->min_progress == 0 &&
             monster->max_progress == 0) ||
            (story_progress >= monster->min_progress &&
             story_progress <= monster->max_progress));
}
int npe_map_exit(const NpeMap *map, unsigned index, NpeMapExit *exit) {
    const uint8_t *raw;
    if (!map || !exit || index >= 12 || map->tail_size < 592) return -1;
    raw = map->tail + 340 + index * 21;
    exit->x = raw[0]; exit->y = raw[1];exit->kind=raw[2];
    exit->destination_map = be16(raw + 3);
    exit->min_progress = be16(raw + 5);
    exit->destination_x = raw[7]; exit->destination_y = raw[8];
    exit->next_mode = raw[10]; exit->next_id = raw[11];
    exit->second_map=be16(raw+12);exit->second_min_progress=be16(raw+14);
    exit->second_x=raw[16];exit->second_y=raw[17];
    exit->second_mode=raw[19];exit->second_id=raw[20];
    exit->raw = raw;
    return 0;
}
int npe_map_encounter(const NpeMap *map, unsigned slot,
                      unsigned *enemy_id) {
    if (!map || !enemy_id || slot >= 8 || map->tail_size < 652)
        return -1;
    *enemy_id = map->tail[644 + slot];
    return *enemy_id ? 1 : 0;
}
int npe_npc_action(const NpeFile *file, unsigned npc_id, unsigned case_index,
                   NpeNpcAction *action) {
    const NpeSection *section;
    const uint8_t *data, *entry, *raw;
    uint32_t table_size, offset;
    unsigned count, i;
    if (!file || !action || file->section_count <= 5 || !npc_id) return -1;
    section = &file->sections[5];
    if (section->size < 5) return -1;
    data = file->bytes + section->offset;
    table_size = be32(data + 1);
    if (!table_size || table_size % 5 || table_size > section->size ||
        npc_id > table_size / 5) return -1;
    entry = data + (npc_id - 1) * 5;
    count = entry[0]; offset = be32(entry + 1);
    if (!count || count == 255 || case_index >= count ||
        offset < table_size || offset > section->size ||
        (size_t)count * 38 > section->size - offset) return -1;
    raw = data + offset + case_index * 38;
    for (i = 0; i < 3; ++i) {
        action->dialogue_offset[i] = be32(raw + 26 + i * 4);
        if (action->dialogue_offset[i] >= section->size) return -1;
    }
    action->case_count = count;
    action->raw = raw;
    return 0;
}
int npe_npc_action_for_state(const NpeFile *file, unsigned npc_id,
                             uint8_t state, NpeNpcAction *action,
                             unsigned *selected_case) {
    NpeNpcAction first;
    unsigned index;
    if (!action || npe_npc_action(file, npc_id, 0, &first)) return -1;
    index = state < first.case_count ? state : first.case_count - 1;
    if (npe_npc_action(file, npc_id, index, action)) return -1;
    if (selected_case) *selected_case = index;
    return 0;
}
int npe_dialogue_page(const NpeFile *file, uint32_t offset,
                      NpeDialoguePage *page) {
    const NpeSection *section;
    const uint8_t *data;
    size_t pos;
    if (!file || !page || file->section_count <= 5) return -1;
    section = &file->sections[5];
    if (offset >= section->size) return -1;
    data = file->bytes + section->offset;
    pos = offset;
    while (pos < section->size) {
        uint8_t value = data[pos];
        if (value == '@' || value == '&' || value == 0) {
            page->bytes = data + offset;
            page->size = pos - offset;
            page->terminator = value;
            return 0;
        }
        if (value >= 0x81 && value <= 0xfe) {
            if (pos + 1 >= section->size) return -1;
            pos += 2;
        } else pos++;
    }
    return -1;
}
int npe_story_record(const NpeFile *file, unsigned index,
                     NpeStoryRecord *story) {
    const uint8_t *raw;
    size_t size, i;
    if (!story || npe_record(file, 10, index, &raw, &size) || size < 9 ||
        raw[7] != '{' || raw[size - 2] != '@' || raw[size - 1] != 0)
        return -1;
    for (i = 7; i < size - 2;) {
        if (raw[i] >= 0x81 && raw[i] <= 0xfe) {
            if (i + 1 >= size - 2) return -1;
            i += 2;
        } else {
            if (raw[i] == 0 || raw[i] == '@') return -1;
            i++;
        }
    }
    story->header_value = be32(raw);
    story->control_byte = raw[4];
    story->control_word = be16(raw + 5);
    story->text = raw + 7;
    story->text_size = size - 9;
    return 0;
}
int npe_map_spawn_viewport(const NpeMap *map, unsigned x, unsigned y,
                           NpeViewport *viewport) {
    if (!map || !viewport) return -1;
    /* PRG48:2290..23B2 uses argument low bytes, compares promoted values,
       then stores offsets/screen positions as bytes. No dimension clamp. */
    x = (uint8_t)x; y = (uint8_t)y;
    if (x + 8 < map->width) {
        viewport->offset_x = x < 7 ? 0 : (uint8_t)(x - 7);
        viewport->screen_x = x < 7 ? x : 7;
    } else {
        viewport->offset_x = (uint8_t)(map->width - 15);
        viewport->screen_x = (uint8_t)(x - viewport->offset_x);
    }
    if (y + 2 < map->height) {
        viewport->offset_y = y < 4 ? 0 : (uint8_t)(y - 4);
        viewport->screen_y = y < 4 ? y : 4;
    } else {
        viewport->offset_y = (uint8_t)(map->height - 7);
        viewport->screen_y = (uint8_t)(y - viewport->offset_y - 1);
    }
    return 0;
}
int npe_render_map(const NpeFile *file, const NpeMap *map,
                   unsigned left, unsigned top,
                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned row, col, x, y;
    NpeIndexedBitmap bitmap;
    uint16_t cached[105];int resident=0;
    if (!file || !map || !pixels)
        return -1;
    if(map->resident_file==file){
        resident=npe_rpg_runtime_map_cache(file,map->number,left,top,cached);
        if(resident<0)return -1;
    }
    if(!resident && (left>=map->width || top>=map->height))return -1;
    /* The native seven-row map sits between four LCD rows of color 2. */
    memset(pixels, 2, NPE_WIDTH * NPE_HEIGHT);
    for (row = 0; row < 7 && (resident || top + row < map->height); ++row) {
        for (col = 0; col < 15 && (resident || left + col < map->width); ++col) {
            uint16_t tile = resident?cached[row*15+col]:npe_map_tile(map, left + col, top + row);
            if(file->rpg_runtime){
                int result=npe_rpg_runtime_draw_indexed(file,(int16_t)tile,
                    (int16_t)(col*16),(int16_t)(4+row*16),0,pixels);
                if(result==-2)return -1;
                continue;
            }
            if (npe_indexed_bitmap(file, tile, &bitmap) ||
                bitmap.width != 16 || bitmap.height != 16) return -1;
            for (y = 0; y < 16; ++y)
                for (x = 0; x < 16; ++x)
                    /* Native map frames leave four blank LCD rows above
                       and below the seven 16-pixel tile rows. */
                    pixels[(4 + row * 16 + y) * NPE_WIDTH + col * 16 + x] =
                        npe_indexed_pixel(&bitmap, x, y);
        }
    }
    return 0;
}
