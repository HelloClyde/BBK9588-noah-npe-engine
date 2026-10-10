#include "../sound_sdl.h"
#ifdef main
#undef main
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
#include "audio_foreground_wait_native.h"
#include "audio_callback_lifetime_native.h"

/* Pause the real device while this diagnostic supplies callback bytes.
   The production wait sees an available virtual delivery backend. */
static int manual_backend;
static SDL_AudioStatus delivery_status(SDL_AudioDeviceID device) {
    return manual_backend ? SDL_AUDIO_PLAYING : SDL_GetAudioDeviceStatus(device);
}
#define SDL_GetAudioDeviceStatus delivery_status
#include "../sound_sdl.c"
#undef SDL_GetAudioDeviceStatus

static NpeSoundPlayer *player;
static NpeWorld *world;
static SDL_sem *entered;
static unsigned requests;
static struct {uint16_t music, mode, pending;uint32_t generation, completed;} trace[4];

static void observe_before(void) {
    assert(requests < 4);
    trace[requests].music=world->music_enabled;
    trace[requests].mode=world->game_mode;
    trace[requests].pending=world->map_init_pending;
    assert(!SDL_SemPost(entered));
}
static void observe_after(void) {
    SDL_LockAudioDevice(player->device);
    trace[requests].generation=player->request_generation;
    trace[requests].completed=player->request_complete;
    SDL_UnlockAudioDevice(player->device);
    assert(trace[requests].generation==trace[requests].completed);
    ++requests;
}
static void range_request(void *context,uint32_t offset,int32_t length,uint32_t callback) {
    observe_before();sound_range_request(context,offset,length,callback);observe_after();
}
static void section_request(void *context,unsigned section,int loop) {
    observe_before();sound_request(context,section,loop);observe_after();
}
static int music_sequence(void *context) {
    (void)context;
    assert(npe_world_music_controller(world,0)==1); // Old off -> original on cursor.
    assert(npe_world_music_controller(world,1)==1);
    assert(world->music_enabled==1 && world->game_mode==1 && world->map_init_pending==1);
    assert(npe_world_music_controller(world,2)==1 && !world->map_init_pending);
    assert(npe_world_music_controller(world,0)==1); // Old on -> original off cursor.
    assert(npe_world_music_controller(world,1)==1);
    assert(!world->music_enabled && world->game_mode==1 && world->map_init_pending==1);
    return 0;
}
static void wait_for_request(uint32_t prior) {
    assert(!SDL_SemWaitTimeout(entered,2000));
    Uint32 started=SDL_GetTicks();
    for(;;) {
        SDL_LockAudioDevice(player->device);
        int ready=player->request_generation!=prior && player->request_waiting;
        SDL_UnlockAudioDevice(player->device);
        if(ready)return;
        assert(SDL_GetTicks()-started<2000);SDL_Delay(1);
    }
}
static void deliver_wait(unsigned ticks,int completes) {
    size_t count=(size_t)ticks*NPE_AUDIO_OUTPUT_RATE*2/1000;
    assert(count && count<=12800);
    uint8_t bytes[12800];
    SDL_LockAudioDevice(player->device);
    uint32_t generation=player->request_generation;
    assert(player->wait_bytes==count && player->request_complete!=generation);
    /* One byte short must leave the caller pending, even if earlier phase
       notifications have already been posted to its semaphore. */
    sound_callback(player,bytes,(int)count-1);
    assert(player->wait_bytes==1 && player->request_complete!=generation);
    for(size_t i=0;i<count-1;++i)assert(!bytes[i]);
    sound_callback(player,bytes,1);assert(!bytes[0]);
    assert((player->request_complete==generation)==completes);
    assert(!player->cursor && !player->decode_failed);
    SDL_UnlockAudioDevice(player->device);
}

