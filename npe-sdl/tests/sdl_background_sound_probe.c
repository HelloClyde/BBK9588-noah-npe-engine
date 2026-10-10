#include "../sound_sdl.h"
#ifdef main
#undef main
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
static unsigned ranges;
static int observed_range(const NpeFile *f,uint32_t o,int32_t s,NpeAudioResource *r){++ranges;return npe_audio_resource_range(f,o,s,r);}
#define npe_audio_resource_range observed_range
#include "../sound_sdl.c"
#undef npe_audio_resource_range
#define main npe_app_main
#include "../main.c"
#undef main
static void long_write(NpeFile *f,uint32_t address,uint32_t value){
    uint8_t bytes[4];for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(value>>((3-i)*8));assert(!npe_rpg_runtime_write(f,address,bytes,4));
}
static uint32_t seed_for(unsigned roll){for(uint32_t n=0;n<1000000;++n)if((((n*UINT32_C(0x514f111f)+1)>>16)&32767)==roll)return n;assert(0);return 0;}
static void deliver(NpeSoundPlayer *p,size_t count){uint8_t data[12800];while(count){size_t n=count<sizeof(data)?count:sizeof(data);sound_callback(p,data,(int)n);count-=n;}}
static void prefix(NpeSoundPlayer *p,uint8_t data[1280]){while(p->wait_bytes)deliver(p,p->wait_bytes);sound_callback(p,data,1280);assert(!p->decode_failed);}
static void short_battle(NpeFile *f,NpeWorld *w,NpeSoundPlayer *p,uint32_t offset){
    SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);long_write(f,0x527872,offset);long_write(f,0x527856,26);
    w->music_enabled=1;npe_world_seed(w,seed_for(0));npe_world_sound_battle_start(w);
    assert(p->continuation==SOUND_CONTINUATION_BATTLE);while(p->wait_bytes)deliver(p,p->wait_bytes);
    deliver(p,1280);assert(p->phase==NPE_SOUND_END_WAIT && p->wait_bytes==3200);deliver(p,3199);assert(!p->end_pending);
}
int main(int argc,char **argv){
    assert(argc==2 && !SDL_Init(SDL_INIT_EVENTS));NpeFile f;battle_sound_file(argv[1],&f);
    uint32_t short_offset=(uint32_t)f.size;uint8_t *grown=realloc(f.bytes,f.size+26);assert(grown);f.bytes=grown;
    memcpy(f.bytes+f.size,f.bytes+f.sections[23].offset,25);f.bytes[f.size+2]=0;f.bytes[f.size+3]=1;f.bytes[f.size+25]=0;f.size+=26;
    NpeWorld w;assert(!npe_world_init(&f,&w,9,29,13));assert(!npe_world_resources_open(&f,&w));NpeSoundPlayer p;
    npe_sound_open(&p,&f);assert(p.device);SDL_PauseAudioDevice(p.device,1);npe_sound_select(&p,&w);game_sound_player=&p;
    uint8_t direct[1280],live[1280];
    for(unsigned i=0;i<4;++i){
        assert(!npe_sound_play_section(&p,16+i,0));prefix(&p,direct);w.music_enabled=1;
        if(i)npe_world_seed(&w,seed_for(i-1));
        npe_world_sound_scene_start(&w,i?2:1);
        assert(p.active==NPE_SOUND_TRACKS && p.selected_section==(int)(16+i) && !p.loop && !p.continuation);
        prefix(&p,live);assert(!memcmp(direct,live,sizeof(live)));
    }
    for(unsigned i=0;i<6;++i){
        assert(!npe_sound_play_section(&p,23+i,1));prefix(&p,direct);npe_world_seed(&w,seed_for(i*10));
        npe_world_sound_battle_start(&w);assert(p.active==NPE_SOUND_TRACKS && p.selected_section==(int)(23+i) && p.continuation==SOUND_CONTINUATION_BATTLE);
        prefix(&p,live);assert(!memcmp(direct,live,sizeof(live)));
    }
    /* Scene requests read current cache; battle selection stores current cache. */
    long_write(&f,0x5278d8,f.sections[19].offset);long_write(&f,0x5278c8,f.sections[19].size);
    npe_world_seed(&w,seed_for(0));npe_world_sound_scene_start(&w,2);assert(p.selected_section==19 && !p.loop);
    long_write(&f,0x52787e,f.sections[23].offset);long_write(&f,0x527862,f.sections[23].size);
    npe_world_seed(&w,seed_for(30));npe_world_sound_battle_start(&w);assert(p.selected_section==23);
    const uint16_t words[]={0,1,2,0x100,0x101,0xffff};
    for(unsigned i=0;i<6;++i){
        short_battle(&f,&w,&p,short_offset);w.music_enabled=words[i];
        long_write(&f,0x52799a,f.sections[24].offset);long_write(&f,0x527996,f.sections[24].size);
        /* Changing the directory alone must not override the selected pair. */
        long_write(&f,0x527872,f.sections[25].offset);long_write(&f,0x527856,f.sections[25].size);
        unsigned before=ranges;uint32_t random=w.random_state;deliver(&p,1);
        assert(p.end_pending==NPE_SOUND_CALLBACK_BATTLE);assert(game_WaitEventTimeout(&(SDL_Event){0},0)==0);
        assert(ranges==before+(words[i]!=0) && !p.end_pending && w.music_enabled==words[i] && w.random_state==random);
        if(words[i])assert(p.selected_section==24 && p.phase==NPE_SOUND_CLEANUP_WAIT && p.continuation==SOUND_CONTINUATION_BATTLE);
        else assert(p.phase==NPE_SOUND_STOPPED && p.finished && p.driver_configured);
    }
    /* Pending background callback cancelled by a finite scene. */
    short_battle(&f,&w,&p,short_offset);deliver(&p,1);unsigned before=ranges;
    w.music_enabled=1;npe_world_sound_scene_start(&w,1);assert(ranges==before+1 && !p.continuation && !p.end_pending && p.selected_section==16);
    assert(!npe_sound_pump(&p));
    /* Actual audio-thread EOF during a no-input UI wait. */
    for(unsigned disabled=0;disabled<2;++disabled){
        SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);long_write(&f,0x527872,short_offset);long_write(&f,0x527856,26);
        w.music_enabled=1;npe_world_seed(&w,seed_for(0));npe_world_sound_battle_start(&w);w.music_enabled=disabled?0:0x101;
        long_write(&f,0x52799a,f.sections[24].offset);long_write(&f,0x527996,f.sections[24].size);before=ranges;
        SDL_PauseAudioDevice(p.device,0);SDL_Event e;assert(!game_WaitEventTimeout(&e,450));SDL_PauseAudioDevice(p.device,1);
        assert(ranges==before+(disabled==0) && !p.end_pending);
        if(disabled)assert(p.phase==NPE_SOUND_STOPPED && p.finished);else assert(p.selected_section==24);
    }
    game_sound_player=NULL;npe_sound_close(&p);npe_free(&f);SDL_Quit();
    puts("SDL4 scene/6 battle candidate PCM prefixes,current caches/selected pair,6 full-word EOF gates,finite replacement and2 actual audio-thread UI handoffs passed");return 0;
}
