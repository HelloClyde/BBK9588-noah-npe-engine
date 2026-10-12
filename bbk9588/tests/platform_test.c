#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../platform.c"
static uint16_t test_mode=1;
uint16_t *npe_bbk_game_mode=&test_mode;
void npe_bbk_log_reset(void){}

static SDL_Event next_event(void){SDL_Event e;assert(SDL_PollEvent(&e));return e;}
static void callback(void *unused,Uint8 *bytes,int count){
    (void)unused;int16_t *s=(int16_t *)bytes;
    for(int i=0;i<count/2;++i)s[i]=8192;
}
static void test_audio(void){
    SDL_AudioSpec want={32000,AUDIO_S16SYS,1,1024,callback,NULL};
    assert(SDL_OpenAudioDevice(NULL,0,&want,NULL,0)==1);
    assert(audio_original==96);
    SDL_PauseAudioDevice(1,0);audio_pump();
    assert(mock_applied==0&&mock_writes==4&&mock_last_peak==8192);
    for(unsigned i=0;i<mock_writes;++i)assert(mock_sizes[i]==1024);
    touch_dispatch(NPE_TOUCH_VOL_DOWN);audio_pump();assert(mock_applied==12);
    touch_dispatch(NPE_TOUCH_MUTE);audio_pump();assert(mock_applied==96&&mock_last_peak==0);
    touch_dispatch(NPE_TOUCH_MUTE);audio_pump();assert(mock_applied==12);
    SDL_CloseAudioDevice(1);
    assert(mock_applied==96&&mock_last_peak==0&&mock_stopped==1);
    assert(!audio_open);
    assert(SDL_OpenAudioDevice(NULL,0,&want,NULL,0)==1);
    SDL_PauseAudioDevice(1,0);mock_write_error=-17;audio_pump();
    assert(audio_failed&&!audio_open&&audio_write_error==-17&&mock_stopped==2);
    mock_write_error=0;
}
static void test_keyboard(void){
    SDL_Init(SDL_INIT_VIDEO);
    touch_dispatch(NPE_TOUCH_ALPHA);
    touch_dispatch(NPE_TOUCH_SHIFT);touch_dispatch('1');
    SDL_Event e=next_event();assert(e.key.keysym.sym=='!'&&!(e.key.keysym.mod&KMOD_SHIFT));
    assert(!(touch_modifiers&KMOD_SHIFT));
    touch_dispatch(NPE_TOUCH_SHIFT);touch_dispatch('a');
    e=next_event();assert(e.key.keysym.sym=='a'&&(e.key.keysym.mod&KMOD_SHIFT));
    touch_dispatch(NPE_TOUCH_CAPS);touch_dispatch('z');
    e=next_event();assert(e.key.keysym.sym=='z'&&(e.key.keysym.mod&KMOD_CAPS));
    touch_dispatch(NPE_TOUCH_SYMBOLS);touch_dispatch('+');
    e=next_event();assert(e.key.keysym.sym=='+');
    touch_dispatch(NPE_TOUCH_FUNCTIONS);touch_dispatch(SDLK_F12);
    e=next_event();assert(e.key.keysym.sym==SDLK_F12);
    touch_dispatch(NPE_TOUCH_PAD);
    /* The top/bottom/left/right geometry and center action are explicit. */
    assert(npe_touch_hit(0,60,204)==SDLK_UP);
    assert(npe_touch_hit(0,60,280)==SDLK_DOWN);
    assert(npe_touch_hit(0,22,242)==SDLK_LEFT);
    assert(npe_touch_hit(0,98,242)==SDLK_RIGHT);
    assert(npe_touch_hit(0,60,242)==SDLK_SPACE);
    mock_x=60;mock_y=204;mock_ticks=0;
    proc(0,BDA_MSG_TOUCH_COORDINATE,0,((u32)mock_y<<16)|mock_x);
    SDL_PumpEvents();assert(!SDL_HasEvent(SDL_KEYDOWN));
    frame=1;mock_keys[4]=1;
    mock_ticks=300;e=next_event();assert(e.key.keysym.sym==SDLK_UP&&!e.key.repeat);
    mock_ticks=400;e=next_event();assert(e.key.keysym.sym==SDLK_UP&&e.key.repeat);
    mock_keys[4]=0;mock_ticks=500;SDL_PumpEvents();assert(!SDL_HasEvent(SDL_KEYDOWN));
    proc(0,BDA_MSG_TOUCH_RELEASE,0,((u32)mock_y<<16)|mock_x);
    SDL_PumpEvents();assert(!SDL_HasEvent(SDL_KEYDOWN));frame=0;
    proc(0,BDA_MSG_TOUCH_COORDINATE,0,((u32)mock_y<<16)|mock_x);
    SDL_PumpEvents();mock_ticks=550;
    proc(0,BDA_MSG_TOUCH_RELEASE,0,((u32)mock_y<<16)|mock_x);
    e=next_event();assert(e.key.keysym.sym==SDLK_UP&&!e.key.repeat);
}
static void test_coverage_and_bounds(void){
    unsigned char found[128]={0};
    unsigned char pixels[32+240*320*2+32];
    for(int page=0;page<4;++page){
        for(int y=148;y<320;++y)for(int x=0;x<240;++x){
            int k=npe_touch_hit(page,x,y);if(k>=32&&k<127)found[k]=1;
        }
        for(int ch='a';ch<='z';++ch)if(found[ch])found[ch-'a'+'A']=1;
        memset(pixels,0x55,sizeof(pixels));
        npe_touch_draw(pixels+32,page,KMOD_SHIFT|KMOD_CAPS,100,0,232,310);
        for(int i=0;i<32;++i)assert(pixels[i]==0x55&&pixels[sizeof(pixels)-32+i]==0x55);
        for(int i=28*240*2;i<148*240*2;++i)assert(pixels[32+i]==0x55);
    }
    for(int ch=32;ch<127;++ch)assert(found[ch]);
}
static void test_touch_escape(void){
    SDL_Init(SDL_INIT_VIDEO);frame=1;mock_ticks=0;mock_keys[4]=1;
    SDL_PumpEvents();mock_ticks=500;SDL_PumpEvents();
    assert(!SDL_HasEvent(SDL_KEYDOWN));
    proc(0,0x21,0,0);mock_keys[4]=0;mock_ticks=700;SDL_PumpEvents();
    assert(!SDL_HasEvent(SDL_KEYDOWN));
    mock_keys[4]=1;SDL_PumpEvents();mock_ticks=1000;SDL_PumpEvents();
    assert(!SDL_HasEvent(SDL_KEYDOWN));
    mock_keys[4]=0;SDL_PumpEvents();mock_ticks=1151;
    SDL_Event e=next_event();assert(e.key.keysym.sym==SDLK_F4);
    frame=0;
}
int main(void){
    SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO);
    test_audio();test_keyboard();test_coverage_and_bounds();test_touch_escape();
    puts("PASS: PCM volume/block/restore/errors; printable ASCII/modifiers; D-pad hold; render bounds");
    return 0;
}
