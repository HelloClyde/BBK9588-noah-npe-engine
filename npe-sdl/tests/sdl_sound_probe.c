#include "../sound_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise the same callback supplied to SDL, including exact tail buffers. */
static SDL_AudioDeviceID checked_device(const char *,int,const SDL_AudioSpec *,SDL_AudioSpec *,int);
#define SDL_OpenAudioDevice checked_device
#include "../sound_sdl.c"
#undef SDL_OpenAudioDevice
static SDL_AudioDeviceID checked_device(const char *name,int capture,const SDL_AudioSpec *wanted,SDL_AudioSpec *obtained,int changes){
    assert(wanted->freq==32000&&wanted->channels==1&&wanted->format==AUDIO_S16SYS&&!changes);
    SDL_AudioDeviceID device=SDL_OpenAudioDevice(name,capture,wanted,obtained,changes);
    assert(device&&obtained->freq==32000&&obtained->channels==1&&obtained->format==AUDIO_S16SYS);return device;
}
typedef struct {unsigned current,target,count,after_current,after_target;int16_t in0,in1,out0,out1;} GainNative;

typedef struct {
    const char *name;unsigned configure,rate,words,advances,retracts;
    uint32_t add_ms,milliseconds;uint64_t value;uint32_t increment;
    unsigned count;NpeAudioClock trace[8];
} AudioClockNative;
#include "audio_clock_native.h"
static void check_clock_step(const AudioClockNative *n,unsigned index,const NpeAudioClock *clock){
    assert(index<n->count);
    if(clock->value!=n->trace[index].value||clock->increment!=n->trace[index].increment)
        fprintf(stderr,"PRG clock %s checkpoint%u differs\n",n->name,index);
    assert(clock->value==n->trace[index].value&&clock->increment==n->trace[index].increment);
}
static void check_audio_clock(void){
    for(unsigned i=0;i<sizeof(audio_clock_native)/sizeof(*audio_clock_native);++i){
        const AudioClockNative *n=&audio_clock_native[i];
        NpeAudioClock clock={n->value,n->increment};unsigned step=0;
        check_clock_step(n,step++,&clock);
        if(n->configure){
            assert(!npe_audio_clock_configure(&clock,(uint16_t)n->rate,(uint16_t)n->words));
            check_clock_step(n,step++,&clock);
        }
        for(unsigned j=0;j<n->advances;++j){npe_audio_clock_advance(&clock);check_clock_step(n,step++,&clock);}
        npe_audio_clock_add_ms(&clock,n->add_ms);check_clock_step(n,step++,&clock);
        for(unsigned j=0;j<n->retracts;++j){npe_audio_clock_retract(&clock);check_clock_step(n,step++,&clock);}
        assert(step==n->count&&npe_audio_clock_ms(&clock)==n->milliseconds);
    }
    /* Host API guards, not reference evidence for guest divide-by-zero. */
    NpeAudioClock clock={12345,678};
    assert(npe_audio_clock_configure(&clock,0,80)==-1&&clock.value==12345&&clock.increment==678);
    assert(npe_audio_clock_configure(NULL,32000,320)==-1&&!npe_audio_clock_ms(NULL));
    npe_audio_clock_advance(NULL);npe_audio_clock_retract(NULL);npe_audio_clock_add_ms(NULL,1);
}
typedef struct {uint8_t raw[6];int returned;uint16_t metadata[10];} AudioHeaderNative;
#include "audio_header_native.h"
typedef struct {
    uint16_t count,rate;uint32_t start,length;
    NpeAudioCursor positions[7];uint16_t samples[3];uint32_t expected[7];
} AudioPlanNative;
#include "audio_stream_plan_native.h"
static void check_stream_plans(void){
    for(unsigned i=0;i<sizeof(audio_plan_native)/sizeof(*audio_plan_native);++i){
        const AudioPlanNative *n=&audio_plan_native[i];NpeAudioHeader h={0};NpeAudioStreamPlan p;
        h.count=n->count;h.bitrate=n->rate;
        assert(!npe_audio_stream_plan(&h,n->positions,n->samples,n->start,n->length,&p));
        uint32_t actual[]={p.header_bits,p.frame_bits,p.decoded_samples,p.total_ms,p.elapsed_ms,p.start_bits,p.length_bits};
        if(memcmp(actual,n->expected,sizeof(actual)))fprintf(stderr,"stream plan native case%u mismatch\n",i);
        assert(!memcmp(actual,n->expected,sizeof(actual)));
    }
}
static void check_audio_headers(void){
    for(unsigned i=0;i<sizeof(audio_header_native)/sizeof(*audio_header_native);++i){
        const AudioHeaderNative *n=&audio_header_native[i];NpeAudioHeader h;
        assert(npe_audio_header(n->raw,sizeof(n->raw),&h)==n->returned);
        uint16_t actual[]={h.type_revision,h.format,h.count,h.control2e,h.source_rate,
            h.control32,h.control34,h.control36,h.bitrate,h.output_rate};
        if(memcmp(actual,n->metadata,sizeof(actual)))fprintf(stderr,"original audio header case%u mismatch\n",i);
        assert(!memcmp(actual,n->metadata,sizeof(actual)));
    }
}
#include "audio_gain_native.h"
#include "audio_eof_native.h"
typedef struct {
    unsigned consumed,active,next,debt,frame_bits,flags8a,file_remaining;
    int eof_delta;
    unsigned output_words,flags89,source_pointer,after_available,pending1,pending0,after_debt;
    uint32_t tail;
    unsigned after_flags89,after_flags8a;
    uint16_t window[512];
} AudioInputNative;
#include "audio_input_native.h"
typedef struct {
    unsigned flags8a,remaining,actual0,actual1,key,calls,request0,request1,read_calls;
    unsigned after_remaining,pending0,pending1,after_flags89,after_flags8a;
    uint64_t hashes[2];
} AudioProducerNative;
#include "audio_producer_native.h"
typedef struct {unsigned actual[2],request[2],calls;} ProducerReader;
static uint64_t pcm_hash(const void *bytes,size_t count);
typedef struct {
    unsigned index,half,output_words,decoded,current,target,calls;
    unsigned after_index,after_half,after_output_words,after_gain,after_flags8a;
    unsigned codec_calls,transfer_calls,transfer_words,configured_words;
    uint64_t hashes[2];
} AudioOutputNative;
#include "audio_output_native.h"
static void check_output_halves(void){
    static const int16_t pattern[]={INT16_MIN,INT16_MAX,-30000,30000,-1,0,1,1000};
    for(unsigned ci=0;ci<sizeof(audio_output_native)/sizeof(*audio_output_native);++ci){
        const AudioOutputNative *n=&audio_output_native[ci];
        NpeAudioOutput state={(uint16_t)n->index,(uint16_t)n->half,(uint16_t)n->output_words};
        NpeAudioGain gain={(uint8_t)n->current,(uint8_t)n->target};uint16_t flags=0x48c3;
        int16_t pcm[176],half[88],repeated[648];unsigned codecs=0,transfers=0,last=65535,configured=0;
        for(unsigned i=0;i<176;++i)pcm[i]=pattern[i%8];
        for(unsigned i=0;i<648;++i)repeated[i]=0x5aa5;
        for(unsigned call=0;call<n->calls;++call){
            NpeAudioOutput before=state;
            if(before.half_index>=2){
                ++codecs;
                unsigned length=(n->decoded>>1)*4;
                if(!n->decoded||length!=before.output_words)configured=length;
            }
            int result=npe_audio_output_half(&state,&gain,&flags,pcm,176,(uint16_t)n->decoded,half,88);
            assert(result>=0);
            if(!result)continue;
            unsigned offset=(state.half_index-1)*state.half_samples;
            /* Original gain mutates that source half. Preserve its effects
               here when the diagnostic codec boundary doesn't refill it. */
            for(unsigned i=0;i<state.half_samples;++i){
                pcm[offset+i]=half[i];
                for(unsigned j=0;j<4;++j)repeated[i*4+j]=half[i];
            }
            ++transfers;last=state.output_words;
        }
        assert(state.half_index==n->after_index&&state.half_samples==n->after_half&&state.output_words==n->after_output_words);
        assert(((unsigned)gain.current<<8|gain.target)==n->after_gain&&flags==n->after_flags8a);
        assert(codecs==n->codec_calls&&transfers==n->transfer_calls&&last==n->transfer_words&&configured==n->configured_words);
        if(pcm_hash(pcm,sizeof(pcm))!=n->hashes[0]||pcm_hash(repeated,sizeof(repeated))!=n->hashes[1])
            fprintf(stderr,"output half native case%u PCM/repeated buffer mismatch\n",ci);
        assert(pcm_hash(pcm,sizeof(pcm))==n->hashes[0]&&pcm_hash(repeated,sizeof(repeated))==n->hashes[1]);
    }
}
static size_t producer_read(void *context,unsigned source,uint16_t *destination,size_t requested){
    ProducerReader *r=context;unsigned count=r->actual[source];uint16_t pattern=source?0xabcd:0x1234;
    assert(source<2&&count<=requested);r->request[source]=(unsigned)requested;++r->calls;
    for(unsigned i=0;i<count/2;++i)destination[i]=pattern;
    if(count&1)destination[count/2]=(uint16_t)((destination[count/2]&255)|(pattern&0xff00));
    return count;
}

