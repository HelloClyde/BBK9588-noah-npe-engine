#include "../sound_sdl.h"
#ifdef main
#undef main
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
#include "audio_callback_lifetime_native.h"
static unsigned ranges;
static int observed_range(const NpeFile *f,uint32_t o,int32_t n,NpeAudioResource *r){
    ++ranges;return npe_audio_resource_range(f,o,n,r);
}
#define npe_audio_resource_range observed_range
#include "../sound_sdl.c"
#undef npe_audio_resource_range
#define main npe_app_main
#include "../main.c"
#undef main

static void word32(NpeFile *f,uint32_t address,uint32_t value){
    uint8_t bytes[4];for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(value>>((3-i)*8));
    assert(!npe_rpg_runtime_write(f,address,bytes,4));
}
static void deliver(NpeSoundPlayer *p,size_t count,int quiet){
    uint8_t bytes[12800];while(count){size_t n=count<sizeof(bytes)?count:sizeof(bytes);
        memset(bytes,0xff,n);sound_callback(p,bytes,(int)n);
        if(quiet)for(size_t i=0;i<n;++i)assert(!bytes[i]);
        count-=n;}
}
static void begin(NpeFile *f,NpeWorld *w,NpeSoundPlayer *p,uint32_t offset,unsigned mode){
    SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);w->music_enabled=1;
    p->irq_flags8d=0;p->irq_stack_word=0x6000;
    if(mode==0)assert(!npe_sound_play_range(p,offset,26));
    if(mode==1)assert(!npe_sound_play_range_loop(p,offset,26,1));
    if(mode==2){word32(f,0x5278f2,26);word32(f,0x5278fe,offset);w->map_music_slot=0;
        npe_world_sound_map_resume(w);assert(p->continuation==SOUND_CONTINUATION_MAP);}
    if(mode==3){word32(f,0x527996,26);word32(f,0x52799a,offset);
        assert(npe_world_sound_battle_replay(w)==1&&p->continuation==SOUND_CONTINUATION_BATTLE);}
    while(p->wait_bytes)deliver(p,p->wait_bytes,1);
    assert(p->phase==NPE_SOUND_PLAYING&&!p->cursor);
    assert(!!(p->decoder.input.flags89&0x8000)==(mode!=0));
}
static void new_range(NpeFile *f,unsigned mode){
    if(mode==2){word32(f,0x5278f2,f->sections[21].size);word32(f,0x5278fe,f->sections[21].offset);}
    if(mode==3){word32(f,0x527996,f->sections[24].size);word32(f,0x52799a,f->sections[24].offset);}
}
static void callback_activity(const NpeSoundPlayer *p,unsigned stage){
    unsigned actual=p->decoder.input.flags89&8u;
    if(actual!=audio_callback_activity[stage])
        fprintf(stderr,"callback activity stage%u: C%u original%u phase%u\n",
            stage,actual,audio_callback_activity[stage],(unsigned)p->phase);
    assert(actual==audio_callback_activity[stage]);
}
static void reopen_return(NpeSoundPlayer *p){
    assert(p->callback_return_pending);
    callback_activity(p,0);
    size_t cleanup=(size_t)AUDIO_CALLBACK_CLEANUP_TICKS*NPE_AUDIO_OUTPUT_RATE*2/1000;
    assert(p->phase==NPE_SOUND_CLEANUP_WAIT&&p->wait_bytes==cleanup);
    deliver(p,cleanup-1,1);callback_activity(p,0);
    assert(p->wait_bytes==1&&p->callback_return_pending);
    deliver(p,1,1);callback_activity(p,1);
    size_t startup=(size_t)AUDIO_CALLBACK_START_TICKS*NPE_AUDIO_OUTPUT_RATE*2/1000;
    assert(p->phase==NPE_SOUND_START_WAIT&&p->wait_bytes==startup);
    deliver(p,startup-1,1);callback_activity(p,2);
    assert(p->wait_bytes==1&&p->callback_return_pending);
    deliver(p,1,1);callback_activity(p,3);
    assert(p->phase==NPE_SOUND_PLAYING&&!p->cursor&&!p->callback_return_pending);
}
int main(int argc,char **argv){
    assert(argc==2&&!SDL_Init(SDL_INIT_EVENTS));NpeFile f;battle_sound_file(argv[1],&f);
    uint32_t offset=(uint32_t)f.size;uint8_t *grown=realloc(f.bytes,f.size+26);assert(grown);f.bytes=grown;
    memcpy(f.bytes+f.size,f.bytes+f.sections[20].offset,25);f.bytes[f.size+2]=0;f.bytes[f.size+3]=1;f.bytes[f.size+25]=0;f.size+=26;
    NpeWorld w;assert(!npe_world_init(&f,&w,9,29,13)&&!npe_world_resources_open(&f,&w));
    NpeSoundPlayer p;npe_sound_open(&p,&f);assert(p.device);SDL_PauseAudioDevice(p.device,1);
    npe_sound_select(&p,&w);game_sound_player=&p;
    const uint16_t guards89[]={1,2,4,0,0,8,0,7};
    const uint16_t guards8d[]={0,0,0,1,4,0,0,5};
    for(unsigned mode=0;mode<4;++mode)for(unsigned guard=0;guard<8;++guard){
        begin(&f,&w,&p,offset,mode);deliver(&p,1279,0);
        p.decoder.input.flags89|=(uint16_t)(0x7000u|guards89[guard]);p.irq_flags8d=guards8d[guard];
        if(guard==6)p.irq_stack_word=0x1fe;
        deliver(&p,1,0);assert(p.phase==NPE_SOUND_EOF_BLOCKED&&!p.wait_bytes&&!p.finished&&!p.end_pending);
        int early=guard==5||guard==6;
        assert((p.decoder.input.flags89&0x7000)==(early?0x7000:0));
        unsigned before=ranges;size_t cursor=p.cursor;uint64_t clock=p.clock.value;unsigned gain=p.gain.current;
        deliver(&p,1280,1);assert(p.phase==NPE_SOUND_EOF_BLOCKED&&p.cursor==cursor&&p.clock.value==clock&&p.gain.current==gain);
        assert(!npe_sound_pump(&p)&&ranges==before);
        p.decoder.input.flags89&=(uint16_t)~guards89[guard];p.irq_flags8d=0;p.irq_stack_word=0x6000;
        deliver(&p,1,1);assert(p.phase==NPE_SOUND_END_WAIT&&p.wait_bytes==3199&&!p.end_pending);
        assert(!(p.decoder.input.flags89&0x7000));new_range(&f,mode);
        deliver(&p,3198,1);assert(p.wait_bytes==1&&!p.end_pending);deliver(&p,1,1);
        if(mode==0)assert(p.phase==NPE_SOUND_STOPPED&&p.finished&&!(p.decoder.input.flags89&8));
        if(mode==1){callback_activity(&p,0);reopen_return(&p);
            assert(npe_audio_clock_ms(&p.clock)==20&&(p.decoder.input.flags89&0x8000));}
        if(mode>=2){assert(p.phase==NPE_SOUND_STOPPED&&p.end_pending&&(p.decoder.input.flags89&8));
            assert(npe_sound_pump(&p)==1&&ranges==before+1&&!p.end_pending);
            assert(p.phase==NPE_SOUND_CLEANUP_WAIT&&p.selected_section==(mode==2?21:24));
            callback_activity(&p,0);assert(!npe_sound_pump(&p)&&ranges==before+1);
            reopen_return(&p);}
    }
    /* Live registration read after the stop wait. */
    begin(&f,&w,&p,offset,2);deliver(&p,1280,0);assert(p.phase==NPE_SOUND_END_WAIT);
    p.decoder.input.flags89&=(uint16_t)~0x8000u;unsigned before=ranges;
    deliver(&p,3200,1);assert(p.phase==NPE_SOUND_STOPPED&&p.finished&&!p.end_pending&&!(p.decoder.input.flags89&8));
    assert(!npe_sound_pump(&p)&&ranges==before);
    /* A finite replacement cancels an already queued active callback. */
    begin(&f,&w,&p,offset,2);deliver(&p,1280,0);deliver(&p,3200,1);
    assert(p.end_pending&&(p.decoder.input.flags89&8));before=ranges;
    assert(!npe_sound_play_range(&p,f.sections[36].offset,(int32_t)f.sections[36].size));
    assert(!p.end_pending&&!(p.decoder.input.flags89&8)&&!npe_sound_pump(&p)&&ranges==before+1);
    /* Explicit stop while EOF is blocked never dispatches the old callback. */
    begin(&f,&w,&p,offset,2);deliver(&p,640,0);p.irq_flags8d=4;deliver(&p,640,0);
    assert(p.phase==NPE_SOUND_EOF_BLOCKED);before=ranges;npe_world_sound_stop(&w);
    deliver(&p,3200,1);assert(p.phase==NPE_SOUND_STOPPED&&!p.end_pending&&!npe_sound_pump(&p)&&ranges==before);
    /* A callback with no new request still clears its active bit. */
    begin(&f,&w,&p,offset,3);deliver(&p,1280,0);deliver(&p,3200,1);w.music_enabled=0;before=ranges;
    assert(!npe_sound_pump(&p)&&!(p.decoder.input.flags89&8)&&!p.end_pending&&ranges==before);
    /* Original mute bit survives cleanup and skips the200 startup ticks. */
    for(unsigned mode=1;mode<=2;++mode){
        p.gain=(NpeAudioGain){0,0};begin(&f,&w,&p,offset,mode);
        deliver(&p,1280,1);assert(p.phase==NPE_SOUND_END_WAIT&&(p.decoder.input.flags8a&0x80));
        deliver(&p,3200,1);
        if(mode==2){new_range(&f,mode);assert(npe_sound_pump(&p)==1);}
        assert(p.phase==NPE_SOUND_CLEANUP_WAIT&&p.wait_bytes==3200);
        callback_activity(&p,0);assert(p.callback_return_pending);
        deliver(&p,3200,1);
        /* The short cached range retains source requests; the new long
           map range primes both file sources and clears them before31AC. */
        uint16_t expected8a=mode==1?0x83u:0x80u;
        if(p.phase!=NPE_SOUND_PLAYING||p.wait_bytes||p.cursor||p.decoder.input.flags8a!=expected8a)
            fprintf(stderr,"mute reopen mode%u: phase=%u wait=%zu cursor=%zu flags8a=%04x pending=%u clock=%u\n",
                mode,(unsigned)p.phase,p.wait_bytes,p.cursor,p.decoder.input.flags8a,p.pcm_halves_pending,
                npe_audio_clock_ms(&p.clock));
        assert(p.phase==NPE_SOUND_PLAYING&&!p.wait_bytes&&!p.cursor&&p.decoder.input.flags8a==expected8a);
        callback_activity(&p,3);assert(!p.callback_return_pending);
        deliver(&p,1,1);assert(p.decoder.input.flags8a&0x80);
    }
    p.gain=(NpeAudioGain){7,7};
    /* Replacement or explicit stop cancels a deferred callback in either
       wait phase; its eventual completion cannot affect the new request. */
    for(unsigned mode=1;mode<4;++mode)for(unsigned started=0;started<2;++started)
        for(unsigned stop=0;stop<2;++stop){
        begin(&f,&w,&p,offset,mode);deliver(&p,1280,0);deliver(&p,3200,1);
        if(mode>=2){new_range(&f,mode);assert(npe_sound_pump(&p)==1);}
        assert(p.callback_return_pending);callback_activity(&p,0);
        if(started){deliver(&p,3200,1);assert(p.phase==NPE_SOUND_START_WAIT);callback_activity(&p,1);}
        if(stop)npe_world_sound_stop(&w);
        else assert(!npe_sound_play_range(&p,f.sections[36].offset,(int32_t)f.sections[36].size));
        assert(!p.callback_return_pending&&!p.end_pending);callback_activity(&p,3);
        while(p.wait_bytes)deliver(&p,p.wait_bytes,1);
        assert(p.phase==(stop?NPE_SOUND_STOPPED:NPE_SOUND_PLAYING));callback_activity(&p,3);
    }
    /* A failed cached reopen releases activity only after its existing
       failure cleanup completes, not at the initial reopen boundary. */
    begin(&f,&w,&p,offset,1);deliver(&p,1280,0);deliver(&p,3200,1);
    p.resources[p.active].size=4;deliver(&p,3200,1);
    assert(p.decode_failed&&p.phase==NPE_SOUND_CLEANUP_WAIT&&p.callback_return_pending);
    callback_activity(&p,1);deliver(&p,3199,1);callback_activity(&p,2);
    deliver(&p,1,1);assert(p.phase==NPE_SOUND_STOPPED&&!p.driver_configured&&!p.callback_return_pending);
    callback_activity(&p,3);
    /* Reinitializing compression state must retain an in-flight callback.
       Its return clears the bit in the newly initialized driver state. */
    begin(&f,&w,&p,offset,1);p.decoder.input.flags89|=8;
    sound_start_wait(&p);while(p.wait_bytes)deliver(&p,p.wait_bytes,1);
    assert(p.phase==NPE_SOUND_PLAYING&&(p.decoder.input.flags89&8));
    deliver(&p,1280,0);assert(p.phase==NPE_SOUND_EOF_BLOCKED&&(p.decoder.input.flags89&8));
    npe_audio_eof_callback_end(&p.decoder.input.flags89);
    deliver(&p,1,1);assert(p.phase==NPE_SOUND_END_WAIT&&p.wait_bytes==3199);
    /* Actual SDL audio thread retries a busy EOF while the UI has no input. */
    for(unsigned source=0;source<2;++source){
        begin(&f,&w,&p,offset,2);deliver(&p,1279,0);p.decoder.input.flags89|=source?0:1;p.irq_flags8d=source?4:0;
        before=ranges;new_range(&f,2);SDL_PauseAudioDevice(p.device,0);
        SDL_Event e;assert(!game_WaitEventTimeout(&e,150));SDL_LockAudioDevice(p.device);
        assert(p.phase==NPE_SOUND_EOF_BLOCKED&&!p.end_pending&&ranges==before);
        p.decoder.input.flags89&=(uint16_t)~1u;p.irq_flags8d=0;SDL_UnlockAudioDevice(p.device);
        assert(!game_WaitEventTimeout(&e,200));SDL_PauseAudioDevice(p.device,1);
        assert(ranges==before+1&&p.selected_section==21&&!p.end_pending&&!(p.decoder.input.flags89&8));
    }
    game_sound_player=NULL;npe_sound_close(&p);npe_free(&f);SDL_Quit();
    puts("SDL EOF gate:32 finite/cached/map/battle blocked paths;24 split-wait original callback lifetimes;12 replacement/stop cancellations and failed reopen;2 mute startup bypasses and2 actual audio-thread busy/resume UI waits passed");
    return 0;
}
