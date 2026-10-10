#include "npe.h"
#include <SDL.h>
#include <string.h>

/* Host-only, versioned save. Every field is encoded explicitly so pointers,
   structure padding and host byte order never enter the file. */
enum { SAVE_HEADER = 36, SAVE_CAPACITY = 4096+196+2*NPE_MAP_SNAPSHOT_WORDS };
enum { SAVE_WALK_TRIGGER_TAG = 0x4c425457 }; /*Optional trailing "WTBL"; existing flags100..199 contain the cache. */
enum { SAVE_PARTY_GROUP_TAG = 0x5047 }; /* "GP" in the five V19 reserved bytes. */
typedef struct { uint8_t bytes[SAVE_CAPACITY]; size_t pos, size; int bad; } Codec;

static uint64_t hash_bytes(const uint8_t *bytes, size_t size) {
    uint64_t value = UINT64_C(14695981039346656037);
    size_t i;
    for (i = 0; i < size; ++i) {
        value ^= bytes[i];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

uint64_t npe_game_fingerprint(const NpeFile *file) {
    return file && file->bytes ? hash_bytes(file->bytes, file->size) : 0;
}

static void put(Codec *c, uint64_t value, unsigned count) {
    unsigned i;
    if (c->bad || c->pos + count > SAVE_CAPACITY) { c->bad = 1; return; }
    for (i = 0; i < count; ++i) c->bytes[c->pos++] = (uint8_t)(value >> (8 * i));
}

static uint64_t get(Codec *c, unsigned count) {
    uint64_t value = 0;
    unsigned i;
    if (c->bad || c->pos + count > c->size) { c->bad = 1; return 0; }
    for (i = 0; i < count; ++i) value |= (uint64_t)c->bytes[c->pos++] << (8 * i);
    return value;
}

static void write_world(Codec *c, const NpeWorld *w) {
    unsigned i, j;
    uint32_t npc_positions = 0;
    put(c, w->map_number, 2); put(c, w->x, 2); put(c, w->y, 2);
    put(c, w->direction, 1);
    put(c, w->view.screen_x, 1); put(c, w->view.screen_y, 1);
    put(c, w->view.offset_x, 2); put(c, w->view.offset_y, 2);
    for (i = 0; i < 256; ++i) put(c, w->npc_state[i], 1);
    for (i = 0; i < 255; ++i) put(c, w->condition_words[i], 2);
    for (i = 0; i < 3; ++i) for (j = 0; j < 6; ++j)
        put(c, w->equipped_items[i][j], 1);
    for (i = 0; i < 3; ++i) {
        put(c, w->hp[i], 2); put(c, w->max_hp[i], 2);
        put(c, w->qi[i], 2); put(c, w->max_qi[i], 2);
        put(c, w->attack[i], 2); put(c, w->defense[i], 2);
        put(c, w->damage_floor_stat[i], 1);
        for (j = 0; j < 3; ++j) put(c, w->status_guard[i][j], 2);
    }
    for (i = 0; i < 200; ++i) put(c, w->condition5_bytes[i], 1);
    put(c, w->story_progress, 2); put(c, w->story_event_id, 2);
    put(c, w->money, 4);
    for (i = 0; i < 256; ++i) put(c, w->object_done[i], 1);
    put(c, w->random_state, 4); put(c, w->encounter_steps, 2);
    for (i = 0; i < 3; ++i) {
        put(c, w->experience[i], 2);
        put(c, w->experience_to_next[i], 2);
    }
    for (i = 0; i < 10; ++i) {
        put(c, w->quiz_category_correct[i], 2);
        put(c, w->quiz_category_wrong[i], 2);
    }
    /* V19 extension within the five reserved host-session bytes.
       Old V19 readers ignore them; offsets and payload size stay fixed. */
    put(c, SAVE_PARTY_GROUP_TAG, 2);
    for(i=0;i<3;++i)put(c,w->party_attack_groups[i],1);
    for (i = 0; i < 3; ++i) {
        put(c, w->level[i], 1);
        for (j = 0; j < 7; ++j) put(c, w->growth[i][j], 1);
        for (j = 0; j < 3; ++j) put(c, w->other_stats[i][j], 2);
    }
    for (i = 0; i < 20; ++i) {
        unsigned x=w->npc_override_x[i], y=w->npc_override_y[i];
        if ((x==NPE_NPC_POSITION_NONE)!=(y==NPE_NPC_POSITION_NONE) ||
            (x!=NPE_NPC_POSITION_NONE && (x>255 || y>255))) c->bad=1;
        if (x!=NPE_NPC_POSITION_NONE) npc_positions |= UINT32_C(1)<<i;
        put(c, w->npc_override_x[i], 1);
        put(c, w->npc_override_y[i], 1);
    }
    for (i = 0; i < 3; ++i) {
        put(c, w->party_ids[i], 1);
        put(c, w->party_active[i], 1);
    }
    for (i = 0; i < 20; ++i) put(c, w->npc_override_direction[i], 1);
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 18; ++j) put(c, w->skill_slots[i][j], 1);
        put(c, w->learned_skill_count[i], 1);
    }
    for (i = 0; i < 2; ++i) {
        put(c, w->chase_x[i], 2); put(c, w->chase_y[i], 2);
        put(c, w->chase_cooldown[i], 1);
    }
    put(c, w->tile_change_applied, 1); put(c, w->tile_override_count, 1);
    for (i = 0; i < 45; ++i) {
        put(c, w->tile_override_indices[i], 2);
        put(c, w->tile_override_tiles[i], 2);
    }
    /* Append the missing fourth map state; retain every V14 field offset. */
    for (i = 0; i < 3; ++i) put(c, w->status_guard[i][3], 2);
    put(c, w->quiz_fixed_correct, 2); put(c, w->quiz_fixed_wrong, 2);
    /* V17 keeps the V16 prefix and distinguishes coordinate255 from its
       historical absent-position byte using one validity bit per pair. */
    put(c, npc_positions, 4);
    for(i=0;i<2;++i)put(c,w->companion_directions[i],1);
    /*V19 retains the V18 prefix and adds eight object tile pairs. */
    for(i=45;i<NPE_TILE_OVERRIDES;++i){
        put(c,w->tile_override_indices[i],2);put(c,w->tile_override_tiles[i],2);
    }
    /* V20 appends map character metadata; battle copies are transient.
       Encode 20 bytes per character, independent of C structure padding. */
    for(i=0;i<3;++i){
        put(c,w->party_characters[i].sex,1);
        for(j=0;j<8;++j)put(c,w->party_characters[i].pose_ids[j],2);
        for(j=0;j<3;++j)put(c,w->party_characters[i].animation_ids[j],1);
    }
    /* V21 appends all twelve resident name bytes, including NUL tails. */
    for(i=0;i<3;++i)for(j=0;j<12;++j)put(c,w->party_names[i][j],1);
    /*V22 retains the V21 prefix and appends the opaque record images. */
    for(i=0;i<3;++i)for(j=0;j<148;++j)put(c,w->party_record_residue[i][j],1);
}