typedef struct {
    uint32_t remaining,start_bits,length_bits,header_bits,base,actual,key;
    uint32_t start_bytes,length_bytes,after_remaining,reset_calls,restore_calls,seek_calls;
    uint32_t read_calls,read_request,returned,reached_prime,absolute;
    uint64_t window_hash;
} AudioSeekNative;
#include "audio_seek_native.h"
static void check_seek_plans(void){
    for(unsigned ci=0;ci<sizeof(audio_seek_native)/sizeof(*audio_seek_native);++ci){
        const AudioSeekNative *n=&audio_seek_native[ci];NpeAudioStreamPlan stream={0};NpeAudioSeekPlan p;
        uint16_t window[512];ProducerReader reader={{n->actual,0},{0xffff,0xffff},0};
        stream.start_bits=n->start_bits;stream.length_bits=n->length_bits;stream.header_bits=(uint16_t)n->header_bits;
        for(unsigned i=0;i<512;++i)window[i]=0x5aa5;
        assert(!npe_audio_seek_plan(&stream,n->remaining,n->base,&p));
        assert(p.start_bytes==n->start_bytes&&p.length_bytes==n->length_bytes&&p.remaining==n->after_remaining);
        assert(p.seek_required==(int)n->seek_calls&&p.seek_offset==n->absolute&&p.read_request==n->read_request);
        assert(p.bit_remainder==(n->start_bits&15));
        int result=npe_audio_seek_window(&p,window,producer_read,&reader,(uint8_t)n->key);
        assert(result==(n->seek_calls?(n->actual?1:-2):0));
        assert((result==1)==(int)n->restore_calls&&(!p.seek_required||n->returned==(result==-2?65534:0)));
        assert(reader.calls==n->read_calls);
        if(reader.calls)assert(reader.request[0]==n->read_request);
        assert(p.remaining==n->after_remaining&&pcm_hash(window,sizeof(window))==n->window_hash);
    }
}
typedef struct {
    unsigned remaining,actual0,actual1,flags8a,samples,key;
    unsigned after_remaining,tail,available,pending0,pending1,output_words,after_flags89,after_flags8a;
    unsigned read_calls,request0,request1;uint64_t hashes[2];
} AudioPrimeNative;
#include "audio_prime_native.h"
static void check_input_prime(void){
    for(unsigned ci=0;ci<sizeof(audio_prime_native)/sizeof(*audio_prime_native);++ci){
        const AudioPrimeNative *n=&audio_prime_native[ci];NpeAudioInput input={0};
        ProducerReader reader={{n->actual0,n->actual1},{0xffff,0xffff},0};
        for(unsigned i=0;i<512;++i)input.window[i]=0x6bb6;
        for(unsigned i=0;i<2048;++i)input.source[0][i]=input.source[1][i]=0x5aa5;
        input.file_remaining=n->remaining;input.tail_pointer=0x8c00;
        input.pending[0]=17;input.pending[1]=19;input.available=31;
        input.flags89=0x4000;input.flags8a=(uint16_t)n->flags8a;input.plan.decoded_samples=(uint16_t)n->samples;
        assert(!npe_audio_input_prime(&input,producer_read,&reader,(uint8_t)n->key));
        assert(input.file_remaining==n->after_remaining&&input.tail_pointer==n->tail&&input.available==n->available);
        assert(input.pending[0]==n->pending0&&input.pending[1]==n->pending1&&input.initial_output_words==n->output_words);
        assert(input.flags89==n->after_flags89&&input.flags8a==n->after_flags8a);
        assert(reader.calls==n->read_calls&&reader.request[0]==n->request0&&reader.request[1]==n->request1);
        for(unsigned i=0;i<512;++i)assert(input.window[i]==0x6bb6);
        for(unsigned source=0;source<2;++source)assert(pcm_hash(input.source[source],4096)==n->hashes[source]);
    }
}
static void check_input_producer(void){
    for(unsigned case_index=0;case_index<sizeof(audio_producer_native)/sizeof(*audio_producer_native);++case_index){
        const AudioProducerNative *n=&audio_producer_native[case_index];NpeAudioInput input={0};
        ProducerReader reader={{n->actual0,n->actual1},{0xffff,0xffff},0};
        for(unsigned i=0;i<2048;++i)input.source[0][i]=input.source[1][i]=0x5aa5;
        input.pending[0]=17;input.pending[1]=19;input.file_remaining=n->remaining;
        input.flags8a=(uint16_t)n->flags8a;input.flags89=0x4040;
        for(unsigned i=0;i<n->calls;++i)assert(!npe_audio_input_produce(&input,producer_read,&reader,(uint8_t)n->key));
        assert(reader.calls==n->read_calls&&reader.request[0]==n->request0&&reader.request[1]==n->request1);
        assert(input.file_remaining==n->after_remaining&&input.pending[0]==n->pending0&&input.pending[1]==n->pending1);
        assert(input.flags89==n->after_flags89&&input.flags8a==n->after_flags8a);
        for(unsigned source=0;source<2;++source)assert(pcm_hash(input.source[source],4096)==n->hashes[source]);
    }
}
static void check_input_refill(void){
    for(unsigned case_index=0;case_index<sizeof(audio_input_native)/sizeof(*audio_input_native);++case_index){
        const AudioInputNative *n=&audio_input_native[case_index];NpeAudioInput input={0};
        for(unsigned i=0;i<512;++i)input.window[i]=(uint16_t)(0x1000+i);
        for(unsigned i=0;i<32;++i){input.source[0][i]=(uint16_t)(0x2000+i);input.source[1][i]=(uint16_t)(0x3000+i);}
        input.source_index=!!(n->flags8a&0x800);input.available=(uint16_t)n->active;
        input.pending[!input.source_index]=(uint16_t)n->next;input.debt=(uint16_t)n->debt;
        input.frame_bits=n->frame_bits;input.flags8a=(uint16_t)n->flags8a;input.flags89=(uint16_t)n->flags89;
        input.file_remaining=n->file_remaining;input.tail_pointer=0x8c00;
        assert(!npe_audio_input_refill(&input,n->consumed));
        assert(!memcmp(input.window,n->window,sizeof(input.window)));
        assert(0xd948+0x1000*input.source_index+input.source_offset*2==n->source_pointer);
        assert(input.available==n->after_available&&input.pending[1]==n->pending1&&input.pending[0]==n->pending0);
        assert(input.debt==n->after_debt&&input.tail_pointer==n->tail&&input.flags8a==n->after_flags8a);
        uint32_t cursor=input.tail_pointer+(uint32_t)n->eof_delta;
        npe_audio_input_check_end(&input,(uint16_t)n->output_words,cursor);
        assert(input.flags89==n->after_flags89);
    }
}
static void check_input_fields(const NpeFile *file){
    for(unsigned section=16;section<38;++section){
        NpeAudioResource resource;NpeAudioInput input;NpeAudioMode5Fields direct,buffered;
        assert(!npe_audio_resource(file,section,&resource)&&!npe_audio_input_open(&input,&resource));
        const AudioPlanNative *n=&audio_plan_native[section-16];
        assert(input.plan.header_bits==n->expected[0]&&input.frame_bits==n->expected[1]);
        assert(input.plan.total_ms==n->expected[3]&&input.plan.start_bits==n->expected[5]&&input.plan.length_bits==n->expected[6]);
        assert(input.initial_output_words==320);
        assert(!input.seek.seek_required&&input.seek.start_bytes==4);
        assert(input.seek.length_bytes==((input.plan.length_bits>>3)&0xfffffe)&&input.seek.remaining==input.seek.length_bytes);
        if(resource.size-4<=1024)assert(!(input.flags89&0x4010)&&input.flags8a==3);
        for(unsigned frame=0;frame<resource.header_count;++frame){
            assert(!npe_audio_mode5_fields(&resource,frame,&direct));
            assert(!npe_audio_input_fields(&input,&buffered));
            if(memcmp(&direct,&buffered,sizeof(direct)))fprintf(stderr,"input fields section%u frame%u\n",section,frame);
            assert(!memcmp(&direct,&buffered,sizeof(direct)));
        }
        assert(npe_audio_input_fields(&input,&buffered)==-1);
    }
}
static void check_gain(void){
    for(unsigned i=0;i<sizeof(gain_native)/sizeof(*gain_native);++i){const GainNative *n=&gain_native[i];
        NpeAudioGain g={(uint8_t)n->current,(uint8_t)n->target};int16_t v[4]={12345,n->in0,n->in1,-23456};
        assert(!npe_audio_gain_block(&g,v+1,n->count)&&g.current==n->after_current&&g.target==n->after_target);
        assert(v[0]==12345&&v[1]==n->out0&&v[2]==n->out1&&v[3]==-23456);
    }
}
static void repeat_word(uint8_t *out,int16_t word){for(unsigned i=0;i<4;++i)memcpy(out+i*2,&word,2);}
static void silence_output(NpeSoundPlayer *sound,size_t count){
    uint8_t output[127];
    while(count){size_t n=count<sizeof(output)?count:sizeof(output);
        memset(output,0xaa,sizeof(output));sound_callback(sound,output,(int)n);
        for(size_t i=0;i<n;++i)assert(!output[i]);count-=n;
    }
}
static void drain_start(NpeSoundPlayer *sound){
    assert(sound->phase==NPE_SOUND_START_WAIT||sound->phase==NPE_SOUND_CLEANUP_WAIT);
    int already_primed=sound->phase==NPE_SOUND_START_WAIT;
    unsigned gain=sound->gain.current;
    while(sound->wait_bytes)silence_output(sound,sound->wait_bytes);
    assert(sound->phase==NPE_SOUND_PLAYING&&!sound->wait_bytes&&!sound->cursor);
    assert((!already_primed||sound->gain.current==gain)&&sound->driver_configured);
    assert(sound->frame_valid&&sound->decoder.next_frame==1&&!sound->decode_failed);
    assert(npe_audio_clock_ms(&sound->clock)==20&&sound->clock.increment==655360&&
           sound->pcm_halves_pending==3u);
}
/* Tail isolation supplies the sequential decoder history explicitly.
   Production does not support arbitrary cursor seeking or track caches. */
