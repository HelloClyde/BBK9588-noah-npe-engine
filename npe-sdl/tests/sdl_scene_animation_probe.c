#include "../npe.h"
#include "../text.h"
#include "sound_choice_native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../sound_sdl.c"

typedef struct { const char *name; unsigned group,x,y,draw,erase,frames; } SceneCase;
static const SceneCase scenes[] = {
    {"opening",1,0,0,0,0,62},
    {"local-0-0",2,60,0,0,0,12},{"local-0-1",2,60,0,0,1,12},
    {"local-1-0",2,60,0,1,0,12},{"local-1-1",2,60,0,1,1,12},
    {"local-2-0",2,60,0,2,0,12},{"local-2-1",2,60,0,2,1,12},
    {"ending",3,0,0,0,0,37}
};
static const SceneCase *current;
static Uint32 fake_clock;
static unsigned commits, checked_commits, startup, startup_checked, title_checked;
static unsigned requests, requested[8];
static int requested_loop[8];
static NpeSoundPlayer *observed_sound;
static unsigned event_count,event_index;
static NpeWorld *startup_world;
static SDL_Event wait_events[3];
static Uint32 probe_ticks(void) { return fake_clock; }
static void probe_delay(Uint32 ms) { fake_clock += ms; }
static int probe_poll(SDL_Event *event) {
    if (event_index == event_count) return 0;
    *event = wait_events[event_index++]; return 1;
}
static int probe_wait(SDL_Event *event, int timeout) {
    (void)timeout; assert(startup && startup_checked==62 && title_checked);
    memset(event,0,sizeof(*event));event->type=SDL_QUIT;return 1;
}
static void probe_present(SDL_Renderer *renderer);
static int probe_title(const NpeFile *file,const NpeFont *font,unsigned selection,uint8_t *pixels);
static void probe_scene_start(NpeWorld *world,unsigned group);
static SDL_Window *probe_window(const char *title,int x,int y,int w,int h,Uint32 flags) {
    (void)w;(void)h;return SDL_CreateWindow(title,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_WaitEventTimeout probe_wait
#define SDL_CreateWindow probe_window
#define SDL_RenderPresent probe_present
#define npe_render_native_title_screen probe_title
#define npe_world_sound_scene_start probe_scene_start
#define main npe_app_main
#include "../main.c"
#undef main
#undef npe_render_native_title_screen
#undef npe_world_sound_scene_start
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_PollEvent
#undef SDL_Delay
#undef SDL_GetTicks

static uint32_t next_state(uint32_t state) { return state*UINT32_C(0x514f111f)+1u; }
static uint32_t seed_for(unsigned value) {
    for(uint32_t s=0;s<1000000;++s)if(((next_state(s)>>16)&32767u)==value)return s;
    assert(0);return 0;
}
static void observe(void *context,unsigned section,int loop) {
    (void)context;assert(requests<8);
    requested[requests]=section;requested_loop[requests++]=loop;
    if(observed_sound)assert(npe_sound_play_section(observed_sound,section,loop)==(section?0:-1));
}
static void read_pixels(const char *path,uint8_t *pixels) {
    FILE *f=fopen(path,"rb");assert(f);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT&&fgetc(f)==EOF&&!fclose(f));
}
static void check_present(SDL_Renderer *renderer) {
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];char path[128];
    snprintf(path,sizeof(path),"tmp/native-scene-%s-commit-%u.idx",current->name,commits);
    read_pixels(path,native);
    assert(!memcmp(native,last_pixel_frame,sizeof(native)));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned diff=0;
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        uint32_t expected=UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15);
        diff+=rgba[i]!=expected;
    }
    assert(!diff);++commits;++checked_commits;
}
static void probe_present(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);
    if(startup && current && startup_checked==62){
        /*48:0DB0/0DB4 commits the title clear after the opening service
          has stopped. It is not a63rd animation frame. */
        for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i)assert(!last_pixel_frame[i]);
        current=NULL;return;
    }
    if(current){check_present(renderer);if(startup){
        assert(startup_world&&startup_world->sound_section==16&&!startup_world->sound_loop);
        NpeSoundPlayer *player=startup_world->sound_context;
        assert(player&&player->device&&player->active==0&&!player->loop);
        ++startup_checked;
    }}
}
static void probe_scene_start(NpeWorld *world,unsigned group) {
    uint32_t before=world->random_state;
    npe_world_sound_scene_start(world,group);
    if(startup){assert(group==1&&world->music_enabled==1&&world->random_state==before);startup_world=world;}
}
static int probe_title(const NpeFile *file,const NpeFont *font,unsigned selection,uint8_t *pixels) {
    if(startup){assert(startup_checked==62&&startup_world&&!startup_world->sound_section);
        NpeSoundPlayer *player=startup_world->sound_context;assert(player&&player->active==-1);
        current=NULL;++title_checked;
    }
    return npe_render_native_title_screen(file,font,selection,pixels);
}
static void sound_choices(void) {
    for(unsigned i=0;i<sizeof(sound_choice_native)/sizeof(*sound_choice_native);++i)
        for(unsigned enabled=0;enabled<3;++enabled)
            for(unsigned group=1;group<=3;++group){
                NpeWorld world={0};world.music_enabled=(uint8_t)enabled;
                world.random_state=seed_for(sound_choice_native[i].roll);
                uint32_t before=world.random_state;
                npe_world_sound_bind(&world,observe,NULL);requests=0;
                npe_world_sound_scene_start(&world,group);
                assert(world.random_state==(group==1?before:next_state(before)));
                assert(requests==(enabled==1));
                if(requests){assert(requested[0]==(group==1?16:17+sound_choice_native[i].map_slot));assert(!requested_loop[0]);}
                npe_world_sound_stop(&world);assert(requested[requests-1]==0);
            }
}
static void finite_sound(NpeSoundPlayer *sound) {
    observed_sound=sound;
    for(unsigned section=16;section<=19;++section){
        assert(!npe_sound_play_section(sound,section,0));
        SDL_LockAudioDevice(sound->device);
        int track=sound->active;assert(track==(int)section-16&&!sound->loop);
        size_t count=sound->byte_count[track];assert(count>6);
        uint8_t quiet[512];
        while(sound->wait_bytes){
            size_t n=sound->wait_bytes<sizeof(quiet)?sound->wait_bytes:sizeof(quiet);
            sound_callback(sound,quiet,(int)n);for(size_t i=0;i<n;++i)assert(!quiet[i]);
        }
        assert(sound->phase==NPE_SOUND_PLAYING);
        npe_audio_mode5_init(&sound->decoder);
        unsigned last=sound->resources[track].header_count-1;
        for(unsigned frame=0;frame<=last;++frame)
            assert(!npe_audio_mode5_decode(&sound->decoder,&sound->resources[track],frame,sound->frame));
        sound->frame_valid=1;sound->frame_index=last;
        sound->gain=(NpeAudioGain){7,7};
        sound->output=(NpeAudioOutput){1,80,320};
        assert(npe_audio_output_half(&sound->output,&sound->gain,&sound->decoder.input.flags8a,
            sound->frame,160,0,sound->half[1],80)==1);
        sound->pcm_halves_pending=2u;
        uint8_t out[32],expected[32]={0};sound->cursor=count-6;
        for(unsigned i=0;i<3;++i){
            int16_t sample=npe_audio_gain_sample(sound->frame[157+i],7);
            for(unsigned copy=0;copy<4;++copy)memcpy(expected+i*8+copy*2,&sample,2);
        }
        sound_callback(sound,out,sizeof(out));assert(!memcmp(out,expected,sizeof(out)));
        assert(sound->phase==NPE_SOUND_END_WAIT&&sound->wait_bytes==3192);
        assert(sound->finished&&sound->cursor==count);SDL_UnlockAudioDevice(sound->device);
    }
    observed_sound=NULL;
}
static void waits(void) {
    fake_clock=UINT32_MAX-80;event_count=event_index=0;
    Uint32 before=fake_clock;assert(!scene_frame_wait()&&fake_clock-before==188);
    const SDL_Keycode keys[]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT,SDLK_RETURN,
                             SDLK_SPACE,SDLK_KP_ENTER,SDLK_F4,SDLK_ESCAPE};
    for(unsigned i=0;i<sizeof(keys)/sizeof(*keys);++i){
        memset(wait_events,0,sizeof(wait_events));event_index=0;event_count=3;
        wait_events[0].type=SDL_KEYUP;wait_events[0].key.keysym.sym=keys[i];
        wait_events[1].type=SDL_KEYDOWN;wait_events[1].key.keysym.sym=SDLK_a;
        wait_events[2].type=SDL_KEYDOWN;wait_events[2].key.keysym.sym=keys[i];
        before=fake_clock;assert(!scene_frame_wait()&&event_index==3&&fake_clock==before);
    }
    /* Original2E:FC6A ignores the delete key89; the old host check did not. */
    memset(wait_events,0,sizeof(wait_events));event_index=0;event_count=1;
    wait_events[0].type=SDL_KEYDOWN;wait_events[0].key.keysym.sym=SDLK_BACKSPACE;
    before=fake_clock;assert(!scene_frame_wait()&&event_index==1&&fake_clock-before==188);
    memset(wait_events,0,sizeof(wait_events));event_index=0;event_count=1;wait_events[0].type=SDL_QUIT;
    assert(scene_frame_wait()==-1);
    SDL_Event quit;assert(SDL_PeepEvents(&quit,1,SDL_GETEVENT,SDL_QUIT,SDL_QUIT)==1&&quit.type==SDL_QUIT);
    event_count=event_index=0;
}
int main(int argc,char **argv) {
    assert(argc==2);
    uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    sound_choices();
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    assert(!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("scene probe",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    NpeSoundPlayer sound;npe_sound_open(&sound,&file);assert(sound.device);
    SDL_PauseAudioDevice(sound.device,1); /* Deterministic callback inspection. */
    finite_sound(&sound);waits();
    for(unsigned i=0;i<sizeof(scenes)/sizeof(*scenes);++i){
        NpeWorld world;assert(!npe_world_init(&file,&world,9,29,13));
        assert(!npe_world_resources_open(&file,&world));
        current=&scenes[i];commits=0;
        char path[128];snprintf(path,sizeof(path),"tmp/native-scene-%s-before.idx",current->name);
        read_pixels(path,last_pixel_frame);last_pixel_renderer=renderer;
        npe_world_seed(&world,19);npe_world_sound_bind(&world,observe,NULL);requests=0;observed_sound=&sound;
        Uint32 before=fake_clock;
        assert(play_scene_group(renderer,texture,&file,&world,current->group,current->x,current->y,current->draw,current->erase)==1);
        unsigned expected=current->frames*(current->draw&&current->erase?2:1);
        assert(commits==expected&&fake_clock-before==188*current->frames);
        assert(requests==2&&requested[1]==0&&!requested_loop[0]&&!requested_loop[1]);
        assert(sound.active==-1&&!world.sound_section&&world.random_state==(current->group==1?19:next_state(19)));
        fprintf(stderr,"scene %s: %u C/SDL native commits, sound start/stop passed\n",current->name,commits);
        assert(!npe_rpg_runtime_scene_frames(&file));
        npe_rpg_runtime_close(&file);
    }
    current=NULL;observed_sound=NULL;
    NpeWorld world;assert(!npe_world_init(&file,&world,73,7,1));world.game_mode=15;world.story_event_id=1;
    assert(!npe_world_resources_open(&file,&world));
    read_pixels("tmp/native-scene-local-2-1-before.idx",last_pixel_frame);last_pixel_renderer=renderer;
    current=&scenes[6];commits=0;npe_world_sound_bind(&world,observe,NULL);requests=0;
    assert(play_scene_animation(renderer,texture,&file,&world)==1&&commits==24&&requests==2);
    current=NULL;assert(npe_world_finish_animation_event(&file,&world)==1&&world.game_mode==5&&world.story_event_id==50);
    assert(!npe_world_init(&file,&world,107,7,1));world.game_mode=15;world.story_event_id=1;
    npe_world_sound_bind(&world,observe,NULL);requests=0;uint32_t before=world.random_state;
    assert(!play_scene_animation(renderer,texture,&file,&world)&&!requests&&world.random_state==before);
    NpeWorld expected=world;assert(npe_world_apply_story_event(&expected,2)>=0);
    assert(npe_world_finish_animation_event(&file,&world)==1&&world.game_mode==expected.game_mode&&
           world.story_event_id==expected.story_event_id&&world.story_progress==expected.story_progress);
    world.music_enabled=0;requests=0;
    assert(play_scene_group(renderer,texture,&file,&world,2,60,0,2,1)==1&&requests==1&&requested[0]==0&&world.random_state==next_state(before));
    before=world.random_state;requests=0;
    assert(!play_scene_group(renderer,texture,&file,&world,100,0,0,0,0)&&!requests&&world.random_state==before);
    assert(!npe_world_init(&file,&world,73,7,1));world.game_mode=15;world.story_event_id=1;
    npe_world_sound_bind(&world,observe,NULL);requests=0;
    memset(wait_events,0,sizeof(wait_events));event_index=0;event_count=1;wait_events[0].type=SDL_QUIT;
    advance_world_events(window,renderer,texture,&file,NULL,&world,1,0);
    assert(world.game_mode==15&&world.story_event_id==1&&requests==2&&requested[1]==0);
    assert(!npe_rpg_runtime_scene_frames(&file));
    SDL_Event quit;assert(SDL_PeepEvents(&quit,1,SDL_GETEVENT,SDL_QUIT,SDL_QUIT)==1);
    event_count=event_index=0;
    npe_sound_close(&sound);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_free(&file);SDL_Quit();
    last_pixel_renderer=NULL;startup=1;current=&scenes[0];commits=0;
    char *args[]={"npe-sdl",argv[1],NULL};
    assert(!run(2,args)&&startup_checked==62&&title_checked==1);
    startup=0;current=NULL;event_count=1;event_index=0;wait_events[0].type=SDL_QUIT;
    assert(!run(2,args)&&title_checked==1); /* Close during opening skips title. */
    printf("%u C/actual SDL native comparisons (8 scenes, real map73 event and default opening), finite audio/gates/RNG and frame wait input passed\n",checked_commits);
    return 0;
}