static void read_world(Codec *c, NpeWorld *w, int version, int *has_party_groups) {
    unsigned i, j;
    w->map_number = (unsigned)get(c, 2); w->x = (unsigned)get(c, 2);
    w->y = (unsigned)get(c, 2); w->direction = (unsigned)get(c, 1);
    w->view.screen_x = (unsigned)get(c, 1);
    w->view.screen_y = (unsigned)get(c, 1);
    w->view.offset_x = (unsigned)get(c, 2);
    w->view.offset_y = (unsigned)get(c, 2);
    for (i = 0; i < 256; ++i) w->npc_state[i] = (uint8_t)get(c, 1);
    for (i = 0; i < 255; ++i) w->condition_words[i] = (uint16_t)get(c, 2);
    for (i = 0; i < 3; ++i) for (j = 0; j < 6; ++j)
        w->equipped_items[i][j] = (uint8_t)get(c, 1);
    for (i = 0; i < 3; ++i) {
        w->hp[i] = (uint16_t)get(c, 2); w->max_hp[i] = (uint16_t)get(c, 2);
        w->qi[i] = (uint16_t)get(c, 2); w->max_qi[i] = (uint16_t)get(c, 2);
        w->attack[i] = (uint16_t)get(c, 2);
        w->defense[i] = (uint16_t)get(c, 2);
        w->damage_floor_stat[i] = (uint8_t)get(c, 1);
        for (j = 0; j < 3; ++j)
            w->status_guard[i][j] = (uint16_t)get(c, 2);
    }
    for (i = 0; i < 200; ++i) w->condition5_bytes[i] = (uint8_t)get(c, 1);
    w->story_progress = (uint16_t)get(c, 2);
    w->story_event_id = (uint16_t)get(c, 2);
    w->money = (uint32_t)get(c, 4);
    for (i = 0; i < 256; ++i) w->object_done[i] = (uint8_t)get(c, 1);
    w->random_state = (uint32_t)get(c, 4);
    w->encounter_steps = (uint16_t)get(c, 2);
    if (version >= 2) for (i = 0; i < 3; ++i) {
        w->experience[i] = (uint16_t)get(c, 2);
        w->experience_to_next[i] = (uint16_t)get(c, 2);
    }
    if (version >= 3) {
        for (i = 0; i < 10; ++i) {
            w->quiz_category_correct[i] = (uint16_t)get(c, 2);
            w->quiz_category_wrong[i] = (uint16_t)get(c, 2);
        }
        /* Older saves kept host quiz-session data here, then reserved
           it. Recognize only the V19 tagged map-group extension. */
        unsigned tag=(unsigned)get(c,2);
        uint8_t groups[3];for(i=0;i<3;++i)groups[i]=(uint8_t)get(c,1);
        if(version>=19 && tag==SAVE_PARTY_GROUP_TAG){
            memcpy(w->party_attack_groups,groups,3);*has_party_groups=1;
        }
    }
    if (version >= 4) for (i = 0; i < 3; ++i) {
        w->level[i] = (uint8_t)get(c, 1);
        for (j = 0; j < 7; ++j) w->growth[i][j] = (uint8_t)get(c, 1);
        for (j = 0; j < 3; ++j) w->other_stats[i][j] = (uint16_t)get(c, 2);
    }
    if (version >= 5) for (i = 0; i < 20; ++i) {
        w->npc_override_x[i] = (uint8_t)get(c, 1);
        w->npc_override_y[i] = (uint8_t)get(c, 1);
    }
    if (version >= 6) for (i = 0; i < 3; ++i) {
        w->party_ids[i] = (uint8_t)get(c, 1);
        w->party_active[i] = (uint8_t)get(c, 1);
    }
    if (version >= 7) for (i = 0; i < 20; ++i)
        w->npc_override_direction[i] = (uint8_t)get(c, 1);
    if (version >= 8) for (i = 0; i < 3; ++i) {
        for (j = 0; j < 18; ++j)
            w->skill_slots[i][j] = (uint8_t)get(c, 1);
        w->learned_skill_count[i] = (uint8_t)get(c, 1);
    }
    if (version >= 13) for (i = 0; i < 2; ++i) {
        w->chase_x[i] = (uint16_t)get(c, 2);
        w->chase_y[i] = (uint16_t)get(c, 2);
        w->chase_cooldown[i] = (uint8_t)get(c, 1);
    }
    if (version >= 14) {
        w->tile_change_applied = (uint8_t)get(c, 1);
        w->tile_override_count = (uint8_t)get(c, 1);
        if (w->tile_override_count > (version>=19?NPE_TILE_OVERRIDES:45) || w->tile_change_applied > 7) c->bad = 1;
        for (i = 0; i < 45; ++i) {
            w->tile_override_indices[i] = (uint16_t)get(c, 2);
            w->tile_override_tiles[i] = (uint16_t)get(c, 2);
        }
    }
    if (version >= 15) for (i = 0; i < 3; ++i)
        w->status_guard[i][3] = (uint16_t)get(c, 2);
    if (version >= 16) {
        w->quiz_fixed_correct = (uint16_t)get(c, 2);
        w->quiz_fixed_wrong = (uint16_t)get(c, 2);
    }
    uint32_t npc_positions=version>=17?(uint32_t)get(c,4):0;
    if (npc_positions & ~UINT32_C(0x000fffff)) c->bad=1;
    if(version>=18)for(i=0;i<2;++i)w->companion_directions[i]=(uint8_t)get(c,1);
    if(version>=19)for(i=45;i<NPE_TILE_OVERRIDES;++i){
        w->tile_override_indices[i]=(uint16_t)get(c,2);w->tile_override_tiles[i]=(uint16_t)get(c,2);
    }
    if(version>=20)for(i=0;i<3;++i){
        w->party_characters[i].sex=(uint8_t)get(c,1);
        for(j=0;j<8;++j)w->party_characters[i].pose_ids[j]=(uint16_t)get(c,2);
        for(j=0;j<3;++j)w->party_characters[i].animation_ids[j]=(uint8_t)get(c,1);
    }
    if(version>=21)for(i=0;i<3;++i)for(j=0;j<12;++j)w->party_names[i][j]=(uint8_t)get(c,1);
    if(version>=22)for(i=0;i<3;++i)for(j=0;j<148;++j)w->party_record_residue[i][j]=(uint8_t)get(c,1);
    if (version>=5) for(i=0;i<20;++i){
        unsigned x=w->npc_override_x[i],y=w->npc_override_y[i];
        if(version>=17 && (npc_positions&(UINT32_C(1)<<i))) continue;
        if(version>=17 && (x!=255 || y!=255)) c->bad=1;
        if(x==255)w->npc_override_x[i]=NPE_NPC_POSITION_NONE;
        if(y==255)w->npc_override_y[i]=NPE_NPC_POSITION_NONE;
    }
}