typedef struct {NpeWorld *world;NpeSoundPlayer *player;int battle,pump;} CallbackOperation;
static int callback_operation(void *context){
    CallbackOperation *op=context;
    if(op->pump)return npe_sound_pump(op->player);
    if(op->battle)npe_world_sound_battle_start(op->world);
    else npe_world_sound_map_resume(op->world);
    return 0;
}
static void wait_callback_request(NpeSoundPlayer *p,uint32_t prior){
    Uint32 start=SDL_GetTicks();
    for(;;){
        SDL_LockAudioDevice(p->device);
        int ready=p->request_generation!=prior&&p->request_waiting;
        SDL_UnlockAudioDevice(p->device);
        if(ready)return;
        assert(SDL_GetTicks()-start<2000);SDL_Delay(1);
    }
}
static void deliver_callback_wait(NpeSoundPlayer *p,unsigned ticks,unsigned after_stage){
    size_t count=(size_t)ticks*NPE_AUDIO_OUTPUT_RATE*2/1000;
    uint8_t bytes[12800];assert(count&&count<=sizeof(bytes));
    SDL_LockAudioDevice(p->device);
    assert(p->wait_bytes==count&&!p->callback_return_pending);
    assert((p->decoder.input.flags89&8u)==audio_callback_activity[0]);
    sound_callback(p,bytes,(int)count-1);
    assert(p->wait_bytes==1&&(p->decoder.input.flags89&8u)==audio_callback_activity[0]);
    sound_callback(p,bytes,1);
    /* The waiting game thread cannot return until this audio lock is
       released; capture the original after-service/before-return boundary. */
    assert((p->decoder.input.flags89&8u)==audio_callback_activity[after_stage]);
    SDL_UnlockAudioDevice(p->device);
}
static void check_foreground_callbacks(NpeSoundPlayer *p,NpeWorld *w){
    w->sound_observer=sound_request;w->sound_range_observer=sound_range_request;
    w->music_enabled=1;
    for(unsigned battle=0;battle<2;++battle)for(unsigned mute=0;mute<2;++mute){
        p->gain=(NpeAudioGain){mute?0:7,mute?0:7};
        p->decoder.input.flags8a=(uint16_t)(mute?0x80u:0);
        uint32_t prior=p->request_generation;
        CallbackOperation op={w,p,(int)battle,0};
        SDL_Thread *thread=SDL_CreateThread(callback_operation,"background start",&op);assert(thread);
        wait_callback_request(p,prior);
        if(p->phase==NPE_SOUND_CLEANUP_WAIT)deliver_wait(AUDIO_CALLBACK_CLEANUP_TICKS,mute!=0);
        if(!mute)deliver_wait(AUDIO_CALLBACK_START_TICKS,1);
        int result=-1;SDL_WaitThread(thread,&result);assert(!result);
        SDL_LockAudioDevice(p->device);
        assert(p->phase==NPE_SOUND_PLAYING&&!p->wait_bytes);
        sound_end(p);assert(p->phase==NPE_SOUND_END_WAIT);
        uint8_t stop[3200];assert(p->wait_bytes==sizeof(stop));
        sound_callback(p,stop,sizeof(stop));
        assert(p->end_pending&&(p->decoder.input.flags89&8u)==audio_callback_activity[0]);
        prior=p->request_generation;SDL_UnlockAudioDevice(p->device);
        op.pump=1;thread=SDL_CreateThread(callback_operation,"background callback",&op);assert(thread);
        wait_callback_request(p,prior);
        deliver_callback_wait(p,AUDIO_CALLBACK_CLEANUP_TICKS,mute?2:1);
        if(!mute)deliver_callback_wait(p,AUDIO_CALLBACK_START_TICKS,2);
        SDL_WaitThread(thread,&result);assert(result==1);
        SDL_LockAudioDevice(p->device);
        assert(p->phase==NPE_SOUND_PLAYING&&!p->wait_bytes&&!p->end_pending&&!p->callback_return_pending);
        assert((p->decoder.input.flags89&8u)==audio_callback_activity[3]);
        SDL_UnlockAudioDevice(p->device);
    }
}