static void prepare_tail(NpeSoundPlayer *sound,size_t source_bytes){
    size_t end=sound->byte_count[sound->active];
    uint16_t control=(uint16_t)(sound->decoder.input.flags89&0x800fu);
    npe_audio_mode5_init(&sound->decoder);sound->frame_valid=0;
    unsigned last=sound->resources[sound->active].header_count-1;
    for(unsigned i=0;i<=last;++i)
        assert(!npe_audio_mode5_decode(&sound->decoder,&sound->resources[sound->active],i,sound->frame));
    /* The offline history builder owns codec input, not registration or
       callback flags from the original driver state preceding this tail. */
    sound->decoder.input.flags89|=control;
    sound->frame_index=last;sound->frame_valid=1;sound->cursor=end-source_bytes;
    sound->output=(NpeAudioOutput){1,80,320};
    assert(npe_audio_output_half(&sound->output,&sound->gain,&sound->decoder.input.flags8a,
        sound->frame,160,0,sound->half[1],80)==1);
    sound->pcm_halves_pending=2u;
    /* Explicit diagnostic state for the already prepared tail; this is
       not an inferred native duration or a full playback clock reference. */
    sound->clock=(NpeAudioClock){UINT64_C(0x12345678abcd),UINT32_C(655360)};
}
typedef struct {unsigned section;struct {uint64_t raw,output;} frames[3];} AudioStreamReference;
#include "audio_stream_reference.h"
typedef struct {unsigned section,count;uint8_t fields[NPE_AUDIO_MODE5_FIELDS];} AudioTailReference;
#include "audio_tail_reference.h"
static void check_tail_fields(const NpeFile *file){
    for(unsigned i=0;i<sizeof(audio_tail_reference)/sizeof(*audio_tail_reference);++i){
        const AudioTailReference *r=&audio_tail_reference[i];NpeAudioResource resource;NpeAudioMode5Fields fields;
        assert(!npe_audio_resource(file,r->section,&resource)&&resource.header_count==r->count);
        assert(!npe_audio_mode5_fields(&resource,r->count-1,&fields));
        for(unsigned field=0;field<NPE_AUDIO_MODE5_FIELDS;++field){
            if(fields.values[field]!=r->fields[field])fprintf(stderr,"section%u last-frame field%u: C%u original-swap reference%u\n",r->section,field,fields.values[field],r->fields[field]);
            assert(fields.values[field]==r->fields[field]);
        }
    }
}
static uint64_t pcm_hash(const void *bytes,size_t count){
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<count;i+=2){int16_t word;memcpy(&word,(const uint8_t *)bytes+i,2);
        uint16_t bits=(uint16_t)word;
        h=(h^(bits&255))*UINT64_C(1099511628211);
        h=(h^(bits>>8))*UINT64_C(1099511628211);
    }
    return h;
}
static void check_stream(NpeSoundPlayer *sound){
    for(unsigned track=0;track<sizeof(audio_stream_reference)/sizeof(*audio_stream_reference);++track){
        const AudioStreamReference *r=&audio_stream_reference[track];
        sound->gain=(NpeAudioGain){7,7};
        assert(!npe_sound_play_section(sound,r->section,1));drain_start(sound);
        for(unsigned frame=0;frame<3;++frame){
            NpeSoundPlayer a=*sound,b=*sound;uint8_t joined[1280],split[1280];
            sound_callback(&a,joined,sizeof(joined));
            sound_callback(&b,split,1);
            assert(npe_audio_clock_ms(&b.clock)==(frame?frame*20+10:20));
            sound_callback(&b,split+1,95);
            sound_callback(&b,split+96,600);sound_callback(&b,split+696,584);
            assert(!memcmp(joined,split,sizeof(joined))&&pcm_hash(joined,sizeof(joined))==r->frames[frame].output);
            assert(pcm_hash(a.frame,sizeof(a.frame))==r->frames[frame].raw);
            assert(!memcmp(&a.decoder,&b.decoder,sizeof(a.decoder))&&!memcmp(a.frame,b.frame,sizeof(a.frame)));
            assert(a.decoder.next_frame==frame+1&&a.frame_index==frame&&a.cursor==(frame+1)*320);
            assert(a.cursor==b.cursor&&!a.sample_pending&&!b.sample_pending&&!a.decode_failed);
            assert(npe_audio_clock_ms(&a.clock)==(frame+1)*20&&a.clock.increment==UINT32_C(655360));
            assert(a.clock.value==b.clock.value&&a.clock.increment==b.clock.increment);
            *sound=a;
        }
        /* An explicit repeat builds fresh decode history, not frame3. */
        assert(!npe_sound_play_section(sound,r->section,1));drain_start(sound);
        uint8_t first[1280];sound_callback(sound,first,sizeof(first));
        assert(sound->decoder.next_frame==1&&pcm_hash(first,sizeof(first))==r->frames[0].output);
        assert(npe_audio_clock_ms(&sound->clock)==20);
    }
    /* A damaged/truncated compressed resource stops locally, with no
       substitute track, stale frame output, source or gain advancement. */
    NpeAudioResource saved=sound->resources[4];
    sound->resources[4].size=4;unsigned gain=sound->gain.current;
    assert(!npe_sound_play_section(sound,20,1));
    if(sound->phase==NPE_SOUND_CLEANUP_WAIT)silence_output(sound,sound->wait_bytes);
    uint8_t quiet[24];sound_callback(sound,quiet,sizeof(quiet));
    for(unsigned i=0;i<sizeof(quiet);++i)assert(!quiet[i]);
    assert(sound->decode_failed&&sound->finished&&!sound->loop&&sound->cursor==0);
    assert(sound->decoder.next_frame==0&&sound->gain.current==gain&&sound->active==4);
    assert(!sound->clock.value&&!sound->clock.increment);
    silence_output(sound,sound->wait_bytes);
    assert(sound->phase==NPE_SOUND_STOPPED&&!sound->driver_configured);
    assert(!sound->clock.value&&!sound->clock.increment);
    sound->resources[4]=saved;
}