static int valid_position(const NpeFile *file, const NpeWorld *w) {
    NpeMap map;
    NpeViewport spawn;
    if (!w->map_number || w->direction > 255 ||
        w->view.screen_x > 255 || w->view.screen_y > 255 ||
        w->view.offset_x > 255 || w->view.offset_y > 255 ||
        npe_map(file, w->map_number - 1, &map)) return 0;
    if (w->x < map.width && w->y < map.height &&
        w->view.screen_x < 15 && w->view.screen_y < 7 &&
        w->view.offset_x < map.width && w->view.offset_y < map.height &&
        w->view.offset_x + w->view.screen_x == w->x &&
        w->y >= w->view.offset_y) {
        unsigned expected_y = w->y - w->view.offset_y;
        if (w->view.screen_y == expected_y) return 1;
        /* Native lower-edge spawn retains logical Y one row below screen Y. */
        if (map.height > 7 && w->y + 2 >= map.height && expected_y &&
            w->view.screen_y + 1 == expected_y) return 1;
    }
    /*48:2290..23B2 accepts zero/small dimensions and out-of-grid spawn
      bytes. Its byte offsets can wrap beyond the nominal viewport; own
      saves must accept the same constructor result without clamping it.
      Keep position/view consistency and reject unrelated host coordinates. */
    unsigned sum_x = w->view.offset_x + w->view.screen_x;
    unsigned sum_y = w->view.offset_y + w->view.screen_y;
    if ((w->x != sum_x && w->x != (uint8_t)sum_x) ||
        (w->y != sum_y && w->y != (uint8_t)sum_y &&
         w->y != sum_y + 1 && w->y != (uint8_t)(sum_y + 1)) ||
        npe_map_spawn_viewport(&map, w->x, w->y, &spawn)) return 0;
    return spawn.offset_x == w->view.offset_x && spawn.offset_y == w->view.offset_y &&
           spawn.screen_x == w->view.screen_x && spawn.screen_y == w->view.screen_y;
}

#include "save_terrain.inc"

int npe_world_save(const NpeFile *file, const NpeWorld *world,
                   const char *path) {
    Codec c = {{0}, 0, 0, 0};
    SDL_RWops *stream;
    int result;
    if (!file || !file->bytes || !world || !path || world->game_mode != 1 ||
        !valid_position(file, world)) return -1;
    int extended=save_terrain_extended(file,world);if(extended<0)return -1;
    memcpy(c.bytes, extended==2?"NPESAV24":extended?"NPESAV23":"NPESAV22", 8); c.pos = 8;
    put(&c, file->size, 8); put(&c, npe_game_fingerprint(file), 8);
    c.pos = SAVE_HEADER;
    write_world(&c, world);
    if(extended && write_terrain_snapshot(&c,file,world,extended==2))return -1;
    put(&c,SAVE_WALK_TRIGGER_TAG,4);
    if (c.bad) return -1;
    c.size = c.pos;
    c.pos = 24;
    put(&c, c.size - SAVE_HEADER, 4);
    put(&c, hash_bytes(c.bytes + SAVE_HEADER, c.size - SAVE_HEADER), 8);
    stream = SDL_RWFromFile(path, "wb");
    if (!stream) return -1;
    result = SDL_RWwrite(stream, c.bytes, 1, c.size) == c.size;
    if (SDL_RWclose(stream) != 0) result = 0;
    return result ? 0 : -1;
}

