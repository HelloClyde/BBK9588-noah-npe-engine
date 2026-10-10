#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif

static NpeWorld *live;
static NpeSoundPlayer *player;
static unsigned title_entry, enabled, slot, saving, step, stage, resumes, restores;
static uint32_t resident_seed;
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
#include "shop_state_load_native.h"
static unsigned shop_load_index;
static void seed_shop(NpeWorld *w,const ShopLoadCase *s){
    w->event_context_id=(uint16_t)s->context;w->mode2_context=(uint8_t)s->npcContext;
    w->shop_selected_item=(uint16_t)s->selected;w->shop_first_item=(uint16_t)s->first;w->shop_last_item=(uint16_t)s->last;
    w->shop_row=(uint8_t)s->row;w->shop_ui=(uint8_t)s->ui;w->shop_quantity=(uint8_t)s->quantity;w->shop_choice=(uint8_t)s->choice;
    w->shop_description_start=(uint16_t)s->descStart;w->shop_description_end=(uint16_t)s->descEnd;
    /* C activity/index flags are not resident7A0E. A successful load returns
       to map mode and invalidates the old catalog, even if these are set. */
    w->shop_kind=w->shop_ready=(uint8_t)s->kind;w->shop_list_ready=1;
}
static void check_shop(const NpeWorld *w,const ShopLoadCase *s){
    assert(w->event_context_id==s->context&&w->mode2_context==s->npcContext&&
           w->shop_selected_item==s->selected&&w->shop_first_item==s->first&&w->shop_last_item==s->last&&
           w->shop_row==s->row&&w->shop_ui==s->ui&&w->shop_quantity==s->quantity&&w->shop_choice==s->choice&&
           w->shop_description_start==s->descStart&&w->shop_description_end==s->descEnd);
    assert(w->shop_kind==s->kind&&!w->shop_ready&&!w->shop_list_ready&&w->game_mode==s->mode);
}
#elif defined(NPE_EXIT_STATE_LOAD_MATRIX)
#include "exit_state_load_native.h"
static unsigned exit_load_index;
#endif
static int probe_wait(SDL_Event *, int);
static int probe_logical_wait(SDL_Event *,int,const char *);
static int popup_wait(SDL_Event *);
static void probe_select(NpeSoundPlayer *, NpeWorld *);
static void probe_resume(NpeWorld *);
static int probe_restore(const NpeFile *, NpeWorld *, const char *);
#define SDL_WaitEventTimeout(event,timeout) probe_logical_wait(event,timeout,caller)
#define SDL_WaitEvent popup_wait
#define npe_sound_select probe_select
#define npe_world_sound_map_resume probe_resume
#define npe_world_restore probe_restore
#define main npe_app_main
#include "../main.c"
#undef main
#undef npe_world_restore
#undef npe_world_sound_map_resume
#undef npe_sound_select
#undef SDL_WaitEvent
#undef SDL_WaitEventTimeout