typedef struct {int16_t left,right,target_left,target_right;unsigned repeat,count;
    struct {unsigned channel;int16_t old,new;} transitions[8];} DacRampNative;
#include "audio_dac_native.h"
static void check_dac_ramp(void){
    for(unsigned i=0;i<sizeof(dac_ramp_native)/sizeof(*dac_ramp_native);++i){
        const DacRampNative *n=&dac_ramp_native[i];
        NpeAudioDacRamp r={{n->left,n->right},{n->target_left,n->target_right},(uint16_t)n->repeat};
        unsigned next=0;uint32_t nops;int changed;
        for(;;){
            int16_t before[2]={r.current[0],r.current[1]};
            changed=npe_audio_dac_ramp_step(&r,&nops);assert(changed>=0);
            assert(nops==((changed&2)?n->repeat+1:0));
            if(!changed)break;
            for(unsigned ch=0;ch<2;++ch)if(changed&(1<<ch)){
                assert(next<n->count&&n->transitions[next].channel==ch);
                assert(n->transitions[next].old==before[ch]&&n->transitions[next].new==r.current[ch]);
                ++next;
            }
        }
        assert(next==n->count&&r.current[0]==n->target_left&&r.current[1]==n->target_right);
    }
    /* Full signed ranges and REP's maximum are C boundaries, not new
       dynamic original references. Signed comparison never wraps a word. */
    NpeAudioDacRamp r={{INT16_MIN,INT16_MAX},{INT16_MAX,INT16_MIN},UINT16_MAX};
    uint32_t nops=0;unsigned count=0;
    while(npe_audio_dac_ramp_step(&r,&nops)){assert(nops==65536);++count;}
    assert(count==65535&&!nops&&r.current[0]==INT16_MAX&&r.current[1]==INT16_MIN);
    assert(npe_audio_dac_ramp_step(NULL,&nops)==-1&&npe_audio_dac_ramp_step(&r,NULL)==-1);
}