int npe_world_load(const NpeFile *file, NpeWorld *world,
                   const char *path) {
    Codec c = {{0}, 0, 0, 0};
    NpeWorld saved, restored;
    SDL_RWops *stream;
    Sint64 length;
    uint64_t file_size, fingerprint, checksum;
    uint32_t payload_size;
    unsigned i;
    int version,has_party_groups=0,has_walk_triggers=0;
    if (!file || !file->bytes || !world || !path) return -1;
    stream = SDL_RWFromFile(path, "rb");
    if (!stream) return -1;
    length = SDL_RWsize(stream);
    if (length < SAVE_HEADER || length > SAVE_CAPACITY ||
        SDL_RWread(stream, c.bytes, 1, (size_t)length) != (size_t)length) {
        SDL_RWclose(stream); return -1;
    }
    if (SDL_RWclose(stream) != 0) return -1;
    c.size = (size_t)length;
    version = memcmp(c.bytes, "NPESAV24", 8) == 0 ? 24 :
              memcmp(c.bytes, "NPESAV23", 8) == 0 ? 23 :
              memcmp(c.bytes, "NPESAV22", 8) == 0 ? 22 :
              memcmp(c.bytes, "NPESAV21", 8) == 0 ? 21 :
              memcmp(c.bytes, "NPESAV20", 8) == 0 ? 20 :
              memcmp(c.bytes, "NPESAV19", 8) == 0 ? 19 :
              memcmp(c.bytes, "NPESAV18", 8) == 0 ? 18 :
              memcmp(c.bytes, "NPESAV17", 8) == 0 ? 17 :
              memcmp(c.bytes, "NPESAV16", 8) == 0 ? 16 :
              memcmp(c.bytes, "NPESAV15", 8) == 0 ? 15 :
              memcmp(c.bytes, "NPESAV14", 8) == 0 ? 14 :
              memcmp(c.bytes, "NPESAV13", 8) == 0 ? 13 :
              memcmp(c.bytes, "NPESAV12", 8) == 0 ? 12 :
              memcmp(c.bytes, "NPESAV11", 8) == 0 ? 11 :
              memcmp(c.bytes, "NPESAV10", 8) == 0 ? 10 :
              memcmp(c.bytes, "NPESAV09", 8) == 0 ? 9 :
              memcmp(c.bytes, "NPESAV08", 8) == 0 ? 8 :
              memcmp(c.bytes, "NPESAV07", 8) == 0 ? 7 :
              memcmp(c.bytes, "NPESAV06", 8) == 0 ? 6 :
              memcmp(c.bytes, "NPESAV05", 8) == 0 ? 5 :
              memcmp(c.bytes, "NPESAV04", 8) == 0 ? 4 :
              memcmp(c.bytes, "NPESAV03", 8) == 0 ? 3 :
              memcmp(c.bytes, "NPESAV02", 8) == 0 ? 2 :
              memcmp(c.bytes, "NPESAV01", 8) == 0 ? 1 : 0;
    if (!version) return -1;
    c.pos = 8;
    file_size = get(&c, 8); fingerprint = get(&c, 8);
    payload_size = (uint32_t)get(&c, 4); checksum = get(&c, 8);
    if (c.bad || file_size != file->size ||
        fingerprint != npe_game_fingerprint(file) ||
        payload_size != c.size - SAVE_HEADER ||
        checksum != hash_bytes(c.bytes + SAVE_HEADER, payload_size)) return -1;
    memset(&saved, 0, sizeof(saved));
    c.pos = SAVE_HEADER;
    read_world(&c, &saved, version,&has_party_groups);
    if(version>=23)read_terrain_snapshot(&c,&saved,version>=24);
    if(version>=22 && !c.bad && c.pos<=c.size && c.size-c.pos==4){
        has_walk_triggers=get(&c,4)==SAVE_WALK_TRIGGER_TAG;
        if(!has_walk_triggers)c.bad=1;
    }
    if (c.bad || c.pos != c.size || !valid_position(file, &saved)) return -1;
    if (version >= 3 && version < 16) {
        unsigned first, count;
        /* The old C runtime merged fixed questions into category5.
           Empty category5 proves this slot contains only fixed counts.
           Otherwise the aggregate cannot be separated: retain it in
           both independent counters, then update them separately. */
        saved.quiz_fixed_correct = saved.quiz_category_correct[5];
        saved.quiz_fixed_wrong = saved.quiz_category_wrong[5];
        if (!npe_quiz_category_range(file, 5, &first, &count) && !count) {
            saved.quiz_category_correct[5] = 0;
            saved.quiz_category_wrong[5] = 0;
        }
    }
    if (version >= 5) {
        NpeMap map;
        if (npe_map(file, saved.map_number - 1, &map)) return -1;
        if (version <= 11) for (i = 0; i < 20; ++i) {
            const uint8_t *npc;
            unsigned event_id, matches = 0, target_x = 0, target_y = 0;
            if (saved.npc_override_x[i] == NPE_NPC_POSITION_NONE) continue;
            if (npe_map_npc(&map, i, &npc)) return -1;
            /* V5..V11 placed moved NPCs at raw+6/+7. Migrate only
               overrides matching this NPC's old event coordinates;
               leave unrelated positions and all other state intact. */
            for (event_id = 1; event_id <= 5; ++event_id) {
                NpeMapNpcMoveEvent event;
                int found = npe_map_npc_move_event(&map, event_id, &event);
                if (found < 0) return -1;
                if (!found) break;
                if (event.npc_id != ((unsigned)npc[0] << 8 | npc[1]) ||
                    saved.npc_override_x[i] != event.raw[6] ||
                    saved.npc_override_y[i] != event.raw[7]) continue;
                if (matches && (target_x != event.x || target_y != event.y))
                    return -1;
                target_x = event.x; target_y = event.y; ++matches;
            }
            if (matches) {
                saved.npc_override_x[i] = (uint8_t)target_x;
                saved.npc_override_y[i] = (uint8_t)target_y;
            }
        }
        for (i = 0; i < 20; ++i) {
            unsigned x = saved.npc_override_x[i];
            unsigned y = saved.npc_override_y[i];
            /* Scripted NPC movement may leave map bounds, including255. */
            if ((x == NPE_NPC_POSITION_NONE) != (y == NPE_NPC_POSITION_NONE) ||
                (x != NPE_NPC_POSITION_NONE && (x>255 || y>255))) return -1;
        }
    }
    if (version >= 6) {
        unsigned seen = 0;
        for (i = 0; i < 3; ++i) {
            unsigned id = saved.party_ids[i];
            /* V18+ retains resident flag bytes and holes in the party.
               A battle can return flag0 in slot0, or non-Boolean bytes. */
            if (id < 1 || id > 3 || (seen & (1u << id)) ||
                (version<18 && saved.party_active[i]>1) ||
                (version<18 && i && saved.party_active[i] > saved.party_active[i - 1]))
                return -1;
            seen |= 1u << id;
        }
        if (version<18 && (!saved.party_active[0] || saved.party_ids[0] != 1)) return -1;
    }
    /* Resident NPC records retain the complete direction byte, including
       inactive records and writes through shared RAM. Current slots can
       save these bytes; do not reject their own saved state on loading. */
    if (version >= 7 && version < 18) for (i = 0; i < 20; ++i)
        if (saved.npc_override_direction[i] > 3 &&
            saved.npc_override_direction[i] != 255) return -1;
    if (version >= 8) for (i = 0; i < 3; ++i)
        if (saved.learned_skill_count[i] > 18) return -1;
    for (i = 0; i < 3; ++i)
        /* Native damage can wrap negative HP into positive HP above the
           maximum. Preserve every HP low word when loading that state. */
        if (saved.qi[i] > saved.max_qi[i] &&
            (int16_t)saved.qi[i] > (int16_t)saved.max_qi[i]) return -1;
    memset(&restored, 0, sizeof(restored));
    if (npe_world_init(file, &restored, saved.map_number, saved.x, saved.y))
        return -1;
    restored.direction = saved.direction;
    restored.view = saved.view;
    memcpy(restored.npc_state, saved.npc_state, sizeof(saved.npc_state));
    if (version >= 5) {
        memcpy(restored.npc_override_x, saved.npc_override_x,
               sizeof(saved.npc_override_x));
        memcpy(restored.npc_override_y, saved.npc_override_y,
               sizeof(saved.npc_override_y));
    }
    if (version >= 7)
        memcpy(restored.npc_override_direction, saved.npc_override_direction,
               sizeof(saved.npc_override_direction));
    if (version >= 8) {
        memcpy(restored.skill_slots, saved.skill_slots,
               sizeof(saved.skill_slots));
        memcpy(restored.learned_skill_count, saved.learned_skill_count,
               sizeof(saved.learned_skill_count));
        if (version <= 10) for (i = 0; i < 3; ++i) {
            NpeCharacter character;
            unsigned learned = saved.learned_skill_count[i];
            if (npe_character(file, saved.party_ids[i] - 1, &character))
                return -1;
            /* V8 used raw+20, V9/V10 used raw+13 for the initial slots.
               Native loader 2E:7CB2..8258 reads them from raw+58.
               Preserve the learned prefix and rebuild the initial tail. */
            memcpy(restored.skill_slots[i] + learned,
                   character.raw + 58, 18 - learned);
        }
    }
    memcpy(restored.condition_words, saved.condition_words,
           sizeof(saved.condition_words));
    memcpy(restored.equipped_items, saved.equipped_items,
           sizeof(saved.equipped_items));
    if (version >= 6) {
        memcpy(restored.party_ids, saved.party_ids,
               sizeof(saved.party_ids));
        memcpy(restored.party_active, saved.party_active,
               sizeof(saved.party_active));
    }
    if(has_party_groups)memcpy(restored.party_attack_groups,saved.party_attack_groups,3);
    else if(file->section_count>12 && file->offset_table_records[12]>=3)
        for(i=0;i<3;++i){NpeCharacter character;
            if(npe_character(file,restored.party_ids[i]-1,&character))return -1;
            restored.party_attack_groups[i]=character.raw[79];
        }
    if(version>=20)memcpy(restored.party_characters,saved.party_characters,sizeof(restored.party_characters));
    else if(file->section_count>12 && file->offset_table_records[12]>=3)
        for(i=0;i<3;++i)
            if(npe_character_metadata(file,restored.party_ids[i]-1,&restored.party_characters[i]))return -1;
    if(version>=21)memcpy(restored.party_names,saved.party_names,sizeof(restored.party_names));
    else if(file->section_count>12 && file->offset_table_records[12]>=3)
        for(i=0;i<3;++i){NpeCharacter character;
            if(npe_character(file,restored.party_ids[i]-1,&character))return -1;
            memcpy(restored.party_names[i],character.name,12);
        }
    if(version>=22)memcpy(restored.party_record_residue,saved.party_record_residue,sizeof(restored.party_record_residue));
    else for(i=0;i<3;++i){
        memset(restored.party_record_residue[i],0,148);
        if(npe_character_record_seed(file,restored.party_ids[i]-1,restored.party_record_residue[i]))return -1;
    }
    if(version>=18)memcpy(restored.companion_directions,saved.companion_directions,2);
    else for(i=0;i<2;++i){NpeCharacter character;
        if(npe_character(file,restored.party_ids[i+1]-1,&character))return -1;
        restored.companion_directions[i]=character.raw[78];}
    memcpy(restored.hp, saved.hp, sizeof(saved.hp));
    memcpy(restored.max_hp, saved.max_hp, sizeof(saved.max_hp));
    memcpy(restored.qi, saved.qi, sizeof(saved.qi));
    memcpy(restored.max_qi, saved.max_qi, sizeof(saved.max_qi));
    memcpy(restored.attack, saved.attack, sizeof(saved.attack));
    memcpy(restored.defense, saved.defense, sizeof(saved.defense));
    if (version >= 2) {
        memcpy(restored.experience, saved.experience,
               sizeof(saved.experience));
        memcpy(restored.experience_to_next, saved.experience_to_next,
               sizeof(saved.experience_to_next));
    }
    memcpy(restored.damage_floor_stat, saved.damage_floor_stat,
           sizeof(saved.damage_floor_stat));
    memcpy(restored.status_guard, saved.status_guard,
           sizeof(saved.status_guard));
    memcpy(restored.condition5_bytes, saved.condition5_bytes,
           sizeof(saved.condition5_bytes));
    restored.story_progress = saved.story_progress;
    restored.story_event_id = saved.story_event_id;
    restored.money = saved.money;
    memcpy(restored.object_done, saved.object_done, sizeof(saved.object_done));
    if(!has_walk_triggers){
        /* Older own saves stored object flags here, before7490 had a cache
          owner. Reconstruct cold event slots rather than disabling triggers
          with those historical zero bytes. Warm restore reloads live slots. */
        NpeMap map;memset(restored.object_done+100,0,100);
        if(npe_map(file,restored.map_number-1,&map)||npe_world_load_walk_triggers(&restored,&map))return -1;
    }
    restored.random_state = saved.random_state;
    restored.encounter_steps = saved.encounter_steps;
    if (version >= 13) {
        memcpy(restored.chase_x, saved.chase_x, sizeof(saved.chase_x));
        memcpy(restored.chase_y, saved.chase_y, sizeof(saved.chase_y));
        memcpy(restored.chase_cooldown, saved.chase_cooldown,
               sizeof(saved.chase_cooldown));
    }
    if (version >= 3) {
        memcpy(restored.quiz_category_correct, saved.quiz_category_correct,
               sizeof(saved.quiz_category_correct));
        memcpy(restored.quiz_category_wrong, saved.quiz_category_wrong,
               sizeof(saved.quiz_category_wrong));
        restored.quiz_fixed_correct = saved.quiz_fixed_correct;
        restored.quiz_fixed_wrong = saved.quiz_fixed_wrong;
    }
    if (version >= 4) {
        memcpy(restored.level, saved.level, sizeof(saved.level));
        memcpy(restored.growth, saved.growth, sizeof(saved.growth));
        memcpy(restored.other_stats, saved.other_stats,
               sizeof(saved.other_stats));
        if (version <= 9) for (i = 0; i < 3; ++i) {
            NpeCharacter character;
            unsigned old_level, corrected, earned;
            unsigned id = restored.party_ids[i];
            if (id < 1 || id > 3 ||
                npe_character(file, id - 1, &character)) return -1;
            /* V4..V9 encoded a level derived from file byte +12, which
               is the source character ID. Preserve levels earned in the
               old C runtime while rebasing on the native +35 level. */
            old_level = character.raw[12];
            if (old_level == character.level || saved.level[i] < old_level)
                continue;
            earned = saved.level[i] - old_level;
            corrected = character.level + earned;
            if (corrected > 99) corrected = 99;
            restored.level[i] = (uint8_t)corrected;
            if (earned)
                restored.experience_to_next[i] = (uint16_t)(
                    3u * (corrected * corrected + 5u * corrected) - 8u);
        }
    }
    /* Older C saves misidentified this byte as first skill. */
    for (i = 0; i < 3; ++i) restored.damage_floor_stat[i] = restored.level[i];
    {
        NpeMap map;
        if (npe_map(file, restored.map_number - 1, &map) ||
            npe_world_load_object_records(&restored, &map) ||
            npe_world_load_tiles(&restored, &map)) return -1;
        if (version >= 14) {
            if (version<23 && saved.tile_change_applied >> restored.tile_change_count) return -1;
            for (i = 0; i < saved.tile_override_count; ++i) {
                unsigned j, known = 0;
                for (j = 0; j < i; ++j)
                    if (saved.tile_override_indices[i] == saved.tile_override_indices[j]) return -1;
                for (j = 0; j < restored.tile_change_count; ++j)
                    for (unsigned k = 0; k < restored.tile_changes[j].count; ++k)
                        if (saved.tile_override_indices[i] == restored.tile_changes[j].indices[k] &&
                            saved.tile_override_tiles[i] == restored.tile_changes[j].tiles[k]) known = 1;
                for(j=0;j<8;++j){
                    NpeMapObject object;
                    if(npe_world_object(&restored,&map,j,&object))return -1;
                    /* Own V19+ saves retain live pickup cells even when
                       native bulk initialization skips IDs101..200. */
                    if(object.id && object.id<=200 &&
                       restored.object_done[object.id-1]==1 && saved.tile_override_tiles[i]==53 &&
                       saved.tile_override_indices[i]==object.y*map.width+object.x)known=1;
                }
                if (!known && version<23) return -1;
            }
            restored.tile_change_applied = saved.tile_change_applied;
            restored.tile_override_count = saved.tile_override_count;
            memcpy(restored.tile_override_indices, saved.tile_override_indices, sizeof(saved.tile_override_indices));
            memcpy(restored.tile_override_tiles, saved.tile_override_tiles, sizeof(saved.tile_override_tiles));
            /*Old C saves never recorded pickup terrain. Reconstruct those
              cells when importing them; V19 preserves the exact live order. */
            if(version<19){
                uint16_t indices[NPE_TILE_OVERRIDES],tiles[NPE_TILE_OVERRIDES];
                memcpy(indices,restored.tile_override_indices,sizeof(indices));
                memcpy(tiles,restored.tile_override_tiles,sizeof(tiles));
                unsigned count=restored.tile_override_count;
                if(npe_world_load_tiles(&restored,&map))return -1;
                for(i=0;i<count;++i){
                    unsigned j;
                    for(j=0;j<restored.tile_override_count;++j)if(restored.tile_override_indices[j]==indices[i])break;
                    if(j==restored.tile_override_count)++restored.tile_override_count;
                    restored.tile_override_indices[j]=indices[i];restored.tile_override_tiles[j]=tiles[i];
                }
                restored.tile_change_applied=saved.tile_change_applied;
            }
        }
    }
    for(i=0;i<restored.tile_change_count;++i)
        restored.tile_record_bytes[64u*i+62]=(uint8_t)((restored.tile_change_applied>>i)&1u);
    if(version>=23){
        restored.terrain_snapshot_ready=1;restored.terrain_snapshot_width=saved.terrain_snapshot_width;
        restored.terrain_snapshot_height=saved.terrain_snapshot_height;restored.terrain_snapshot_count=saved.terrain_snapshot_count;
        restored.terrain_buffer_snapshot_ready=saved.terrain_buffer_snapshot_ready;
        unsigned stored=saved.terrain_snapshot_count;
        if(saved.terrain_buffer_snapshot_ready && stored<NPE_MAP_BUFFER_WORDS)stored=NPE_MAP_BUFFER_WORDS;
        memcpy(restored.terrain_snapshot_cells,saved.terrain_snapshot_cells,2u*stored);
        memcpy(restored.tile_record_bytes,saved.tile_record_bytes,192);
    }
    /* Cold host decode has no live audio device. Native menu restoration
       uses npe_world_restore; each controller owns its playback requests. */
    npe_world_sound_stop(&restored);
    *world = restored;
    if(npe_rpg_runtime_owner(file)==world && npe_rpg_runtime_map_load(file,world,1))return -1;
    return 0;
}