static void probe_select(NpeSoundPlayer *p, NpeWorld *w) {
    player = p; live = w;
    npe_sound_select(p, w);
    SDL_PauseAudioDevice(p->device, 1);
}
static void probe_resume(NpeWorld *w) {
    ++resumes;
    npe_world_sound_map_resume(w);
}
static int probe_restore(const NpeFile *file, NpeWorld *w, const char *path) {
    NpeSoundObserver observer = w->sound_observer;
    void *context = w->sound_context;
    assert(w->random_state == resident_seed && w->music_enabled == enabled &&
           w->map_music_slot == slot && w->story_progress == 4);
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
    seed_shop(w,&shop_load_cases[shop_load_index]);
#endif
    assert(!npe_world_restore(file, w, path));
    assert(w->random_state == resident_seed && w->music_enabled == enabled &&
           w->map_music_slot == slot && w->story_progress == 1);
#ifdef NPE_EXIT_STATE_LOAD_MATRIX
    assert(w->exit_choice==exit_load_cases[exit_load_index].choice &&
           w->exit_slot==exit_load_cases[exit_load_index].slot);
#endif
    assert(w->sound_observer == observer && w->sound_context == context);
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
    check_shop(w,&shop_load_cases[shop_load_index]);
#endif
    ++restores;
    return 0;
}
static int popup_wait(SDL_Event *event) {
    assert(!enabled && stage == 0);
    memset(event, 0, sizeof(*event)); event->type = SDL_KEYDOWN;
    event->key.keysym.sym = SDLK_RETURN; /* Original opposite selection: off. */
    return 1;
}
static int probe_logical_wait(SDL_Event *event,int timeout,const char *caller){
    return !strcmp(caller,"game_WaitEvent")?popup_wait(event):probe_wait(event,timeout);
}
static int probe_wait(SDL_Event *event, int timeout) {
    static const SDL_Keycode mute[] = {SDLK_F4,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,
        SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN};
    static const SDL_Keycode system_load[] = {SDLK_F4,SDLK_DOWN,SDLK_DOWN,
        SDLK_DOWN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN};
    static const SDL_Keycode system_save[] = {SDLK_F4,SDLK_DOWN,SDLK_DOWN,
        SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_RETURN};
    static const SDL_Keycode title_load[] = {SDLK_DOWN,SDLK_RETURN,SDLK_RETURN};
    (void)timeout;
    memset(event, 0, sizeof(*event)); event->type = SDL_KEYDOWN;
    if (!stage && !enabled && step < sizeof(mute)/sizeof(*mute)) {
        event->key.keysym.sym = mute[step++]; return 1;
    }
    if (!stage) {
        assert(live->game_mode == 1 && live->music_enabled == enabled);
        live->story_progress = 4;
        live->map_music_slot = (uint8_t)slot;
        live->random_state = resident_seed;
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
        live->story_event_id=shop_load_cases[shop_load_index].event;
#endif
#ifdef NPE_EXIT_STATE_LOAD_MATRIX
        live->story_event_id=exit_load_cases[exit_load_index].event;
        live->exit_choice=(uint8_t)exit_load_cases[exit_load_index].choice;
        live->exit_slot=(uint8_t)exit_load_cases[exit_load_index].slot;
#endif
        if (title_entry) {
            live->game_mode = 7;
            npe_world_sound_stop(live);
        } else if (enabled) npe_world_sound_map_resume(live);
        /* Paused device makes request restarts observable without timing races. */
        player->cursor = 114;
        resumes = restores = step = 0; stage = 1;
    }
    const SDL_Keycode *keys = title_entry ? title_load : saving ? system_save : system_load;
    unsigned count = title_entry ? sizeof(title_load)/sizeof(*keys) :
                     saving ? sizeof(system_save)/sizeof(*keys) : sizeof(system_load)/sizeof(*keys);
    if (step < count) { event->key.keysym.sym = keys[step++]; return 1; }
    assert(live->game_mode == 1 && live->music_enabled == enabled && live->map_music_slot == slot);
    assert(live->random_state == resident_seed && live->story_progress == (saving ? 4 : 1));
    assert(restores == !saving && resumes == !title_entry);
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
    const ShopLoadCase *s=&shop_load_cases[shop_load_index];check_shop(live,s);
    assert(live->story_event_id==s->event&&live->map_number==s->map&&live->story_progress==s->progress);
#endif
#ifdef NPE_EXIT_STATE_LOAD_MATRIX
    assert(live->exit_choice==exit_load_cases[exit_load_index].choice &&
           live->exit_slot==exit_load_cases[exit_load_index].slot &&
           live->game_mode==exit_load_cases[exit_load_index].mode &&
           live->story_event_id==exit_load_cases[exit_load_index].event &&
           live->map_number==exit_load_cases[exit_load_index].map &&
           live->story_progress==exit_load_cases[exit_load_index].progress);
#endif
    if (enabled && !title_entry) {
        assert(player->selected_section == (int)(20 + slot) &&
               player->active == NPE_SOUND_TRACKS && player->continuation==NPE_SOUND_CALLBACK_MAP && player->loop && !player->cursor);
    } else {
        assert(player->selected_section == 0 && player->active == -1);
        /* Title opening explicitly stops; a muted system return emits nothing. */
        assert(player->cursor == (title_entry ? 0 : 114));
    }
    event->type = SDL_QUIT; stage = 2; return 1;
}
int main(int argc, char **argv) {
    NpeFile file; NpeWorld saved; uint8_t *bytes; size_t size; char error[128];
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
    const char *copy = "tmp/npe-shop-state-restore.npe";
#elif defined(NPE_EXIT_STATE_LOAD_MATRIX)
    const char *copy = "tmp/npe-exit-state-restore.npe";
#else
    const char *copy = "tmp/npe-live-restore.npe";
#endif
    assert(argc == 2 && !read_file(argv[1], &bytes, &size));
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
    memcpy(bytes + 0x0c, "SHOP-LOAD-01", 12);
#elif defined(NPE_EXIT_STATE_LOAD_MATRIX)
    memcpy(bytes + 0x0c, "EXIT-LOAD-01", 12);
#else
    memcpy(bytes + 0x0c, "LIVE-LOAD-01", 12);
#endif
    FILE *out = fopen(copy, "wb");
    assert(out && fwrite(bytes, 1, size, out) == size && !fclose(out));
    assert(!npe_parse(&file, bytes, size, error, sizeof(error)));
    char *path = game_save_path(&file, 0); assert(path);
    assert(!SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) &&
           !SDL_setenv("SDL_AUDIODRIVER", "dummy", 1));
    unsigned cases = 0;
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
    for(shop_load_index=0;shop_load_index<sizeof(shop_load_cases)/sizeof(*shop_load_cases);++shop_load_index){
        const ShopLoadCase *s=&shop_load_cases[shop_load_index];title_entry=s->title;saving=enabled=slot=0;
        assert(!npe_world_init(&file,&saved,9,29,13));saved.story_progress=1;saved.story_event_id=1;
        assert(!npe_world_save(&file,&saved,path));
        NpeWorld cold,numeric=saved;numeric.story_progress=4;seed_shop(&numeric,s);
        assert(!npe_world_load(&file,&cold,path)&&!cold.event_context_id&&!cold.mode2_context&&
               !cold.shop_first_item&&!cold.shop_last_item&&!cold.shop_selected_item&&!cold.shop_ui&&
               !cold.shop_row&&!cold.shop_quantity&&!cold.shop_choice&&!cold.shop_description_start&&!cold.shop_description_end);
        NpeWorld unchanged=numeric;
        assert(npe_world_restore(&file,&numeric,"tmp/shop-state-missing-slot.npes")<0&&!memcmp(&unchanged,&numeric,sizeof(numeric)));
        const char *bad="tmp/shop-state-truncated.npes";FILE *broken=fopen(bad,"wb");
        assert(broken&&fwrite("NPESAV15",1,8,broken)==8&&!fclose(broken));
        assert(npe_world_restore(&file,&numeric,bad)<0&&!memcmp(&unchanged,&numeric,sizeof(numeric))&&!remove(bad));
        assert(!npe_world_restore(&file,&numeric,path)&&numeric.story_progress==1);check_shop(&numeric,s);
        stage=step=resumes=restores=0;resident_seed=0;
        char *args[]={"npe-sdl","--walk-preview","9,29,13",(char *)copy,NULL};
        assert(!run(4,args)&&stage==2);++cases;
        fprintf(stderr,"C/SDL shop resident %s %s passed\n",title_entry?"title":"system",s->name);
    }
