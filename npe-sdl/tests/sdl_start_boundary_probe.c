#define SDL_MAIN_HANDLED
#include "../sound_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Explicit post-prefill flags exercise the original start boundary without
   claiming the candidate codec produces these exhaustion/busy states. */
static uint16_t prepared_flags;
static unsigned preparation_calls;
static int original_pcm_service(NpeAudioPcmService *service,NpeAudioInput *input,
    NpeAudioOutput *output,NpeAudioClock *clock,const NpeAudioPcmBackend *backend,void *context){
    int result=npe_audio_pcm_service(service,input,output,clock,backend,context);
    assert(result==2);
    input->flags89=prepared_flags;
    ++preparation_calls;
    return result;
}
#define npe_audio_pcm_service original_pcm_service
#include "../sound_sdl.c"
#undef npe_audio_pcm_service

static void drain_wait(NpeSoundPlayer *player){
    while(player->wait_bytes){
        uint8_t quiet[511];size_t count=player->wait_bytes<sizeof(quiet)?player->wait_bytes:sizeof(quiet);
        sound_callback(player,quiet,(int)count);
        for(size_t i=0;i<count;++i)assert(!quiet[i]);
    }
}
int main(int argc,char **argv){
    SDL_SetMainReady();assert(argc==3);
    FILE *stream=fopen(argv[1],"rb");assert(stream);
    assert(!fseek(stream,0,SEEK_END));long size=ftell(stream);assert(size>0);rewind(stream);
    uint8_t *bytes=malloc((size_t)size);assert(bytes);
    assert(fread(bytes,1,(size_t)size,stream)==(size_t)size&&!fclose(stream));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    uint16_t states[8][6];stream=fopen(argv[2],"rb");assert(stream);
    assert(fread(states,1,sizeof(states),stream)==sizeof(states)&&fgetc(stream)==EOF&&!fclose(stream));
    assert(!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(0));
    NpeSoundPlayer player;npe_sound_open(&player,&file);assert(player.device);
    SDL_PauseAudioDevice(player.device,1);
    unsigned failed=0;
    for(unsigned case_index=0;case_index<8;++case_index){
        (void)npe_sound_play_section(&player,0,0);drain_wait(&player);
        player.decoder.input.flags89=0;
        player.gain=(NpeAudioGain){states[case_index][1]?0:7,states[case_index][1]?0:7};
        prepared_flags=states[case_index][0];preparation_calls=0;
        assert(!npe_sound_play_section(&player,20,!!(prepared_flags&0x8000u)));
        unsigned differences=0;
        differences+=preparation_calls!=1;
        differences+=player.decoder.input.flags89!=states[case_index][2];
        differences+=(player.decoder.input.flags8a&0x80u)!=states[case_index][3];
        differences+=player.wait_bytes!=(size_t)states[case_index][5]*NPE_AUDIO_OUTPUT_RATE*2/1000;
        differences+=player.phase!=(states[case_index][1]?NPE_SOUND_PLAYING:NPE_SOUND_START_WAIT);
        differences+=!player.driver_configured||player.cursor||player.pcm_halves_pending!=3;
        uint64_t clock=player.clock.value;NpeAudioGain gain=player.gain;
        uint32_t generation=player.request_generation;
        failed+=differences!=0;
        drain_wait(&player);
        assert(player.phase==NPE_SOUND_PLAYING&&player.transport_running);
        assert(player.request_complete==generation&&player.cursor==0);
        assert(player.decoder.input.flags89==states[case_index][2] || differences);
        assert(player.clock.value==clock&&!memcmp(&player.gain,&gain,sizeof(gain)));
        assert(preparation_calls==1&&player.pcm_halves_pending==3);
        printf("start boundary %u: flags=%04x; %u original Timer2 ticks; differences=%u\n",
            case_index,player.decoder.input.flags89,states[case_index][5],differences);
    }
    npe_sound_close(&player);npe_free(&file);SDL_Quit();
    printf("Original start flags:8 post-prefill boundaries; %u failed; retained callback/control flags, mute bypass, split wait bytes and foreground completion checked\n",failed);
    return failed?1:0;
}
