#define SDL_MAIN_HANDLED
#include "../sound_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned pcm_calls,producer_calls;
static int observed_pcm(NpeAudioPcmService *s,NpeAudioInput *i,NpeAudioOutput *o,
    NpeAudioClock *c,const NpeAudioPcmBackend *b,void *x){
    if(!(i->flags89&1u)){
        assert(s->flags8e==0xc000u&&!o->half_samples);
        return npe_audio_pcm_service(s,i,o,c,b,x);
    }
    assert(i->flags89&1u);++pcm_calls;return npe_audio_pcm_service(s,i,o,c,b,x);
}
static int observed_producer(NpeAudioInput *input){
    assert((input->flags89&0x44u)==4u);++producer_calls;
    return npe_audio_input_candidate_produce(input);
}
#define npe_audio_pcm_service observed_pcm
#define npe_audio_input_candidate_produce observed_producer
#include "../sound_sdl.c"
#undef npe_audio_pcm_service
#undef npe_audio_input_candidate_produce
static void drain(NpeSoundPlayer *p){
    uint8_t buffer[511];while(p->wait_bytes){size_t n=p->wait_bytes<sizeof(buffer)?p->wait_bytes:sizeof(buffer);
        sound_callback(p,buffer,(int)n);for(size_t i=0;i<n;++i)assert(!buffer[i]);}
}
static void begin(NpeSoundPlayer *p){
    p->irq_flags8d=0;p->decoder.input.flags89&=(uint16_t)~7u;
    assert(!npe_sound_play_section(p,16,0));drain(p);
    assert(p->transport_running&&p->transport_controls[0]==0x8140&&p->transport_controls[1]==0x8140);
    /* Consume the two initially retained halves before running refill. */
    uint8_t buffer[1280];sound_callback(p,buffer,sizeof(buffer));
    assert(p->cursor==320&&!p->pcm_halves_pending&&!p->sample_pending&&!p->decode_failed);
    assert(p->transport_controls[0]==0x8000&&p->transport_controls[1]==0x8000);
}
static void once_quiet(NpeSoundPlayer *p,int count){
    uint8_t buffer[2049];assert(count<=(int)sizeof(buffer));
    memset(buffer,0xaa,(size_t)count);sound_callback(p,buffer,count);
    for(int i=0;i<count;++i)assert(!buffer[i]);
}
int main(int argc,char **argv){
    SDL_SetMainReady();assert(argc==2);FILE *input=fopen(argv[1],"rb");assert(input);
    assert(!fseek(input,0,SEEK_END));long size=ftell(input);assert(size>0);rewind(input);
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,input)==(size_t)size);fclose(input);
    NpeFile file;char error[160];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    assert(file.sections[16].size>16384);
    NpeSoundPlayer p;npe_sound_open(&p,&file);assert(p.device);SDL_PauseAudioDevice(p.device,1);
    unsigned paths=0;
    /* Exact host byte accounting across odd partitions, separate from PCM
       clock progression and from the original physical dual DTU counters. */
    begin(&p);uint8_t buffer[641];sound_callback(&p,buffer,1);
    assert(p.transport_controls[0]==0x8140&&p.transport_controls[1]==0x8140);
    sound_callback(&p,buffer,1);assert(p.transport_controls[0]==0x813f);
    sound_callback(&p,buffer,637);assert(p.transport_controls[0]==0x8001);
    sound_callback(&p,buffer,1);assert(p.transport_controls[0]==0x8000&&!p.pcm_halves_pending);++paths;
    for(unsigned busy=1;busy<=3;++busy){
        begin(&p);assert(!npe_audio_transport_suspend(&p.decoder.input.flags89,0x8040,&sound_transport_backend,&p));
        p.decoder.input.flags89|=(uint16_t)busy;
        unsigned before=pcm_calls;uint64_t clock=p.clock.value;
        once_quiet(&p,641);assert(!p.transport_running&&(p.decoder.input.flags89&0x400u));
        assert(p.cursor==320&&p.clock.value==clock&&pcm_calls==before);
        p.decoder.input.flags89&=(uint16_t)~busy;
        sound_callback(&p,buffer,1);
        assert(p.transport_running&&!(p.decoder.input.flags89&0x400u)&&p.cursor==322);
        assert(pcm_calls==before+1&&!(p.pcm_service.flags8e&2u)&&!p.decode_failed);++paths;
    }
    /* A later yield keeps the prepared half. Original4070 may restart it
       while producer2 remains active; only activity0/1 blocks this route. */
    begin(&p);p.decoder.input.flags89|=0x44u;p.decoder.input.flags8a|=0x40u;
    unsigned before=pcm_calls;once_quiet(&p,641);
    assert(p.pcm_halves_pending&&!p.transport_running&&p.cursor==320&&pcm_calls==before+1);
    assert(p.decoder.input.flags89&0x400u);
    sound_callback(&p,buffer,1);
    assert(p.transport_running&&p.cursor==322&&pcm_calls==before+1&&(p.decoder.input.flags89&4u));
    assert(!(p.decoder.input.flags89&0x400u)&&(p.decoder.input.flags89&0x20u));
    sound_callback(&p,buffer,639);assert(p.cursor==480&&!p.pcm_halves_pending);++paths;
    /* Missing output remains quiet; foreground3994 bypasses starvation,
       but respects all file/SPI guard bits and the active producer. */
    for(unsigned guard=0;guard<4;++guard){
        begin(&p);p.decoder.input.flags89|=0x4040u;p.decoder.input.flags89&=(uint16_t)~0x10u;
        p.decoder.input.flags8a|=0x41u;p.irq_flags8d=(uint16_t)(guard==0?0:guard==1?1:guard==2?4:5);
        unsigned before_producer=producer_calls;before=pcm_calls;uint64_t clock=p.clock.value;
        assert(!npe_sound_pump(&p));
        if(guard){assert(producer_calls==before_producer&&(p.decoder.input.flags89&0x40u));
            p.irq_flags8d=0;assert(!npe_sound_pump(&p));}
        assert(producer_calls==before_producer+1&&(p.decoder.input.flags89&0x10u));
        assert(!(p.decoder.input.flags89&0x44u)&&!(p.decoder.input.flags8a&0x40u));
        assert(p.cursor==320&&p.clock.value==clock&&pcm_calls==before);
        sound_callback(&p,buffer,1);assert(p.cursor==322&&!p.decode_failed);++paths;
    }
    begin(&p);p.decoder.input.flags89|=0x4044u;p.decoder.input.flags8a|=0x41u;
    unsigned before_producer=producer_calls;assert(!npe_sound_pump(&p));assert(producer_calls==before_producer);
    p.decoder.input.flags89&=(uint16_t)~4u;assert(!npe_sound_pump(&p));
    assert(producer_calls==before_producer+1&&!(p.decoder.input.flags89&4u));++paths;
    /* Foreground producer returns through4070; queue PCM on the audio
       thread without decoding while the game thread holds its audio lock. */
    begin(&p);p.decoder.input.flags89|=0x4440u;p.decoder.input.flags8a|=0x41u;
    p.transport_running=0;before=pcm_calls;before_producer=producer_calls;
    assert(!npe_sound_pump(&p));assert(producer_calls==before_producer+1&&pcm_calls==before);
    assert(p.transport_running&&!(p.decoder.input.flags89&0x400u)&&(p.pcm_service.flags8e&2u));
    sound_callback(&p,buffer,1);assert(p.cursor==322&&pcm_calls==before+1&&!(p.pcm_service.flags8e&2u));++paths;
    /* Replacing suspended playback discards its prepared host half. */
    begin(&p);p.decoder.input.flags89|=0x44u;p.decoder.input.flags8a|=0x40u;
    once_quiet(&p,641);assert(p.pcm_halves_pending&&!p.transport_running);
    p.decoder.input.flags89&=(uint16_t)~4u;assert(!npe_sound_play_section(&p,17,0));
    assert(!p.pcm_halves_pending&&!p.transport_running);drain(&p);
    assert(p.transport_controls[0]==0x8140&&p.transport_running&&!p.cursor);
    sound_callback(&p,buffer,1);assert(p.cursor==2&&!p.decode_failed&&p.decoder.input.resource.bytes==p.resources[1].bytes);++paths;
    assert(paths==12);
    printf("SDL transport:12 current-NPE paths; odd-byte counters, busy0/1 recovery, prepared-half restart, live foreground producer/file guards, queued resume PCM and replacement passed (%u PCM/%u producer calls); host transfer adapter, no physical DTU or original codec claim\n",pcm_calls,producer_calls);
    npe_sound_close(&p);npe_free(&file);SDL_Quit();return 0;
}