#elif defined(NPE_EXIT_STATE_LOAD_MATRIX)
    for(exit_load_index=0;exit_load_index<sizeof(exit_load_cases)/sizeof(*exit_load_cases);++exit_load_index){
        title_entry=exit_load_cases[exit_load_index].title; saving=enabled=slot=0;
        assert(!npe_world_init(&file,&saved,9,29,13));
        saved.story_progress=1;saved.story_event_id=1;saved.random_state=123;
        assert(!npe_world_save(&file,&saved,path));
        NpeWorld cold, numeric=saved;
        numeric.story_progress=4;numeric.exit_choice=(uint8_t)exit_load_cases[exit_load_index].choice;
        numeric.exit_slot=(uint8_t)exit_load_cases[exit_load_index].slot;
        assert(!npe_world_load(&file,&cold,path)&&!cold.exit_choice&&!cold.exit_slot);
        NpeWorld unchanged=numeric;
        assert(npe_world_restore(&file,&numeric,"tmp/exit-state-missing-slot.npes")<0);
        assert(!memcmp(&unchanged,&numeric,sizeof(numeric)));
        assert(!npe_world_restore(&file,&numeric,path)&&numeric.story_progress==1&&
               numeric.exit_choice==exit_load_cases[exit_load_index].choice&&numeric.exit_slot==exit_load_cases[exit_load_index].slot);
        stage=step=resumes=restores=0;resident_seed=0;
        char *args[]={"npe-sdl","--walk-preview","9,29,13",(char *)copy,NULL};
        assert(!run(4,args)&&stage==2);++cases;
        fprintf(stderr,"C/SDL exit resident %s choice%u slot%u passed\n",title_entry?"title":"system",
                exit_load_cases[exit_load_index].choice,exit_load_cases[exit_load_index].slot);
    }
#else
    for (saving = 0; saving < 2; ++saving)
        for (title_entry = 0; title_entry < (saving ? 1u : 2u); ++title_entry)
            for (enabled = 0; enabled < 2; ++enabled)
                for (slot = 0; slot < 3; ++slot) {
                    assert(!npe_world_init(&file, &saved, 9, 29, 13));
                    saved.story_progress = 1; saved.random_state = 123;
                    assert(!npe_world_save(&file, &saved, path));
                    stage = step = resumes = restores = 0;
                    resident_seed = slot; /* Includes state zero. */
                    char *args[] = {"npe-sdl","--walk-preview","9,29,13",(char *)copy,NULL};
                    assert(!run(4, args) && stage == 2);
                    fprintf(stderr,"SDL live %s enabled%u slot%u passed\n",
                            saving ? "save" : title_entry ? "title-load" : "system-load",enabled,slot);
                    ++cases;
                }
#endif
    assert(!remove(path) && !remove(copy)); free(path); npe_free(&file);
#ifdef NPE_SHOP_STATE_LOAD_MATRIX
    printf("%u original shop/NPC resident warm-load cases, cold decode and failed-load preservation passed\n",cases);
#elif defined(NPE_EXIT_STATE_LOAD_MATRIX)
    printf("%u original resident exit-choice warm-load cases, cold initialization and failure preservation passed\n",cases);
#else
    printf("%u actual SDL live save/load controller and retained music/RNG cases passed\n",cases);
#endif
    return 0;
}
