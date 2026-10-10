#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {const char *name;unsigned event,music,slot,keys,wrong,key[4],plays[5];} QuizAudioCase;
#include "quiz_audio_native.h"
static unsigned case_index,phase,plays[5],collecting;
static NpeWorld *live;
static NpeSoundPlayer *player;
static Uint32 fake_clock;
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 n){fake_clock+=n;}
static int poll(SDL_Event *e){(void)e;return 0;}
static int wait_event(SDL_Event *,int);
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent poll
#define SDL_WaitEventTimeout wait_event
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
static void request(void *context,unsigned section,int loop){NpeSoundPlayer *p=context;
    if(collecting){const QuizAudioCase *c=&quiz_audio_cases[case_index];assert(phase==c->keys&&section==20+c->slot&&loop);
        unsigned feedback_ms=c->event<10?(c->wrong?2400:800):0;
        assert(fake_clock==feedback_ms);
        ++plays[phase];}
    npe_sound_play_section(p,section,loop);
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;const QuizAudioCase *c=&quiz_audio_cases[case_index];
    assert(phase<c->keys&&plays[phase]==c->plays[phase]&&npe_sound_cursor(player)==128);
    unsigned key=c->key[phase++];memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;
    e->key.keysym.sym=key==0x83?SDLK_DOWN:key==0x8e?SDLK_F4:SDLK_RETURN;
    if(c->wrong){NpeQuizQuestion q;assert(!npe_quiz_question((const NpeFile *)live->battle_file,live->quiz_question_index,&q));
        live->event_context_id=(uint16_t)((q.correct_answer+1)%4);}
    return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    uint8_t *bytes;size_t size;NpeFile file;char error[128];NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *window=SDL_CreateWindow("quiz audio",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    NpeSoundPlayer sound;npe_sound_open(&sound,&file);assert(sound.device);SDL_PauseAudioDevice(sound.device,1);player=&sound;
    unsigned checked=0;
    for(unsigned stopped=0;stopped<2;++stopped)for(case_index=0;case_index<sizeof(quiz_audio_cases)/sizeof(*quiz_audio_cases);++case_index){
        const QuizAudioCase *c=&quiz_audio_cases[case_index];NpeWorld world;assert(!npe_world_new_game(&file,&world));live=&world;
        npe_world_seed(&world,11);world.game_mode=8;world.story_event_id=(uint16_t)c->event;
        world.quiz_answer_count=5;world.quiz_wrong_count=(uint8_t)c->wrong;world.music_enabled=(uint8_t)c->music;world.map_music_slot=(uint8_t)c->slot;
        world.sound_section=c->music&&!stopped?(uint8_t)(20+c->slot):0;world.sound_loop=(uint8_t)(world.sound_section!=0);
        collecting=0;npe_world_sound_bind(&world,request,&sound);SDL_LockAudioDevice(sound.device);sound.cursor=128;SDL_UnlockAudioDevice(sound.device);
        phase=fake_clock=0;memset(plays,0,sizeof(plays));collecting=1;
        advance_world_events(window,renderer,texture,&file,&font,&world,1,1);collecting=0;
        assert(world.game_mode==1&&phase==c->keys&&world.map_music_slot==c->slot);
        assert(world.random_state==(c->event<10?UINT32_C(11)*UINT32_C(0x514f111f)+1u:11u));
        for(unsigned i=0;i<=c->keys;++i)assert(plays[i]==c->plays[i]);
        assert(npe_sound_cursor(&sound)==(c->music?0:128));
        if(c->music)assert(sound.selected_section==(int)(20+c->slot)&&sound.loop&&sound.active>=0&&!sound.finished);
        fprintf(stderr,"Quiz audio %s stopped%u retained slot%u: requests and SDL cursor passed\n",c->name,stopped,c->slot);++checked;
    }
    assert(checked==12);npe_sound_close(&sound);npe_font_free(&font);npe_free(&file);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    puts("Six original quiz audio routes and six initially-stopped C cases: playback phase, retained slot and actual SDL cursor passed");return 0;
}
