#define SDL_MAIN_HANDLED
#include "../sound_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned pcm_calls,producer_calls,raise_request,event_count,events[256];
static int observed_pcm(NpeAudioPcmService *service,NpeAudioInput *input,
    NpeAudioOutput *output,NpeAudioClock *clock,const NpeAudioPcmBackend *backend,void *context){
    if(!(input->flags89&1u)){
        assert(service->flags8e==0xc000u&&!output->half_samples);
        return npe_audio_pcm_service(service,input,output,clock,backend,context);
    }
    assert(input->flags89&1u);assert(!(service->flags8e&2u));++pcm_calls;assert(event_count<256);events[event_count++]=0;
    int result=npe_audio_pcm_service(service,input,output,clock,backend,context);
    assert(input->flags89&1u);return result;
}
static int observed_producer(NpeAudioInput *input){
    assert((input->flags89&0x44u)==4u);++producer_calls;assert(event_count<256);events[event_count++]=2;
    int result=npe_audio_input_candidate_produce(input);
    assert(input->flags89&4u);
    if(raise_request){input->flags89|=0x40u;raise_request=0;}
    return result;
}
#define npe_audio_pcm_service observed_pcm
#define npe_audio_input_candidate_produce observed_producer
#include "../sound_sdl.c"
#undef npe_audio_pcm_service
#undef npe_audio_input_candidate_produce
static void quiet(NpeSoundPlayer *p,size_t count){
    uint8_t bytes[509];while(count){size_t n=count<sizeof(bytes)?count:sizeof(bytes);
        sound_callback(p,bytes,(int)n);for(size_t i=0;i<n;++i)assert(!bytes[i]);count-=n;}
}
static void begin(NpeSoundPlayer *p){
    p->irq_flags8d=0;p->decoder.input.flags89&=(uint16_t)~7u;
    assert(!npe_sound_play_section(p,16,0));while(p->wait_bytes)quiet(p,p->wait_bytes);
    assert(p->phase==NPE_SOUND_PLAYING&&p->pcm_halves_pending==3u);
    assert(p->decoder.input.initialized&&npe_audio_clock_ms(&p->clock)==20);
    /* Consume both primed halves before exercising running refill gates. */
    uint8_t bytes[1280];sound_callback(p,bytes,sizeof(bytes));
    assert(p->decoder.input.initialized&&p->cursor==320&&!p->sample_pending);
    assert((p->decoder.input.flags89&0x1007u)==0x1000u&&!p->decode_failed);
}
static void blocked(NpeSoundPlayer *p){
    NpeAudioMode5Decoder decoder=p->decoder;NpeAudioClock clock=p->clock;
    size_t cursor=p->cursor;unsigned gain=p->gain.current,before_pcm=pcm_calls,before_producer=producer_calls;
    quiet(p,2049);
    /*405A/4070 now update only the suspension/window-request control
       bits during these unavailable-output retries. Codec state stays put. */
    NpeAudioMode5Decoder after=p->decoder;
    after.input.flags89&=(uint16_t)~0x420u;decoder.input.flags89&=(uint16_t)~0x420u;
    assert(p->cursor==cursor&&!memcmp(&after,&decoder,sizeof(decoder)));
    assert(!memcmp(&p->clock,&clock,sizeof(clock))&&p->gain.current==gain);
    assert(pcm_calls==before_pcm&&producer_calls==before_producer&&!p->decode_failed);
    assert(p->phase==NPE_SOUND_PLAYING);
}
int main(int argc,char **argv){
    SDL_SetMainReady();assert(argc==2);FILE *input=fopen(argv[1],"rb");assert(input);
    assert(!fseek(input,0,SEEK_END));long size=ftell(input);assert(size>0);rewind(input);
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,input)==(size_t)size);fclose(input);
    NpeFile file;char error[160];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    assert(file.sections[16].size>16384);
    NpeSoundPlayer p;npe_sound_open(&p,&file);assert(p.device);SDL_PauseAudioDevice(p.device,1);
    for(unsigned guard=0;guard<3;++guard){
        begin(&p);uint16_t mask=guard==0?1u:guard==1?0x1000u:0x10u;
        if(guard==0)p.decoder.input.flags89|=mask;
        else p.decoder.input.flags89&=(uint16_t)~mask;
        blocked(&p);
        if(guard==0)p.decoder.input.flags89&=(uint16_t)~mask;
        else p.decoder.input.flags89|=mask;
        uint8_t sample;sound_callback(&p,&sample,1);
        assert(p.cursor==322&&!(p.decoder.input.flags89&1u)&&!p.decode_failed);
    }
    for(unsigned guard=0;guard<3;++guard){
        begin(&p);p.decoder.input.flags89|=0x4040u;p.decoder.input.flags8a|=1u;
        p.irq_flags8d=guard==0?1u:guard==1?4u:5u;
        unsigned before=producer_calls;uint8_t samples[640];sound_callback(&p,samples,sizeof(samples));
        assert(producer_calls==before&&(p.decoder.input.flags89&0x40u)&&p.cursor==480&&!p.decode_failed);
        p.irq_flags8d=0;unsigned event_start=event_count;sound_callback(&p,samples,1);
        assert(events[event_start]==0&&events[event_start+1]==2);
        assert(producer_calls==before+1&&!(p.decoder.input.flags89&0x44u)&&p.cursor==482&&!p.decode_failed);
    }
    for(unsigned resume=0;resume<2;++resume){
        begin(&p);p.decoder.input.flags89|=(uint16_t)(0x44u|(resume?0x400u:0));
        p.decoder.input.flags8a|=0x40u;
        p.decoder.input.flags89&=(uint16_t)~0x10u;blocked(&p);
        /*3EC2's busy-starvation route has priority over its resume bit. */
        assert(npe_audio_service_begin(&p.decoder.input.flags89,p.decoder.input.flags8a,0,
            NULL,NPE_AUDIO_SERVICE_PRODUCER)==NPE_AUDIO_SERVICE_YIELD);
        p.decoder.input.flags89&=(uint16_t)~4u;
        uint8_t sample;
        if(p.decoder.input.flags89&0x400u){
            sound_callback(&p,&sample,1);
            assert(p.transport_running&&!(p.decoder.input.flags89&0x400u)&&p.cursor==320);
        }
        sound_callback(&p,&sample,1);
        assert(p.cursor==320&&(p.decoder.input.flags89&0x10u));
        sound_callback(&p,&sample,1);assert(p.cursor==322&&!p.decode_failed);
    }
    /* Actual suspension replaces the earlier captured-resume fixture:
       original4070 restarts an already prepared half automatically. */
    begin(&p);p.decoder.input.flags89|=0x44u;p.decoder.input.flags8a|=0x40u;
    unsigned before_pcm=pcm_calls;uint8_t output[641];
    memset(output,0xaa,sizeof(output));sound_callback(&p,output,sizeof(output));
    for(unsigned i=0;i<sizeof(output);++i)assert(!output[i]);
    assert(p.pcm_halves_pending&&!p.transport_running&&(p.decoder.input.flags89&0x400u));
    assert(p.cursor==320&&pcm_calls==before_pcm+1);
    p.decoder.input.flags89&=(uint16_t)~4u;
    uint8_t sample;sound_callback(&p,&sample,1);
    assert(p.cursor==322&&pcm_calls==before_pcm+1&&p.transport_running&&!(p.decoder.input.flags89&0x400u));
    uint8_t tail[639];sound_callback(&p,tail,sizeof(tail));
    assert(p.cursor==480&&!p.pcm_halves_pending&&!p.pcm_service.flags8e);
    sound_callback(&p,&sample,1);
    assert(p.cursor==482&&!p.decode_failed&&npe_audio_clock_ms(&p.clock)==40);
    begin(&p);p.decoder.input.flags89|=0x40u;p.decoder.input.flags89&=(uint16_t)~0x4000u;
    unsigned before=producer_calls;sound_callback(&p,&sample,1);
    assert(producer_calls==before&&(p.decoder.input.flags89&0x40u));
    /* End clears only activity; a fresh request during the call survives. */
    begin(&p);p.decoder.input.flags89|=0x4040u;raise_request=1;
    assert(sound_service_producer(&p)==NPE_AUDIO_SERVICE_READY);
    assert((p.decoder.input.flags89&0x44u)==0x40u);
    before=producer_calls;assert(sound_service_producer(&p)==NPE_AUDIO_SERVICE_READY);
    assert(producer_calls==before+1&&!(p.decoder.input.flags89&0x44u));
    assert(pcm_calls&&producer_calls);
    printf("SDL callback gates:11 current-NPE paths; actual PCM/producer entry flags, registration/output requests, busy/storage/starvation/resume blocking, recovery and live re-request passed (%u PCM/%u producer calls); no physical IRQ or original codec claim\n",pcm_calls,producer_calls);
    npe_sound_close(&p);npe_free(&file);SDL_Quit();return 0;
}