int main(int argc, char **argv) {
    SDL_RWops *input;
    Sint64 length;
    uint8_t *bytes;
    NpeFile file;
    NpeWorld world = {0};
    NpeSoundPlayer sound;
    char error[160];
    size_t cursor;
    if (argc != 2) return 2;
    check_audio_clock();check_gain();check_output_halves();check_audio_headers();check_stream_plans();check_seek_plans();check_input_prime();
    check_input_refill();
    check_input_producer();
    check_dac_ramp();
    input = SDL_RWFromFile(argv[1], "rb");
    if (!input) return 2;
    length = SDL_RWsize(input);
    assert(length > 0 && (uint64_t)length <= SIZE_MAX);
    bytes = malloc((size_t)length);
    assert(bytes && SDL_RWread(input, bytes, 1, (size_t)length) == (size_t)length);
    assert(SDL_RWclose(input) == 0);
    assert(npe_parse(&file, bytes, (size_t)length, error, sizeof(error)) == 0);
    check_tail_fields(&file);
    check_input_fields(&file);
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    assert(SDL_Init(0) == 0);
    npe_sound_open(&sound, &file);
    assert(sound.device && sound.resources[0].bytes && sound.resources[4].bytes &&
           sound.resources[7].bytes&&!sound.decoder.next_frame&&!sound.frame_valid);
    for (unsigned i = 0; i < NPE_SOUND_TRACKS; ++i) {
        NpeAudioResource resource;
        assert(npe_audio_resource(&file, 16+i, &resource) == 0);
        assert(sound.resources[i].bytes==resource.bytes && sound.byte_count[i] ==
               (size_t)resource.header_count * 320u);
        const AudioHeaderNative *n=&audio_header_native[i];
        NpeAudioHeader h;assert(!npe_audio_header(resource.bytes,resource.size,&h));
        assert(!memcmp(resource.bytes,n->raw,6)&&!memcmp(&h,&resource.header,sizeof(h)));
    }
    world.music_enabled = 1;
    world.random_state = 1;
    world.game_mode = 1;
    npe_world_sound_map_loaded(&world);
    npe_sound_select(&sound, &world);
    assert(sound.active == 4 + world.map_music_slot);
    SDL_LockAudioDevice(sound.device);
    assert(sound.request_complete==sound.request_generation&&!sound.wait_bytes);
    SDL_UnlockAudioDevice(sound.device);
    /* The main thread is idle as it would be in a modal SDL menu. */
    SDL_Delay(450);
    cursor = npe_sound_cursor(&sound);
    assert(cursor > 0 && cursor < sound.byte_count[sound.active]);
    world.game_mode = 6;
    npe_sound_select(&sound, &world);
    assert(sound.active == 4 + world.map_music_slot);
    SDL_Delay(450);
    assert(npe_sound_cursor(&sound) > cursor);
    world.game_mode = 3;
    npe_world_sound_battle_start(&world);
    npe_sound_select(&sound, &world);
    assert(sound.active >= 7 && sound.active <= 12);
    SDL_LockAudioDevice(sound.device);
    assert(sound.request_complete==sound.request_generation&&!sound.wait_bytes);
    SDL_UnlockAudioDevice(sound.device);
    SDL_Delay(450);
    assert(npe_sound_cursor(&sound) > 0);
    SDL_PauseAudioDevice(sound.device, 1);
    {
        uint8_t output[12];
        uint32_t state = world.random_state;
        npe_world_music_set(&world, 0);
        assert(sound.active == -1 && sound.finished);
        sound_callback(&sound, output, sizeof(output));
        for (unsigned i = 0; i < sizeof(output); ++i) assert(output[i] == 0);
        npe_world_music_set(&world, 1);
        assert(world.random_state == state && sound.active == 4 + world.map_music_slot);
        assert(npe_sound_cursor(&sound) == 0);
        assert(sound.phase==NPE_SOUND_CLEANUP_WAIT&&sound.wait_bytes==3200);
        drain_start(&sound);
        sound_callback(&sound, output, sizeof(output));
        uint8_t expected[16];
        repeat_word(expected,npe_audio_gain_sample(sound.frame[0],sound.gain.current));
        repeat_word(expected+8,npe_audio_gain_sample(sound.frame[1],sound.gain.current));
        assert(!memcmp(output,expected,sizeof(output)));
        npe_sound_select(&sound, &world);
        assert(npe_sound_cursor(&sound)==4&&sound.sample_pending&&sound.repeat_byte==4);
        npe_world_music_set(&world, 1);
        assert(npe_sound_cursor(&sound) == 0); /* Same explicit request restarts. */
    }
    world.game_mode = 7;
    npe_world_sound_stop(&world);
    npe_sound_select(&sound, &world);
    assert(sound.active == -1 && sound.finished);
    {
        uint8_t output[24],expected[24];
        size_t end = sound.byte_count[0];
        assert(npe_sound_play_section(&sound, 16, 0) == 0);
        drain_start(&sound);
        prepare_tail(&sound,4);
        sound.gain=(NpeAudioGain){7,7};
        memset(output, 0xaa, sizeof(output));
        sound_callback(&sound, output, sizeof(output));
        repeat_word(expected,npe_audio_gain_sample(sound.frame[158],7));
        repeat_word(expected+8,npe_audio_gain_sample(sound.frame[159],7));
        memset(expected+16,0,8);assert(!memcmp(output,expected,sizeof(output)));
        assert(sound.finished && sound.cursor == end);
        memset(output, 0xaa, sizeof(output));
        sound_callback(&sound, output, sizeof(output));
        for (unsigned i = 0; i < sizeof(output); ++i) assert(output[i] == 0);
        assert(sound.phase==NPE_SOUND_END_WAIT&&sound.wait_bytes+8+sizeof(output)==
               AUDIO_EOF_NATIVE_STOP_TICKS*NPE_AUDIO_OUTPUT_RATE*sizeof(int16_t)/1000);
        silence_output(&sound,sound.wait_bytes);
        assert(sound.phase==NPE_SOUND_STOPPED&&sound.driver_configured);
        assert(sound.clock.value==UINT64_C(0x12345678abcd)&&sound.clock.increment==UINT32_C(655360));
        assert(npe_sound_play_section(&sound, 16, 0) == 0);
        assert(!sound.finished && sound.cursor == 0);
        assert(sound.phase==NPE_SOUND_CLEANUP_WAIT&&sound.wait_bytes==3200);
        for (unsigned section = 16; section < 38; ++section) {
            unsigned track = section - 16;
            assert(npe_sound_play_section(&sound, section, 1) == 0);
            drain_start(&sound);
            end = sound.byte_count[track];
            prepare_tail(&sound,4);
            sound.gain=(NpeAudioGain){7,7};
            sound_callback(&sound, output, sizeof(output));
            repeat_word(expected,npe_audio_gain_sample(sound.frame[158],7));
            repeat_word(expected+8,npe_audio_gain_sample(sound.frame[159],7));
            memset(expected+16,0,8);
            assert(!memcmp(output,expected,sizeof(output)));
            assert(!sound.finished&&sound.cursor==end&&!sound.sample_pending);
            assert(sound.phase==NPE_SOUND_END_WAIT&&sound.wait_bytes==3192);
            assert(sound.clock.value==UINT64_C(0x12345678abcd));
            size_t native_wait=sound.wait_bytes+8;
            /* Actual service chain: EOF stop50, reopen cleanup50, start200.
               No delivered source sample advances during these waits;
               reopening primes gain and the service clock before start200. */
            silence_output(&sound,3192);assert(sound.phase==NPE_SOUND_CLEANUP_WAIT&&sound.wait_bytes==3200);
            assert(sound.clock.value==UINT64_C(0x12345678abcd));
            native_wait+=sound.wait_bytes;
            silence_output(&sound,3200);assert(sound.phase==NPE_SOUND_START_WAIT&&sound.wait_bytes==12800);
            assert(npe_audio_clock_ms(&sound.clock)==20&&sound.clock.increment==655360);
            native_wait+=sound.wait_bytes;
            assert(native_wait==AUDIO_EOF_NATIVE_LOOP_TICKS*NPE_AUDIO_OUTPUT_RATE*sizeof(int16_t)/1000);
            silence_output(&sound,12799);assert(sound.cursor==0&&sound.wait_bytes==1);
            silence_output(&sound,1);assert(sound.phase==NPE_SOUND_PLAYING&&sound.cursor==0);
            assert(sound.frame_valid&&sound.decoder.next_frame==1&&sound.pcm_halves_pending==3u);
            assert(npe_audio_clock_ms(&sound.clock)==20&&sound.clock.increment==655360);
            sound_callback(&sound,output,8);repeat_word(expected,npe_audio_gain_sample(sound.frame[0],7));
            assert(!memcmp(output,expected,8)&&sound.cursor==2&&!sound.sample_pending);
            assert(npe_audio_clock_ms(&sound.clock)==20&&sound.clock.increment==UINT32_C(655360));
        }
        assert(npe_sound_play_section(&sound, 15, 1) == -1);
        sound_callback(&sound, output, sizeof(output));
        for (unsigned i = 0; i < sizeof(output); ++i) assert(output[i] == 0);
        assert(npe_sound_play_section(&sound, 38, 1) == -1);
        silence_output(&sound,sound.wait_bytes);
        assert(sound.phase==NPE_SOUND_STOPPED&&!sound.driver_configured);
        assert(npe_sound_play_section(&sound,0,0)==-1&&!sound.wait_bytes);
        // Each original 80-sample half advances gain once, independent
        // of callback partitioning, including odd-byte boundaries.
        sound.gain=(NpeAudioGain){0,7};assert(!npe_sound_play_section(&sound,20,1));
        assert(sound.phase==NPE_SOUND_START_WAIT&&sound.wait_bytes==12800);drain_start(&sound);
        NpeSoundPlayer a=sound,b=sound;uint8_t one[641],split[641];
        sound_callback(&a,one,641);sound_callback(&b,split,31);sound_callback(&b,split+31,606);
        assert(npe_audio_clock_ms(&b.clock)==20);
        assert(b.gain.current==2&&b.cursor==160&&b.sample_pending);
        sound_callback(&b,split+637,3);assert(b.gain.current==2&&b.cursor==160&&!b.sample_pending);
        assert(npe_audio_clock_ms(&b.clock)==20);
        sound_callback(&b,split+640,1);assert(b.gain.current==2&&b.cursor==162);
        assert(npe_audio_clock_ms(&b.clock)==20&&a.clock.value==b.clock.value&&a.clock.increment==b.clock.increment);
        assert(!memcmp(one,split,sizeof(one))&&a.cursor==b.cursor&&a.repeat_byte==b.repeat_byte&&a.gain.current==b.gain.current);
        /* Original gain/clipping fixtures are checked by check_gain();
           editing decoded input cannot alter a half already primed. */
        sound.gain=(NpeAudioGain){7,7};
        /* Compare a callback spanning the entire loop wait with odd splits.
           The first resumed word belongs to the new resource iteration. */
        assert(!npe_sound_play_section(&sound,20,1));drain_start(&sound);
        prepare_tail(&sound,4);a=sound;b=sound;
        uint8_t joined[19224],parts[19224];
        sound_callback(&a,joined,sizeof(joined));sound_callback(&b,parts,17);
        assert(b.phase==NPE_SOUND_END_WAIT&&b.wait_bytes==3199);
        sound_callback(&b,parts+17,3199);assert(b.phase==NPE_SOUND_CLEANUP_WAIT);
        sound_callback(&b,parts+3216,3200);assert(b.phase==NPE_SOUND_START_WAIT);
        sound_callback(&b,parts+6416,12799);assert(b.wait_bytes==1);
        sound_callback(&b,parts+19215,9);
        assert(!memcmp(joined,parts,sizeof(joined))&&a.cursor==2&&b.cursor==2);
        assert(a.phase==NPE_SOUND_PLAYING&&b.phase==a.phase);
        assert(a.gain.current==b.gain.current&&!a.sample_pending&&!b.sample_pending);
        /* A valid slot with no resource is silent, never a title fallback. */
        sound.resources[21].bytes = NULL;
        assert(npe_sound_play_section(&sound, 37, 0) == -1);
        sound_callback(&sound, output, sizeof(output));
        for (unsigned i = 0; i < sizeof(output); ++i) assert(output[i] == 0);
        assert(sound.phase==NPE_SOUND_CLEANUP_WAIT&&sound.wait_bytes==3200-sizeof(output));
        size_t stop_wait=sound.wait_bytes;
        assert(npe_sound_play_section(&sound,0,0)==-1&&sound.wait_bytes==stop_wait);
        silence_output(&sound,stop_wait);
        assert(sound.phase==NPE_SOUND_STOPPED&&!sound.driver_configured);
        assert(npe_sound_play_section(&sound,0,0)==-1&&!sound.wait_bytes);
    }
    check_stream(&sound);
    world.game_mode = 7;
    npe_world_sound_stop(&world);
    npe_sound_select(&sound, &world);
    assert(sound.active == -1 && npe_sound_cursor(&sound) == 0);
    SDL_PauseAudioDevice(sound.device, 0);
    SDL_Delay(450);
    assert(sound.active == -1 && npe_sound_cursor(&sound) == 0);
    npe_sound_select(&sound, &world);
    assert(sound.active == -1 && npe_sound_cursor(&sound) == 0);
    npe_sound_close(&sound);
    npe_free(&file);
    SDL_Quit();
    puts("SDL sound: 22 unchanged PRG clock paths/116 checkpoints, callback-partition invariant service clock, cleanup reset and finite EOF retention; 24 original range seek/window paths with codec/file boundaries, 38 original input prime paths with RAM read/returned PCM boundaries, 36 original stream plan paths with codec boundaries, 64 original headers including22 real NPE sections, 14 original half scheduler paths with codec/transfer boundaries, 12 original producer paths with RAM file boundary, 16 original input refill/EOF paths, 10486 buffered frames/22 NPE sections, original full EOF IRQ wait counts, 12 original final-word swaps and last-frame fields, 6 known candidate PCM frames streamed from NPE, fresh restart history and decode failure, original Timer2 wait order, 22 loop tails and odd splits, 8 DAC ramp traces and 10 gain calls passed");
    return 0;
}
