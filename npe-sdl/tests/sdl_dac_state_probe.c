#define SDL_MAIN_HANDLED
#include "../sound_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../sound_sdl.c"

typedef struct {
    const char *name;unsigned flags88,flags8a;int first,saved_l,saved_r,current_l,current_r;
    unsigned control,gpio,count,calls[2];int after_l,after_r,saved_after_l,saved_after_r;
    unsigned control_after,gpio_after,ticks;
} DacStateNative;
#include "audio_dac_state_native.h"

static void check_native_states(void){
    for(unsigned i=0;i<sizeof(dac_state_native)/sizeof(*dac_state_native);++i){
        const DacStateNative *n=dac_state_native+i;NpeSoundPlayer p={0};
        p.active=-1;p.finished=1;p.dac.current[0]=(int16_t)n->current_l;p.dac.current[1]=(int16_t)n->current_r;
        p.dac_saved[0]=(int16_t)n->saved_l;p.dac_saved[1]=(int16_t)n->saved_r;
        p.dac_control=(uint16_t)n->control;p.dac_gpio=(uint16_t)n->gpio;
        p.decoder.input.flags8a=(uint16_t)n->flags8a;unsigned flags=n->flags88,ticks=0;
        for(unsigned j=0;j<n->count;++j){
            unsigned call=n->calls[j];
            if(call==0x3262){
                if(flags&4u)continue;
                sound_wait(&p,NPE_SOUND_CLEANUP_WAIT,50);ticks+=50;
                assert(p.dac.current[0]==0&&p.dac.current[1]==0&&!(p.dac_gpio&0x40u));
                p.wait_bytes=0;sound_wait_complete(&p);flags=4;
            }else{
                assert(call==0x31ac||call==0x320e);
                if(call==0x31ac&&(flags&1u))continue;
                if(call==0x320e&&!(flags&2u))continue;
                unsigned wait=sound_dac_start_begin(&p,(int16_t)n->first,call==0x320e||(flags&0x10u));
                ticks+=wait;sound_wait(&p,NPE_SOUND_START_WAIT,wait);
                assert(p.dac.current[0]==0&&p.dac.current[1]==0);
                p.wait_bytes=0;sound_wait_complete(&p);flags=(flags|1u)&~6u;
            }
        }
        unsigned bad=p.dac.current[0]!=n->after_l||p.dac.current[1]!=n->after_r||
            p.dac_saved[0]!=n->saved_after_l||p.dac_saved[1]!=n->saved_after_r||
            p.dac_control!=n->control_after||p.dac_gpio!=n->gpio_after||ticks!=n->ticks;
        fprintf(stderr,"Original DAC state %s: %u differences\n",n->name,bad);assert(!bad);
    }
}
static void drain(NpeSoundPlayer *p){
    while(p->wait_bytes){uint8_t bytes[511];size_t count=p->wait_bytes<sizeof(bytes)?p->wait_bytes:sizeof(bytes);
        sound_callback(p,bytes,(int)count);for(size_t i=0;i<count;++i)assert(!bytes[i]);}
}
static void check_delivery(NpeSoundPlayer *p){
    assert(!npe_sound_play_section(p,20,0));drain(p);
    assert(p->dac_saved[0]==p->half[0][0]&&p->dac_saved[1]==p->half[0][0]);
    assert(p->dac.current[0]==p->dac_saved[0]&&p->dac.current[1]==p->dac_saved[1]);
    assert((p->dac_control&0x8001u)==1&&(p->dac_gpio&0x40u));
    /* Deliver through real current-NPE PCM until an observable nonzero
       word, then split the next word across callback boundaries. */
    int16_t sample=0;uint8_t bytes[2];unsigned delivered=0;
    while(!sample&&delivered<1280){sound_callback(p,bytes,2);memcpy(&sample,bytes,2);++delivered;}
    assert(sample&&p->dac.current[0]==sample&&p->dac.current[1]==sample);
    int16_t retained=p->dac.current[0];
    sound_callback(p,bytes,1);assert(p->dac.current[0]==retained&&p->dac.current[1]==retained);
    sound_callback(p,bytes+1,1);memcpy(&sample,bytes,2);
    assert(p->dac.current[0]==sample&&p->dac.current[1]==sample);
    /* A replacement records the last complete delivered word, not the
       pending source cursor. Cleanup then initializes a fresh first word. */
    assert(!npe_sound_play_section(p,23,0));
    assert(p->phase==NPE_SOUND_CLEANUP_WAIT&&p->wait_bytes==3200);
    assert(p->dac_saved[0]==sample&&p->dac_saved[1]==sample);
    assert(!p->dac.current[0]&&!p->dac.current[1]&&!(p->dac_gpio&0x40u));
    assert(p->dac_ramp_nops==(uint64_t)(sample<0?-(int)sample:sample)*96u);
    drain(p);assert(p->phase==NPE_SOUND_PLAYING);
    assert(p->dac_saved[0]==p->half[0][0]&&p->dac_saved[1]==p->half[0][0]);
    assert(p->dac.current[0]==p->dac_saved[0]&&p->dac.current[1]==p->dac_saved[1]);
    (void)npe_sound_play_section(p,0,0);drain(p);
    assert(p->phase==NPE_SOUND_STOPPED&&!p->driver_configured&&(p->dac_control&0x8001u)==0x8000u);
    int16_t saved[2];memcpy(saved,p->dac_saved,sizeof(saved));
    (void)npe_sound_play_section(p,0,0);assert(!p->wait_bytes&&!memcmp(saved,p->dac_saved,sizeof(saved)));
    /* Startup mute clears/enables DAC but skips restoration; stop still
       follows its ordinary fifty-count path. */
    p->gain=(NpeAudioGain){0,0};p->decoder.input.flags8a|=0x80u;
    assert(!npe_sound_play_section(p,20,0)&&!p->wait_bytes);
    assert(!p->dac.current[0]&&!p->dac.current[1]&&(p->dac_control&0x8001u)==1u);
    assert(!(p->dac_gpio&0x40u)&&!p->dac_ramp_nops);
    (void)npe_sound_play_section(p,0,0);assert(p->wait_bytes==3200);drain(p);
    printf("SDL DAC delivery: current-NPE first target, complete/odd word retention, replacement, repeated stop and mute passed\n");
}
int main(int argc,char **argv){
    SDL_SetMainReady();assert(argc==2);check_native_states();
    FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long length=ftell(in);assert(length>0);rewind(in);
    uint8_t *bytes=malloc((size_t)length);assert(bytes&&fread(bytes,1,(size_t)length,in)==(size_t)length&&!fclose(in));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)length,error,sizeof(error)));
    assert(!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(0));NpeSoundPlayer p;npe_sound_open(&p,&file);assert(p.device);
    SDL_PauseAudioDevice(p.device,1);check_delivery(&p);npe_sound_close(&p);npe_free(&file);SDL_Quit();
    puts("10 original DAC states and actual SDL state lifecycle passed; audible ramp timing remains outside this check");return 0;
}
