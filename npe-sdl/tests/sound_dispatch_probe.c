#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound_choice_native.h"

typedef struct { unsigned count, section[16]; int loop[16]; } Requests;
static void observe(void *context, unsigned section, int loop) {
    Requests *r = context;
    assert(r->count < 16);
    r->section[r->count] = section; r->loop[r->count++] = loop;
}
static uint32_t advance(uint32_t state) {
    return state * UINT32_C(0x514f111f) + 1u;
}
static unsigned roll(uint32_t state) { return (advance(state) >> 16) & 32767u; }
static uint32_t seed_for(unsigned value) {
    for (uint32_t seed = 0; seed < 1000000; ++seed)
        if (roll(seed) == value) return seed;
    assert(0); return 0;
}
static void choices(const NpeFile *file) {
    for (unsigned i = 0; i < sizeof(sound_choice_native)/sizeof(sound_choice_native[0]); ++i) {
        NpeWorld world; Requests requests = {0};
        uint32_t seed = seed_for(sound_choice_native[i].roll);
        assert(!npe_world_init(file, &world, 9, 29, 13));
        npe_world_sound_bind(&world, observe, &requests);
        requests.count = 0; npe_world_seed(&world, seed);
        npe_world_sound_map_loaded(&world);
        assert(world.random_state == advance(seed));
        assert(world.map_music_slot == sound_choice_native[i].map_slot);
        assert(requests.count == 1 && requests.section[0] == 20+sound_choice_native[i].map_slot && requests.loop[0]);
        requests.count = 0; npe_world_seed(&world, seed);
        npe_world_sound_battle_start(&world);
        assert(world.random_state == advance(seed));
        assert(requests.count == 1 && requests.section[0] == 23+sound_choice_native[i].battle_slot && requests.loop[0]);
        npe_world_music_set(&world, 0);
        assert(requests.count == 2 && requests.section[1] == 0 && !requests.loop[1]);
        requests.count = 0; npe_world_seed(&world, seed);
        npe_world_sound_map_loaded(&world); npe_world_sound_battle_start(&world);
        assert(world.random_state == seed && !requests.count);
        assert(world.map_music_slot == sound_choice_native[i].map_slot);
        npe_world_music_set(&world, 1); npe_world_music_set(&world, 1);
        assert(world.random_state == seed && requests.count == 2);
        assert(requests.section[0] == 20+world.map_music_slot && requests.section[1] == requests.section[0]);
        npe_world_sound_bind(&world, NULL, NULL);
        npe_world_sound_battle_start(&world);
        assert(world.random_state == advance(seed)); /* No device/observer still draws. */
    }
}
static void battle_entries(const NpeFile *file) {
    NpeWorld enabled, disabled; Requests on = {0}, off = {0};
    assert(!npe_world_init(file, &enabled, 9, 29, 13));
    npe_world_seed(&enabled, 19); enabled.game_mode = 3; enabled.story_event_id = 1;
    disabled = enabled; disabled.music_enabled = 0;
    npe_world_sound_bind(&enabled, observe, &on); npe_world_sound_bind(&disabled, observe, &off);
    on.count = off.count = 0;
    assert(npe_world_prepare_scripted_battle(file, &enabled) == 1);
    assert(npe_world_prepare_scripted_battle(file, &disabled) == 1);
    assert(enabled.random_state == advance(disabled.random_state));
    assert(on.count == 1 && off.count == 0 && on.section[0] == 23+roll(disabled.random_state)%60/10);
    assert(!memcmp(enabled.battle_enemy_ids, disabled.battle_enemy_ids, sizeof(enabled.battle_enemy_ids)));

    /* Force the normal encounter threshold, leaving formation RNG untouched. */
    assert(!npe_world_init(file, &enabled, 9, 27, 13));
    enabled.encounter_steps = 1000; enabled.story_progress = 6;
    npe_world_seed(&enabled, 19); disabled = enabled; disabled.music_enabled = 0;
    npe_world_sound_bind(&enabled, observe, &on); npe_world_sound_bind(&disabled, observe, &off);
    on.count = off.count = 0;
    assert(npe_world_step(file, &enabled, 1) == NPE_MOVE_EVENT);
    assert(npe_world_step(file, &disabled, 1) == NPE_MOVE_EVENT);
    assert(enabled.game_mode == 3 && disabled.game_mode == 3);
    assert(enabled.random_state == advance(disabled.random_state));
    assert(on.count == 1 && off.count == 0 && on.section[0] == 23+roll(disabled.random_state)%60/10);
    assert(!memcmp(enabled.battle_enemy_ids, disabled.battle_enemy_ids, sizeof(enabled.battle_enemy_ids)));

    /* First current-NPE chase record, with isolated contact coordinates. */
    unsigned found = 0;
    for (unsigned m = 0; m < file->offset_table_records[8] && !found; ++m) {
        NpeMap map; NpeMapChase chase;
        if (npe_map(file, m, &map) || npe_map_chase(&map, 0, &chase) != 1 || !chase.enemy_id) continue;
        assert(!npe_world_init(file, &enabled, m+1, 7, 1));
        enabled.story_progress = chase.min_progress ? chase.min_progress : 1;
        enabled.chase_x[0] = enabled.x-1; enabled.chase_y[0] = enabled.y+1;
        npe_world_seed(&enabled, 19); disabled = enabled; disabled.music_enabled = 0;
        npe_world_sound_bind(&enabled, observe, &on); npe_world_sound_bind(&disabled, observe, &off);
        on.count = off.count = 0;
        assert(npe_world_update_chase(file, &enabled) == 1);
        assert(npe_world_update_chase(file, &disabled) == 1);
        assert(enabled.random_state == advance(disabled.random_state));
        assert(on.count == 1 && off.count == 0 && on.section[0] == 23+roll(disabled.random_state)%60/10);
        assert(enabled.battle_enemy_ids[0] == chase.enemy_id); found = 1;
    }
    assert(found);
}
static void map_exit(const NpeFile *file) {
    NpeWorld world; NpeMap map; NpeMapExit exit; Requests requests = {0};
    assert(!npe_map(file, 0, &map) && !npe_map_exit(&map, 0, &exit));
    unsigned x = exit.x, y = exit.y, direction;
    if (x) { --x; direction = 1; } else { ++x; direction = 3; }
    assert(!npe_world_init(file, &world, 1, x, y));
    world.story_progress = exit.min_progress;
    npe_world_seed(&world, 19); npe_world_sound_bind(&world, observe, &requests); requests.count = 0;
    assert(npe_world_step(file, &world, direction) == NPE_MOVE_EXIT);
    assert(world.map_number == exit.destination_map && world.random_state == advance(19));
    assert(requests.count == 1 && requests.section[0] == 20+roll(19)%3);
}
static void load_is_silent(const NpeFile *file) {
    NpeWorld world, restored; Requests requests = {0};
    const char *path = "tmp/sound-dispatch-save.npesav";
    assert(!npe_world_init(file, &world, 9, 29, 13));
    npe_world_seed(&world, 123); npe_world_sound_map_loaded(&world);
    assert(!npe_world_save(file, &world, path));
    assert(!npe_world_load(file, &restored, path));
    assert(restored.random_state == world.random_state && !restored.sound_section);
    npe_world_sound_bind(&restored, observe, &requests);
    assert(requests.count == 1 && !requests.section[0] && !requests.loop[0]);
    npe_world_music_set(&restored, 1);
    assert(restored.random_state == world.random_state && requests.count == 2 && requests.section[1] == 20+restored.map_music_slot);
    assert(!remove(path));
}
static void live_load(const NpeFile *file) {
    NpeWorld saved;
    const char *path = "tmp/sound-live-restore.npesav";
    assert(!npe_world_init(file, &saved, 9, 29, 13));
    saved.story_progress = 1;
    saved.random_state = 123;
    assert(!npe_world_save(file, &saved, path));
    for (unsigned title = 0; title < 2; ++title)
        for (unsigned enabled = 0; enabled < 2; ++enabled)
            for (unsigned slot = 0; slot < 3; ++slot) {
                NpeWorld live, before;
                Requests requests = {0};
                assert(!npe_world_init(file, &live, 9, 29, 13));
                live.story_progress = 4;
                live.game_mode = title ? 7 : 6;
                live.music_enabled = (uint8_t)enabled;
                live.map_music_slot = (uint8_t)slot;
                live.random_state = slot; /* Including resident seed zero. */
                npe_world_sound_bind(&live, observe, &requests);
                npe_world_sound_stop(&live);
                requests.count = 0;
                before = live;
                assert(npe_world_restore(file, &live, "tmp/absent-live-restore.npesav"));
                assert(!memcmp(&live, &before, sizeof(live)) && !requests.count);
                assert(!npe_world_restore(file, &live, path));
                assert(live.story_progress == 1 && live.game_mode == 1);
                assert(live.random_state == slot && live.music_enabled == enabled &&
                       live.map_music_slot == slot && live.sound_observer == observe &&
                       live.sound_context == &requests && !live.sound_section);
                assert(live.battle_animation_observer == before.battle_animation_observer &&
                       live.battle_animation_context == before.battle_animation_context);
                assert(!requests.count);
                if (!title) npe_world_sound_map_resume(&live);
                assert(requests.count == (!title && enabled));
                if (requests.count)
                    assert(requests.section[0] == 20 + slot && requests.loop[0]);
                assert(live.random_state == slot);
            }
    assert(!remove(path));
}
int main(int argc, char **argv) {
    assert(argc == 2); FILE *input = fopen(argv[1], "rb"); assert(input);
    assert(!fseek(input,0,SEEK_END)); long length = ftell(input); assert(length > 0 && !fseek(input,0,SEEK_SET));
    uint8_t *bytes = malloc((size_t)length); assert(bytes && fread(bytes,1,(size_t)length,input)==(size_t)length); fclose(input);
    NpeFile file; char error[128]; assert(!npe_parse(&file,bytes,(size_t)length,error,sizeof(error)));
    choices(&file); battle_entries(&file); map_exit(&file); load_is_silent(&file); live_load(&file);
    npe_free(&file);
    puts("15 original CPU choices, shared RNG/on-off, 3 battle entries, map exit, cold decode and 12 live loads passed");
    return 0;
}