int main(int argc,char **argv) {
    assert(argc==2);
    NpeFile file;battle_sound_file(argv[1],&file);
    NpeWorld w;assert(!npe_world_init(&file,&w,9,29,13) && !npe_world_resources_open(&file,&w));
    assert(!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    NpeSoundPlayer p;npe_sound_open(&p,&file);assert(p.device && p.request_ready);
    SDL_PauseAudioDevice(p.device,1);
    npe_sound_select(&p,&w);
    /* Binding replays the world's initial resource. Finish an explicit stop
       so this case starts with the original idle-driver input. */
    (void)npe_sound_play_section(&p,0,0);
    if(p.wait_bytes){uint8_t cleanup[3200];assert(p.wait_bytes==sizeof(cleanup));
        sound_callback(&p,cleanup,sizeof(cleanup));}
    assert(!p.driver_configured && p.phase==NPE_SOUND_STOPPED);
    w.music_enabled=0;w.game_mode=4;w.map_init_pending=0;
    world=&w;player=&p;entered=SDL_CreateSemaphore(0);assert(entered);
    w.sound_range_observer=range_request;w.sound_observer=section_request;
    manual_backend=1;
    uint32_t generation=p.request_generation;
    SDL_Thread *thread=SDL_CreateThread(music_sequence,"game requests",NULL);assert(thread);
    wait_for_request(generation);generation=p.request_generation;
    assert(p.phase==NPE_SOUND_START_WAIT);
    deliver_wait(foreground_wait_native[0],1);
    wait_for_request(generation);generation=p.request_generation;
    assert(p.phase==NPE_SOUND_CLEANUP_WAIT);
    deliver_wait(foreground_wait_native[1]-foreground_wait_native[0],0);
    assert(p.phase==NPE_SOUND_START_WAIT);
    deliver_wait(foreground_wait_native[0],1);
    wait_for_request(generation);
    assert(p.phase==NPE_SOUND_CLEANUP_WAIT);
    deliver_wait(foreground_wait_native[3],1);
    int result=-1;SDL_WaitThread(thread,&result);assert(!result && requests==3);
    assert(trace[0].music==0 && trace[0].mode==4 && trace[0].pending==0);
    assert(trace[1].music==1 && trace[1].mode==1 && trace[1].pending==1);
    assert(trace[2].music==1 && trace[2].mode==1 && trace[2].pending==0);
    assert(foreground_wait_native[2]==foreground_wait_native[0]+foreground_wait_native[1]);
    assert(!p.driver_configured && p.phase==NPE_SOUND_STOPPED && !p.request_waiting);
    /* Idle stop and mute startup complete immediately, as in the original. */
    sound_request(&p,0,0);assert(!p.wait_bytes && p.request_complete==p.request_generation);
    assert(!foreground_wait_native[4]);
    p.gain=(NpeAudioGain){0,0};p.decoder.input.flags8a|=0x80;
    sound_request(&p,20,0);assert(!p.wait_bytes && p.request_complete==p.request_generation);
    assert(!foreground_wait_native[5]);
    /* Completed token survives a subsequent autonomous cleanup phase. */
    p.phase=NPE_SOUND_CLEANUP_WAIT;p.wait_bytes=3200;
    assert(!npe_sound_wait_request(&p) && p.wait_bytes==3200);
    manual_backend=0;
    assert(npe_sound_wait_request(&p)==-1);
    p.gain=(NpeAudioGain){7,7};p.decoder.input.flags8a&=(uint16_t)~0x80u;
    SDL_PauseAudioDevice(p.device,0);
    sound_request(&p,20,0); // Actual dummy SDL callback supplies the wait notifications.
    SDL_LockAudioDevice(p.device);
    assert(p.request_complete==p.request_generation && !p.request_waiting && !p.wait_bytes);
    SDL_UnlockAudioDevice(p.device);
    assert(npe_sound_wait_request(NULL)==-1);
    SDL_PauseAudioDevice(p.device,1);manual_backend=1;
    check_foreground_callbacks(&p,&w);manual_backend=0;
    SDL_DestroySemaphore(entered);npe_sound_close(&p);npe_free(&file);SDL_Quit();
    puts("Original 200/250/450/50/0/0 foreground counts: split delivery, pre-store state and real SDL wakeups;4 map/battle normal/muted callback lifetimes through cleanup/start/foreground return passed");
    return 0;
}