int npe_world_restore(const NpeFile *file, NpeWorld *world,
                      const char *path) {
    NpeWorld restored;
    uint8_t battle_records[6][148];
    if (!world || npe_world_resident_records_read(world,UINT32_C(0x526450),
            &battle_records[0][0],sizeof(battle_records)) ||
        npe_world_load(file, &restored, path)) return -1;
    /*3C:3DC2 restores the map bank, not6450..67C7. Warm own-slot restore
      keeps the live battle image; cold own-slot load keeps its cold image. */
    memcpy(restored.battle_record_residue,battle_records,sizeof(battle_records));
    restored.battle_record_party_count=world->battle_record_party_count;
    /*3C:40D4..4100 restores69C6..6B81; the map NPC records at6CA2
       are rebuilt from the current NPE, not restored patrol coordinates.
       Keep cold host decode compatible with diagnostic NPESAV15 records. */
    memset(restored.npc_override_x, 255, sizeof(restored.npc_override_x));
    memset(restored.npc_override_y, 255, sizeof(restored.npc_override_y));
    memset(restored.npc_override_direction, 255, sizeof(restored.npc_override_direction));
    restored.lcd_background = world->lcd_background;
    /* Session state is not in the original slot image. Preserve it on
       running UI loads; cold load above supplies its cold initial state. */
    restored.quiz_answer_count = world->quiz_answer_count;
    restored.quiz_wrong_count = world->quiz_wrong_count;
    restored.quiz_previous_index = world->quiz_previous_index;
    restored.quiz_category = world->quiz_category;
    restored.quiz_question_index = world->quiz_question_index;
    restored.quiz_input_type = world->quiz_input_type;
    restored.quiz_input_length = world->quiz_input_length;
    memcpy(restored.quiz_input, world->quiz_input, 32);
    restored.quiz_input[32] = 0;
    memcpy(restored.battle_selected_ids,world->battle_selected_ids,sizeof(restored.battle_selected_ids));
    restored.battle_formation_count=world->battle_formation_count;
    restored.battle_scripted_entry=world->battle_scripted_entry;
    restored.battle_menu_row=world->battle_menu_row;
    restored.battle_item_row=world->battle_item_row;
    restored.battle_actor=world->battle_actor;
    restored.battle_menu_previous_kind=world->battle_menu_previous_kind;
    restored.battle_menu_phase=world->battle_menu_phase;
    restored.battle_status_member=world->battle_status_member;
    restored.battle_resource_flags=world->battle_resource_flags;
    restored.battle_display_target=world->battle_display_target;
    restored.battle_selection_item_type=world->battle_selection_item_type;
    restored.battle_selection_magic_type=world->battle_selection_magic_type;
    restored.battle_selection_type_copy=world->battle_selection_type_copy;
    restored.battle_action_ready=world->battle_action_ready;
    restored.battle_execution_slot=world->battle_execution_slot;
    restored.property_page=world->property_page;
    restored.property_member=world->property_member;
    restored.equipment_view_slot=world->equipment_view_slot;
    restored.menu_party_count=world->menu_party_count;
    restored.equipment_description_item=world->equipment_description_item;
    restored.item_record_id=world->item_record_id;
    memcpy(restored.item_record_bytes,world->item_record_bytes,142);
    memcpy(restored.item_record_raw,world->item_record_raw,41);
    restored.skill_record_id=world->skill_record_id;
    memcpy(restored.skill_record_bytes,world->skill_record_bytes,144);
    memcpy(restored.skill_record_raw,world->skill_record_raw,42);
    restored.item_description_length=world->item_description_length;
    restored.equipment_description_revision=world->equipment_description_revision;
    /* Original map loader overwrites present group6 records without
       clearing unused resident slots. Warm load starts from live cache. */
    memcpy(restored.party_events, world->party_events, sizeof(restored.party_events));
    memcpy(restored.party_event_padding, world->party_event_padding, sizeof(restored.party_event_padding));
    /* NPCs are rebuilt by the map loader too; retain each live slot's
       untouched padding byte rather than the cold constructor's zero. */
    if(!restored.terrain_snapshot_ready)memcpy(restored.tile_record_bytes,world->tile_record_bytes,192);
    memcpy(restored.npc_records, world->npc_records, sizeof(restored.npc_records));
    memcpy(restored.exit_records,world->exit_records,sizeof(restored.exit_records));
    memcpy(restored.monster_records,world->monster_records,sizeof(restored.monster_records));
    memcpy(restored.chase_records,world->chase_records,sizeof(restored.chase_records));
    memcpy(restored.chase_x,world->chase_x,sizeof(restored.chase_x));
    memcpy(restored.chase_y,world->chase_y,sizeof(restored.chase_y));
    memcpy(restored.chase_cooldown,world->chase_cooldown,sizeof(restored.chase_cooldown));
    memcpy(restored.npc_move_events, world->npc_move_events, sizeof(restored.npc_move_events));
    memcpy(restored.npc_move_padding, world->npc_move_padding, sizeof(restored.npc_move_padding));
    memcpy(restored.script_walk_padding, world->script_walk_padding, sizeof(restored.script_walk_padding));
    memcpy(restored.script_walk_events, world->script_walk_events, sizeof(restored.script_walk_events));
    memcpy(restored.object_done+100,world->object_done+100,100);
    memcpy(restored.story_events, world->story_events, sizeof(restored.story_events));
    memcpy(restored.story_event_padding, world->story_event_padding, sizeof(restored.story_event_padding));
    memcpy(restored.money_events, world->money_events, sizeof(restored.money_events));
    memcpy(restored.money_event_padding, world->money_event_padding, sizeof(restored.money_event_padding));
    memcpy(restored.item_events, world->item_events, sizeof(restored.item_events));
    memcpy(restored.teleport_events, world->teleport_events, sizeof(restored.teleport_events));
    memcpy(restored.teleport_padding, world->teleport_padding, sizeof(restored.teleport_padding));
    memcpy(restored.skill_events, world->skill_events, sizeof(restored.skill_events));
    memcpy(restored.skill_event_padding,world->skill_event_padding,sizeof(restored.skill_event_padding));
    memcpy(restored.scene_event_records,world->scene_event_records,sizeof(restored.scene_event_records));
    memcpy(restored.choice_event_records,world->choice_event_records,sizeof(restored.choice_event_records));
    memcpy(restored.battle_event_records,world->battle_event_records,sizeof(restored.battle_event_records));
    restored.party_adjust_event=world->party_adjust_event;
    {
        NpeMap map;
        if (npe_map(file, restored.map_number - 1, &map) ||
            npe_world_load_exit_records(&restored, &map) ||
            npe_world_load_npc_records(&restored, &map) ||
            npe_world_load_map_encounter_records(&restored, &map) ||
            npe_world_load_party_events(&restored, &map) ||
            npe_world_load_npc_move_events(&restored, &map) ||
            npe_world_load_script_walk_events(&restored, &map) ||
            npe_world_load_walk_triggers(&restored, &map) ||
            npe_world_load_story_event_cache(&restored, &map) ||
            npe_world_load_money_events(&restored, &map) ||
            npe_world_load_item_events(&restored, &map) ||
            npe_world_load_teleport_events(&restored, &map) ||
            npe_world_load_skill_events(&restored, &map) ||
            npe_world_load_scene_events(&restored, &map) ||
            npe_world_load_choice_events(&restored, &map) ||
            npe_world_load_battle_events(&restored, &map) ||
            npe_world_load_random_enemy_records(&restored, &map) ||
            npe_world_load_party_adjust_event(&restored, &map) ||
            (!restored.terrain_snapshot_ready && npe_world_load_tile_records(&restored,&map))) return -1;
        for(unsigned i=0;!restored.terrain_snapshot_ready && i<restored.tile_change_count;++i)
            restored.tile_record_bytes[64u*i+62]=(uint8_t)((restored.tile_change_applied>>i)&1u);
    }
    /* The original slot codec restores RPG data, not the resident RNG or
       music flag/slot. Keep the live adapters too: rebinding would replay
       the pending request even when the title controller requests nothing. */
    restored.random_state = world->random_state;
    restored.music_enabled = world->music_enabled;
    restored.system_menu_stage=world->system_menu_stage;
    restored.music_choice=world->music_choice;
    restored.map_init_pending=world->map_init_pending;
    restored.sound_range_observer = world->sound_range_observer;
    restored.map_music_slot = world->map_music_slot;
    restored.sound_section = world->sound_section;
    restored.sound_loop = world->sound_loop;
    restored.sound_observer = world->sound_observer;
    restored.sound_context = world->sound_context;
    /* Original3C:3DC2..4396 restores RPG records and mode1 but leaves
       resident7AC4/7A84 untouched, including title loads. */
    restored.exit_choice = world->exit_choice;
    restored.exit_slot = world->exit_slot;
    /* Resident7938/7A68 and shop UI words/bytes are outside the slot codec.
       Preserve them independently of the C catalog/activity flags: loading
       returns to map mode, and the next shop entry rebuilds its item indices. */
    /*7936 is outside the original slot codec, just like7938. Cold own
      decode still reads its stored word; a running UI load retains it. */
    restored.story_event_id = world->story_event_id;
    restored.event_context_id = world->event_context_id;
    restored.mode2_context = world->mode2_context;
    /* Shared resident7A0A/7A82 survive slot loads. Activity flags stay cold:
       the next NPC/story entry reloads its own continuation. */
    restored.mode2_followup_mode = world->mode2_followup_mode;
    restored.mode2_followup_event_id = world->mode2_followup_event_id;
    /* Original title-slot loads retain these paired-condition controls;
       they are session RAM and do not extend our own save payload. */
    restored.npc_guard_index = world->npc_guard_index;
    restored.npc_guard_word_index = world->npc_guard_word_index;
    restored.npc_guard_a = world->npc_guard_a;
    restored.npc_guard_b = world->npc_guard_b;
    restored.text_file_position=world->text_file_position;
    memcpy(restored.text_rect,world->text_rect,sizeof(restored.text_rect));
    restored.text_offset=world->text_offset;
    restored.text_more=world->text_more;restored.text_style=world->text_style;
    restored.shop_ui = world->shop_ui;
    restored.shop_kind = world->shop_kind;
    restored.shop_transaction_sell = world->shop_transaction_sell;
    restored.shop_scan_first = world->shop_scan_first;
    restored.shop_scan_last = world->shop_scan_last;
    restored.shop_inventory_only = world->shop_inventory_only;
    memcpy(restored.shop_catalog_present,world->shop_catalog_present,255);
    restored.shop_catalog_ready = world->shop_catalog_ready;
    restored.shop_quantity = world->shop_quantity;
    restored.shop_choice = world->shop_choice;
    restored.shop_row = world->shop_row;
    restored.inventory_column = world->inventory_column;
    restored.inventory_prefix_word = world->inventory_prefix_word;
    restored.shop_first_item = world->shop_first_item;
    restored.shop_last_item = world->shop_last_item;
    restored.shop_selected_item = world->shop_selected_item;
    restored.shop_description_start = world->shop_description_start;
    restored.shop_description_end = world->shop_description_end;
    restored.battle_animation_observer = world->battle_animation_observer;
    restored.battle_animation_context = world->battle_animation_context;
    *world = restored;
    if(npe_rpg_runtime_owner(file)==world && npe_rpg_runtime_map_load(file,world,1))return -1;
    return 0;
}
