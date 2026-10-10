#include "../audio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    uint32_t demand,index,half,words,decoded,current,target,flags89,flags8a,
        active,pending,debt,remaining,frame_bits,tail,previous_cursor,previous_remainder;
    uint64_t value;uint32_t increment;NpeAudioCursor positions[2];
} PcmNative;
#include "audio_pcm_service_native.h"
typedef struct {
    const PcmNative *native;
    NpeAudioPcmService service;NpeAudioInput input;NpeAudioOutput output;
    NpeAudioClock clock;NpeAudioGain gain;
    unsigned query,count;
    uint16_t trace[8][32];
    int16_t pcm[176],half[88],repeated[648];
} Probe;

static void capture(Probe *p,uint16_t *v,unsigned event,NpeAudioCursor arg,unsigned aux){
    uint32_t source=0xd948u+0x1000u*p->input.source_index+2u*p->input.source_offset;
    uint16_t words[]={event,(uint16_t)(arg.address>>16),(uint16_t)arg.address,(uint16_t)aux,
        p->input.flags89,p->input.flags8a,p->service.flags8e,
        p->service.reload_words[0],p->service.reload_words[1],p->service.transfer_words,
        (uint16_t)(p->clock.value>>32),(uint16_t)(p->clock.value>>16),(uint16_t)p->clock.value,
        (uint16_t)(p->clock.increment>>16),(uint16_t)p->clock.increment,
        p->output.output_words,p->output.half_index,p->output.half_samples,
        (uint16_t)(p->service.time_snapshot>>16),(uint16_t)p->service.time_snapshot,
        (uint16_t)(p->service.cursor.address>>16),(uint16_t)p->service.cursor.address,
        p->service.cursor.remainder,(uint16_t)(p->input.tail_pointer>>16),(uint16_t)p->input.tail_pointer,
        p->input.available,p->input.pending[1],p->input.pending[0],p->input.debt,0x50,(uint16_t)source,
        (uint16_t)((unsigned)p->gain.current<<8|p->gain.target)};
    assert(sizeof(words)==64);memcpy(v,words,sizeof(words));
}
static void trace(Probe *p,unsigned event,NpeAudioCursor arg,unsigned aux){
    assert(p->count<8);capture(p,p->trace[p->count++],event,arg,aux);
}
static int prepare(void *context,unsigned half,uint32_t destination){
    Probe *p=context;unsigned previous=p->output.output_words;
    int decode=p->output.half_index>=2;
    assert(destination==(half?0x2084u:0x1784u));
    if(decode)trace(p,1,(NpeAudioCursor){0x2984,0},p->native->decoded);
    int result=npe_audio_output_half(&p->output,&p->gain,&p->input.flags8a,
        p->pcm,176,(uint16_t)p->native->decoded,p->half,88);
    assert(result>=0);
    if(p->output.output_words!=previous||(decode&&!p->native->decoded)){
        assert(!npe_audio_clock_configure(&p->clock,32000,p->output.output_words));
        p->service.transfer_words=p->output.output_words;
    }
    if(result){
        /* AD92 is called before the original2B14 increment. */
        --p->output.half_index;
        trace(p,2,(NpeAudioCursor){destination,0},p->output.output_words);
        ++p->output.half_index;
        for(unsigned i=0;i<p->output.half_samples;++i)
            for(unsigned repeat=0;repeat<4;++repeat)p->repeated[i*4+repeat]=p->half[i];
    }
    return 0;
}
static int cursor(void *context,NpeAudioCursor *position){
    Probe *p=context;assert(p->query<2);*position=p->native->positions[p->query++];
    trace(p,3,*position,position->remainder);return 0;
}
static int restore(void *context,NpeAudioCursor position){
    Probe *p=context;assert(position.address==0x8800);
    trace(p,4,position,position.remainder);return 0;
}
static const NpeAudioPcmBackend backend={prepare,cursor,restore};

static size_t reference(const char *directory,const char *name,const char *suffix,
                         uint16_t *words,size_t capacity){
    char path[1024];assert(snprintf(path,sizeof(path),"%s/%s.%s.bin",directory,name,suffix)>0);
    FILE *f=fopen(path,"rb");assert(f);size_t size=0;int low,high;
    while((low=fgetc(f))!=EOF){high=fgetc(f);assert(high!=EOF&&size<capacity);
        words[size++]=(uint16_t)(low|high<<8);}
    assert(!ferror(f));fclose(f);return size;
}
static void equal(const char *name,const char *label,const uint16_t *actual,
                   const uint16_t *expected,size_t count){
    for(size_t i=0;i<count;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"%s %s word%zu C=%04x native=%04x\n",name,label,i,actual[i],expected[i]);
        assert(actual[i]==expected[i]);
    }
}
int main(int argc,char **argv){
    assert(argc==2);unsigned traces=0;
    for(unsigned c=0;c<sizeof(pcm_native)/sizeof(*pcm_native);++c){
        const PcmNative *n=&pcm_native[c];Probe p={0};p.native=n;
        p.service=(NpeAudioPcmService){0,{n->previous_cursor,(uint16_t)n->previous_remainder},
            (uint16_t)n->demand,(uint16_t)n->words,{0x1111,0x2222}};
        p.clock=(NpeAudioClock){n->value,n->increment};
        p.output=(NpeAudioOutput){(uint16_t)n->index,(uint16_t)n->half,(uint16_t)n->words};
        p.gain=(NpeAudioGain){(uint8_t)n->current,(uint8_t)n->target};
        for(unsigned i=0;i<176;++i)p.pcm[i]=(int16_t)0x8ad0;
        for(unsigned i=0;i<512;++i)p.input.window[i]=0x1001;
        for(unsigned i=0;i<2048;++i){p.input.source[0][i]=0x2002;p.input.source[1][i]=0x3003;}
        p.input.source_index=!!(n->flags8a&0x800);p.input.available=(uint16_t)n->active;
        p.input.pending[!p.input.source_index]=(uint16_t)n->pending;
        p.input.debt=(uint16_t)n->debt;p.input.file_remaining=n->remaining;
        p.input.frame_bits=n->frame_bits;p.input.tail_pointer=n->tail;
        p.input.flags89=(uint16_t)n->flags89;p.input.flags8a=(uint16_t)n->flags8a;
        int result=npe_audio_pcm_service(&p.service,&p.input,&p.output,&p.clock,&backend,&p);
        assert(result==(int)(!!(n->demand&0x4000)+!!(n->demand&0x8000)));
        uint16_t expected[8*32],final[32];size_t count=reference(argv[1],n->name,"trace",expected,8*32);
        assert(count==p.count*32);equal(n->name,"trace",&p.trace[0][0],expected,count);traces+=p.count;
        capture(&p,final,0,(NpeAudioCursor){0,0},0);
        assert(reference(argv[1],n->name,"final",expected,8*32)==32);
        equal(n->name,"final",final,expected,32);
        uint16_t window[512],repeated[648];
        assert(reference(argv[1],n->name,"window",window,512)==512);
        equal(n->name,"window",p.input.window,window,512);
        assert(reference(argv[1],n->name,"repeated",repeated,648)==648);
        equal(n->name,"repeated",(const uint16_t *)p.repeated,repeated,648);
    }
    printf("PCM service:28 original paths/%u codec-boundary snapshots, full window, gain/repeat, clock, acknowledgements and sticky EOF passed; no original codec/physical timing proof\n",traces);
    return 0;
}
