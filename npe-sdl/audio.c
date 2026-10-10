#include "audio.h"
#include <string.h>

#define AUDIO_CLOCK_MASK UINT64_C(0xffffffffffff)
int npe_audio_clock_configure(NpeAudioClock *clock,uint16_t rate,uint16_t words){
    if(!clock||!rate)return -1;
    /* Original08:385C executes UMUL, then two UDIVL instructions.
       Preserve their word-sized outputs. On quotient overflow the current
       DMCU reference leaves R6/R7 unchanged; silicon overflow results remain
       undocumented. Ordinary output rates do not enter that case. */
    uint32_t product=(uint32_t)words*1000u;
    uint16_t r6=(uint16_t)(product>>16),r7=(uint16_t)product;
    uint32_t quotient=product/rate;
    if(quotient<=UINT16_MAX){r6=(uint16_t)(product%rate);r7=(uint16_t)quotient;}
    uint16_t integral=r7;
    r7=0;product=(uint32_t)r6<<16;quotient=product/rate;
    if(quotient<=UINT16_MAX){r6=(uint16_t)(product%rate);r7=(uint16_t)quotient;}
    /* SUB sets borrow when the remainder exceeds the halved rate;
       the following two ADDC instructions propagate it through both words. */
    clock->increment=((uint32_t)integral<<16)+r7+(r6>(rate>>1));
    return 0;
}
void npe_audio_clock_advance(NpeAudioClock *clock){
    if(clock)clock->value=(clock->value+clock->increment)&AUDIO_CLOCK_MASK;
}
void npe_audio_clock_retract(NpeAudioClock *clock){
    if(!clock)return;
    uint64_t value=(clock->value-clock->increment)&AUDIO_CLOCK_MASK;
    /* Original tests the sign of the final high-word SUBC, rather than an
       unsigned whole-counter comparison. Preserve that diagnostic edge. */
    clock->value=(value&UINT64_C(0x800000000000))?0:value;
}
void npe_audio_clock_add_ms(NpeAudioClock *clock,uint32_t milliseconds){
    if(clock)clock->value=(clock->value+((uint64_t)milliseconds<<16))&AUDIO_CLOCK_MASK;
}
uint32_t npe_audio_clock_ms(const NpeAudioClock *clock){
    return clock?(uint32_t)(clock->value>>16):0;
}

int npe_audio_stream_plan(const NpeAudioHeader *header,const NpeAudioCursor positions[7],
                          const uint16_t samples[3],uint32_t start_ms,uint32_t length_ms,
                          NpeAudioStreamPlan *plan) {
    NpeAudioStreamPlan next={0};
    uint32_t divisor,product,frame_index,remaining;
    if(!header||!positions||!samples||!plan)return -1;
    next.header_bits=(uint16_t)(((positions[0].address-UINT32_C(0x8800))<<3)+positions[0].remainder);
    for(unsigned i=0;i<3;++i){
        uint32_t bits=((positions[2+2*i].address-positions[1+2*i].address)<<3)+
            positions[2+2*i].remainder-positions[1+2*i].remainder;
        /* The original drops the high word before replacing29D0/29D2;
           this is the last observed step, not a maximum or an average. */
        next.frame_bits=(uint16_t)bits;
        next.decoded_samples=samples[i];
    }
    divisor=header->bitrate/100;
    if(!divisor||!next.frame_bits)return -1; /* Host rejects undefined division. */
    product=UINT32_C(0xa000)*next.frame_bits;
    next.frame_ms_q12=product/divisor;
    if(!next.frame_ms_q12)return -1;
    product=(next.frame_ms_q12>>4)*header->count;
    next.total_ms=product>>8;
    frame_index=(start_ms<<12)/next.frame_ms_q12;
    next.elapsed_ms=frame_index*(next.frame_ms_q12>>12);
    remaining=next.total_ms-next.elapsed_ms;
    if(!length_ms||length_ms>remaining)length_ms=remaining;
    next.start_bits=frame_index*next.frame_bits+next.header_bits;
    next.length_bits=((length_ms<<12)/next.frame_ms_q12)*next.frame_bits;
    *plan=next;
    return 0;
}

int npe_audio_seek_plan(const NpeAudioStreamPlan *stream,uint32_t remaining,
                        uint32_t resource_offset,NpeAudioSeekPlan *plan) {
    NpeAudioSeekPlan next={0};
    if(!stream||!plan)return -1;
    next.bit_remainder=(uint16_t)(stream->start_bits&15u);
    next.start_bytes=(stream->start_bits>>3)&UINT32_C(0x00fffffe);
    next.length_bytes=(stream->length_bits>>3)&UINT32_C(0x00fffffe);
    if(next.start_bytes<=(uint32_t)(stream->header_bits>>3)||next.start_bytes>remaining){
        next.remaining=next.length_bytes;
    }else{
        uint32_t end=next.start_bytes+next.length_bytes;
        next.seek_required=1;
        next.remaining=remaining<=end?remaining-next.start_bytes:next.length_bytes;
        next.seek_offset=resource_offset+next.start_bytes;
        next.read_request=(uint16_t)(next.remaining>1024?1024:next.remaining);
    }
    *plan=next;
    return 0;
}

static void input_normalize(uint16_t *words,size_t count,uint8_t xor_key) {
    for(size_t word=0;word<count/2;++word){
        uint16_t value=words[word]^(uint16_t)(xor_key*0x101u);
        words[word]=(uint16_t)((value<<8)|(value>>8));
    }
    if(count&1)words[count/2]^=(uint16_t)((unsigned)xor_key<<8);
}

int npe_audio_seek_window(const NpeAudioSeekPlan *plan,uint16_t *window,
                          NpeAudioInputReader reader,void *context,uint8_t xor_key) {
    size_t actual;
    if(!plan)return -1;
    if(!plan->seek_required)return 0;
    if(!window||!reader||plan->read_request>NPE_AUDIO_INPUT_WORDS*2)return -1;
    actual=reader(context,0,window,plan->read_request);
    if(actual>plan->read_request)return -1;
    if(!actual)return -2;
    input_normalize(window,actual,xor_key);
    return 1;
}

static int input_copy(NpeAudioInput *input,unsigned destination,unsigned count) {
    if(input->source_index>1||input->source_offset>NPE_AUDIO_SOURCE_WORDS||
       count>NPE_AUDIO_SOURCE_WORDS-input->source_offset||
       destination>NPE_AUDIO_INPUT_WORDS||count>NPE_AUDIO_INPUT_WORDS-destination)return -1;
    memcpy(input->window+destination,input->source[input->source_index]+input->source_offset,
           count*sizeof(uint16_t));
    input->source_offset+=count;
    return 0;
}

int npe_audio_input_refill(NpeAudioInput *input,unsigned consumed_words) {
    unsigned request,destination,count;
    if(!input||input->source_index>1||input->source_offset>NPE_AUDIO_SOURCE_WORDS||
       input->available>NPE_AUDIO_SOURCE_WORDS-input->source_offset||
       input->pending[0]>NPE_AUDIO_SOURCE_WORDS||input->pending[1]>NPE_AUDIO_SOURCE_WORDS||
       consumed_words>NPE_AUDIO_INPUT_WORDS||
       input->debt>NPE_AUDIO_INPUT_WORDS-consumed_words)return -1;
    request=consumed_words+input->debt;
    /* 0D72 compacts the unread words first; prior unmet demand is removed
       from the append position, not copied again as input. */
    memmove(input->window,input->window+consumed_words,
            (NPE_AUDIO_INPUT_WORDS-consumed_words)*sizeof(uint16_t));
    destination=NPE_AUDIO_INPUT_WORDS-request;
    if(input->flags8a&3)input->flags89|=0x40;
    input->debt=0;
    if(!request)return 0;
    if(request<input->available){
        if(input_copy(input,destination,request))return -1;
        input->available-=(uint16_t)request;
        return 0;
    }
    count=input->available;
    if(input_copy(input,destination,count))return -1;
    destination+=count;request-=count;input->available=0;
    /* 31:0276 marks the old source for its producer. If both producers
       are busy it retains the old source pointer and does not switch. */
    if(input->flags8a&0x800){
        input->flags8a|=2;input->flags89|=0x40;
        if(!(input->flags8a&1)){
            input->source_index=0;input->source_offset=0;
            input->available=input->pending[0];input->pending[0]=0;
            input->flags8a&=(uint16_t)~0x800;
        }else{
            input->flags8a|=0x40;
            if(input->flags89&0x4000)input->flags89&=(uint16_t)~0x10;
        }
    }else{
        input->flags8a|=1;input->flags89|=0x40;
        if(!(input->flags8a&2)){
            input->source_index=1;input->source_offset=0;
            input->available=input->pending[1];input->pending[1]=0;
            input->flags8a|=0x800;
        }else{
            input->flags8a|=0x40;
            if(input->flags89&0x4000)input->flags89&=(uint16_t)~0x10;
        }
    }
    if(request<input->available){
        if(input_copy(input,destination,request))return -1;
        input->available-=(uint16_t)request;
    }else{
        /* Original end pointer is set BEFORE copying the second source.
           Keep the full32-bit subtract and frame_bits>>4 word lookahead. */
        input->tail_pointer=UINT32_C(0x8800)+destination*2u-(input->frame_bits>>4)*2u;
        count=input->available;
        if(input_copy(input,destination,count))return -1;
        input->debt=(uint16_t)(request-count);input->available=0;
        if(!input->file_remaining)input->flags89&=(uint16_t)~0x20;
    }
    return 0;
}

void npe_audio_input_check_end(NpeAudioInput *input,uint16_t output_words,
                              uint32_t codec_pointer) {
    if(input&&(!output_words||codec_pointer>input->tail_pointer))input->flags89|=0x800;
}

int npe_audio_service_begin(uint16_t *flags89,uint16_t flags8a,
                           uint16_t flags8d,uint16_t *flags8e,unsigned service) {
    if(!flags89||service>NPE_AUDIO_SERVICE_PRODUCER||
       (service==NPE_AUDIO_SERVICE_PCM&&!flags8e))return -1;
    uint16_t busy=(uint16_t)(1u<<service);
    if(service==NPE_AUDIO_SERVICE_PCM){
        if(*flags89&busy)return NPE_AUDIO_SERVICE_NEXT;
        *flags89|=busy;*flags8e&=(uint16_t)~2u;
    }else if(service==NPE_AUDIO_SERVICE_AUXILIARY){
        if((*flags89&0x2020u)!=0x2020u)return NPE_AUDIO_SERVICE_NEXT;
        if(*flags89&busy)return NPE_AUDIO_SERVICE_YIELD;
        *flags89|=busy;*flags89&=(uint16_t)~0x20u;
    }else{
        /*3EC2 prioritizes starvation with an active producer over10. */
        if((flags8a&0x40u)&&(*flags89&busy))return NPE_AUDIO_SERVICE_YIELD;
        if(*flags89&0x400u)return NPE_AUDIO_SERVICE_RESUME;
        if((*flags89&0x4040u)!=0x4040u||(flags8d&5u)||(*flags89&busy))
            return NPE_AUDIO_SERVICE_NEXT;
        *flags89|=busy;*flags89&=(uint16_t)~0x40u;
    }
    return NPE_AUDIO_SERVICE_READY;
}
void npe_audio_service_end(uint16_t *flags89,unsigned service) {
    if(flags89&&service<=NPE_AUDIO_SERVICE_PRODUCER)
        *flags89&=(uint16_t)~(1u<<service);
}

int npe_audio_transport_suspend(uint16_t *flags89,uint16_t flags88,
                               const NpeAudioTransportBackend *backend,void *context) {
    if(!flags89||!backend||!backend->pause||((flags88&0x10u)&&!backend->adjust))return -1;
    if((flags88&0x10u)&&backend->adjust(context))return -1;
    if(backend->pause(context))return -1;
    *flags89|=0x400u;
    return 0;
}
int npe_audio_transport_resume(uint16_t *flags89,const uint16_t *flags8a,
    const uint16_t *flags8e,const uint16_t controls[2],
    const NpeAudioTransportBackend *backend,void *context) {
    if(!flags89||!flags8a||!flags8e||!controls||!backend||!backend->refill||!backend->resume)return -1;
    if(!(*flags89&0x800u)&&(*flags89&3u))return NPE_AUDIO_TRANSPORT_BLOCKED;
    *flags89&=(uint16_t)~0x400u;
    if(controls[0]==0x8000u&&controls[1]==0x8000u&&
       (*flags89&0x1010u)==0x1010u&&!(*flags8e&2u)){
        if(backend->refill(context)||backend->resume(context))return -1;
        return (*flags8e&2u)?NPE_AUDIO_TRANSPORT_PCM:NPE_AUDIO_TRANSPORT_RETURN;
    }
    *flags89|=0x20u;
    if(backend->resume(context))return -1;
    return (*flags8a&0x40u)?NPE_AUDIO_TRANSPORT_RETURN:NPE_AUDIO_TRANSPORT_AUXILIARY;
}
int npe_audio_producer_poll_begin(uint16_t *flags89,uint16_t flags8d) {
    if(!flags89)return -1;
    if((*flags89&0x4040u)!=0x4040u||(*flags89&4u)||(flags8d&5u))return 0;
    *flags89|=4u;*flags89&=(uint16_t)~0x40u;
    return 1;
}

int npe_audio_eof_gate(uint16_t *flags89,uint16_t flags8d,uint16_t stack_word) {
    if(!flags89)return -1;
    if(stack_word<0x200u||(*flags89&8u)||!(*flags89&0x800u))return NPE_AUDIO_EOF_IGNORED;
    *flags89&=(uint16_t)~0x7000u;
    if((*flags89&7u)||(flags8d&5u))return NPE_AUDIO_EOF_BUSY;
    return NPE_AUDIO_EOF_READY;
}
int npe_audio_eof_callback_begin(uint16_t *flags89) {
    if(!flags89)return -1;
    if(!(*flags89&0x8000u))return 0;
    *flags89|=8u;
    return 1;
}
void npe_audio_eof_callback_end(uint16_t *flags89) {
    if(flags89)*flags89&=(uint16_t)~8u;
}
void npe_audio_cleanup_flags(uint16_t *flags89,uint16_t *flags8a) {
    if(flags89)*flags89&=8u;
    if(flags8a)*flags8a=(uint16_t)((*flags8a&0x80u)|3u);
}

int npe_audio_pcm_service(NpeAudioPcmService *service,NpeAudioInput *input,
                           NpeAudioOutput *output,NpeAudioClock *clock,
                           const NpeAudioPcmBackend *backend,void *context) {
    int count=0;
    if(!service||!input||!output||!clock||!backend||!backend->prepare||
       !backend->cursor||!backend->restore)return -1;
    service->time_snapshot=npe_audio_clock_ms(clock);
    for(unsigned half=0;half<2;++half){
        uint16_t demand=(uint16_t)(0x4000u<<half);
        if(!(service->flags8e&demand))continue;
        if(backend->prepare(context,half,half?0x2084u:0x1784u))return -1;
        /* Original acknowledgements run even after a zero codec return. */
        service->reload_words[half]=service->transfer_words;
        service->flags8e&=(uint16_t)~demand;
        NpeAudioCursor position;
        if(backend->cursor(context,&position))return -1;
        service->cursor=position;
        npe_audio_clock_advance(clock);
        if(input->flags89&0x20){
            uint32_t address=service->cursor.address;
            if(address<0x8800u||address>0x8c00u||(address&1u))return -1;
            if(npe_audio_input_refill(input,(address-0x8800u)/2u)||
               backend->restore(context,(NpeAudioCursor){0x8800u,service->cursor.remainder}))return -1;
        }
        ++count;
    }
    npe_audio_input_check_end(input,output->output_words,service->cursor.address);
    return count;
}

int npe_audio_input_produce(NpeAudioInput *input,NpeAudioInputReader reader,
                            void *context,uint8_t xor_key) {
    if(!input||!reader)return -1;
    for(unsigned source=0;source<2;++source){
        size_t requested,actual;
        if(!(input->flags8a&(1u<<source)))continue;
        requested=input->file_remaining;
        if(requested>NPE_AUDIO_SOURCE_WORDS*2)requested=NPE_AUDIO_SOURCE_WORDS*2;
        else input->flags89&=(uint16_t)~0x4000;
        actual=reader(context,source,input->source[source],requested);
        if(actual>requested)return -1;
        /* Original zero return retains the old pending count and request
           flags. It still clears transmission14 on the last-size branch. */
        if(!actual)continue;
        input_normalize(input->source[source],actual,xor_key);
        input->file_remaining-=(uint32_t)actual;
        input->pending[source]=(uint16_t)(actual/2);
        input->flags8a&=(uint16_t)~((1u<<source)|0x40u);
    }
    input->flags89|=0x10;
    return 0;
}

int npe_audio_input_prime(NpeAudioInput *input,NpeAudioInputReader reader,
                          void *context,uint8_t xor_key) {
    if(!input||input->pending[0]>NPE_AUDIO_SOURCE_WORDS||
       input->pending[1]>NPE_AUDIO_SOURCE_WORDS||
       (input->file_remaining>NPE_AUDIO_INPUT_WORDS*2&&!reader))return -1;
    if(input->file_remaining>NPE_AUDIO_INPUT_WORDS*2){
        input->flags89|=0x20;
        input->file_remaining-=NPE_AUDIO_INPUT_WORDS*2;
        if(npe_audio_input_produce(input,reader,context,xor_key))return -1;
    }else{
        /* The short-window branch never calls the file producer. It also
           preserves an existing89.5, rather than unconditionally clearing. */
        input->flags89&=(uint16_t)~0x4000;
        input->tail_pointer=UINT32_C(0x8800)+input->file_remaining;
        input->file_remaining=0;
    }
    /* Original SL2 wraps at16 bits before SR1; not simply sample_count*2. */
    input->initial_output_words=(uint16_t)((uint16_t)(input->plan.decoded_samples<<2)>>1);
    input->available=input->pending[0];input->pending[0]=0;
    return 0;
}

int npe_audio_dac_ramp_step(NpeAudioDacRamp *ramp,uint32_t *delay_nops) {
    int changed=0;
    if(!ramp||!delay_nops)return -1;
    *delay_nops=0;
    for(unsigned channel=0;channel<2;++channel){
        if(ramp->current[channel]==ramp->target[channel])continue;
        if(ramp->current[channel]<ramp->target[channel])++ramp->current[channel];
        else --ramp->current[channel];
        changed|=1<<channel;
    }
    if(changed&2)*delay_nops=(uint32_t)ramp->repeat_argument+1;
    return changed;
}

static const uint16_t output_gain_coefficients[8]={0,0x2027,0x287a,0x32f5,0x4027,0x50c3,0x65ad,0x7fff};
int16_t npe_audio_gain_sample(int16_t sample,unsigned level) {
    int64_t accumulator=0;
    if(level>7)return 0;
    /* Fractional multiplication, rounding and signed saturation happen
       after EACH instruction, including the first MULS. */
    for(unsigned i=0;i<2;++i){
        int64_t rounded=accumulator+(int64_t)sample*output_gain_coefficients[level]*2+32768;
        int64_t word=rounded>=0?rounded/65536:-((-rounded+65535)/65536);
        accumulator=word*65536;
        if(accumulator>INT32_MAX)accumulator=INT32_MAX;
        if(accumulator<INT32_MIN)accumulator=INT32_MIN;
    }
    int64_t word=accumulator>=0?accumulator/65536:-((-accumulator+65535)/65536);
    return (int16_t)word;
}
int npe_audio_gain_block(NpeAudioGain *gain,int16_t *samples,size_t count) {
    if(!gain||gain->current>7||gain->target>7||(!samples&&count))return -1;
    if(gain->current<gain->target)++gain->current;
    else if(gain->current>gain->target)--gain->current;
    for(size_t i=0;i<count;++i)samples[i]=npe_audio_gain_sample(samples[i],gain->current);
    return 0;
}

int npe_audio_output_half(NpeAudioOutput *output,NpeAudioGain *gain,
                          uint16_t *flags8a,const int16_t *pcm,size_t capacity,
                          uint16_t decoded_samples,int16_t *half,size_t half_capacity) {
    NpeAudioOutput next;
    size_t offset;
    if(!output||!gain||!flags8a||gain->current>7||gain->target>7)return -1;
    next=*output;
    if(next.half_index>=2){
        next.half_index=0;
        next.half_samples=decoded_samples;
        if(!decoded_samples){
            next.output_words=0;*output=next;
            return 0; /* No gain, repeat or transfer on the codec-zero branch. */
        }
        next.half_samples>>=1;
        next.output_words=(uint16_t)(next.half_samples<<2);
    }
    offset=(size_t)next.half_index*next.half_samples;
    if(offset>capacity||next.half_samples>capacity-offset||
       next.half_samples>half_capacity||(!pcm&&next.half_samples)||
       (!half&&next.half_samples))return -1;
    if(gain->current||gain->target)*flags8a&=(uint16_t)~0x80;
    else *flags8a|=0x80;
    /* Gain runs once even when an existing half has no samples. */
    if(next.half_samples)memcpy(half,pcm+offset,next.half_samples*sizeof(*half));
    (void)npe_audio_gain_block(gain,half,next.half_samples);
    ++next.half_index;
    *output=next;
    return 1;
}

/* Widths measured from the isolated candidate's bit reader. MSB first. */
static const uint8_t mode5_widths[NPE_AUDIO_MODE5_FIELDS] = {
    8, 8, 8, 8, 4, 5, 4, 5, 5, 4, 3, 5, 4, 3, 5, 4, 3, 5, 4,
    3, 5, 4, 5, 4, 4, 5, 4, 3, 5, 4, 3, 5, 4, 3, 5, 4, 3
};

static int mode5_resource_valid(const NpeAudioResource *resource) {
    size_t payload,expected;
    if(!resource||!resource->bytes||resource->size<4||
       resource->bytes[0]!=0x20||resource->bytes[1]!=0x0d||
       resource->header_count!=((unsigned)resource->bytes[2]<<8|resource->bytes[3]))return 0;
    payload=(size_t)resource->header_count*NPE_AUDIO_MODE5_FRAME_BYTES;
    expected=4+payload;expected+=expected&1u;
    return resource->size==expected;
}

static unsigned input_load(NpeAudioInput *input,uint16_t *destination,unsigned capacity) {
    size_t left=input->resource.size-input->file_offset;
    unsigned words=left/2<capacity?(unsigned)(left/2):capacity;
    for(unsigned i=0;i<words;++i){
        const uint8_t *p=input->resource.bytes+input->file_offset+i*2;
        destination[i]=(uint16_t)((unsigned)p[0]<<8|p[1]);
    }
    input->file_offset+=words*2;
    return words;
}

static size_t input_read_npe(void *context,unsigned source,uint16_t *destination,size_t requested) {
    NpeAudioInput *input=context;
    size_t left=input->resource.size-input->file_offset;
    size_t count=left<requested?left:requested;
    (void)source;
    for(size_t word=0;word<count/2;++word){
        const uint8_t *p=input->resource.bytes+input->file_offset+word*2;
        /* Raw file bytes occupy the little-endian physical word lanes.
           The producer's swap creates the normalized numeric word. */
        destination[word]=(uint16_t)((unsigned)p[1]<<8|p[0]);
    }
    if(count&1)destination[count/2]=(uint16_t)((destination[count/2]&255u)|
        (unsigned)input->resource.bytes[input->file_offset+count-1]<<8);
    input->file_offset+=count;
    return count;
}

int npe_audio_input_open(NpeAudioInput *input,const NpeAudioResource *resource) {
    NpeAudioHeader header;
    NpeAudioCursor positions[7];
    const uint16_t samples[3]={NPE_AUDIO_MODE5_PCM_SAMPLES,
        NPE_AUDIO_MODE5_PCM_SAMPLES,NPE_AUDIO_MODE5_PCM_SAMPLES};
    if(!input||!mode5_resource_valid(resource)||!resource->header_count)return -1;
    if(npe_audio_header(resource->bytes,resource->size,&header))return -1;
    memset(input,0,sizeof(*input));input->resource=*resource;
    /* The candidate consumes the4-byte format header before its field
       reader. Supply its known layout to the original planning arithmetic;
       these are candidate positions, not observed original B816 outputs. */
    positions[0]=(NpeAudioCursor){0x8804,0};
    for(unsigned i=0;i<3;++i){
        unsigned before=32+i*NPE_AUDIO_MODE5_FRAME_BYTES*8;
        unsigned after=before+NPE_AUDIO_MODE5_FRAME_BYTES*8;
        positions[1+i*2]=(NpeAudioCursor){0x8800+(before/16)*2,(uint16_t)(before%16)};
        positions[2+i*2]=(NpeAudioCursor){0x8800+(after/16)*2,(uint16_t)(after%16)};
    }
    if(npe_audio_stream_plan(&header,positions,samples,0,0,&input->plan))return -1;
    /* Preserve the original range decision separately from the candidate's
       payload extent: missing codec head/restore semantics still matter. */
    if(npe_audio_seek_plan(&input->plan,(uint32_t)resource->size,0,&input->seek))return -1;
    input->file_offset=input->plan.start_bits/8;input->frame_bits=input->plan.frame_bits;
    (void)input_load(input,input->window,NPE_AUDIO_INPUT_WORDS);
    /* Supply the candidate's full payload extent before accounting for
       the initial window. Original range/codec head accounting is separate. */
    input->file_remaining=(uint32_t)(resource->size-input->plan.start_bits/8);
    input->tail_pointer=0x8c00;
    input->flags89|=0x4000;input->flags8a|=3;
    if(npe_audio_input_prime(input,input_read_npe,input,0))return -1;
    input->initialized=1;
    return 0;
}

int npe_audio_input_fields_deferred(NpeAudioInput *input,NpeAudioMode5Fields *fields) {
    if(!input||!fields||!input->initialized||
       input->frames_read>=input->resource.header_count)return -1;
    for(unsigned field=0;field<NPE_AUDIO_MODE5_FIELDS;++field){
        unsigned value=0;
        for(unsigned bit=0;bit<mode5_widths[field];++bit){
            if(!input->bits_left){
                uint16_t word;
                if(input->word_cursor>=NPE_AUDIO_INPUT_WORDS)return -1;
                word=input->window[input->word_cursor++];
                /* Normalized memory bytes are low lane then high lane;
                   candidate fields consume each byte MSB first. */
                input->bit_word=(uint16_t)((word<<8)|(word>>8));input->bits_left=16;
            }
            value=(value<<1)|((input->bit_word>>(--input->bits_left))&1);
        }
        fields->values[field]=(uint8_t)value;
    }
    ++input->frames_read;
    return 0;
}

int npe_audio_input_candidate_cursor(NpeAudioInput *input,NpeAudioCursor *cursor) {
    if(!input||!cursor||!input->initialized||input->word_cursor>NPE_AUDIO_INPUT_WORDS||
       input->bits_left>16)return -1;
    /* This reader has already cached a partial word. Its word pointer is
       after that word; its remainder describes the separately retained bits.
       This is a candidate contract, not the original B816 convention. */
    *cursor=(NpeAudioCursor){0x8800u+input->word_cursor*2u,
        (uint16_t)(input->bits_left?16u-input->bits_left:0)};
    return 0;
}

int npe_audio_input_candidate_restore(NpeAudioInput *input,NpeAudioCursor cursor) {
    if(!input||!input->initialized||cursor.address!=0x8800u||input->bits_left>16||
       cursor.remainder!=(input->bits_left?16u-input->bits_left:0))return -1;
    input->word_cursor=0;
    return 0;
}

int npe_audio_input_candidate_produce(NpeAudioInput *input) {
    if(!input||!input->initialized)return -1;
    return npe_audio_input_produce(input,input_read_npe,input,0);
}

int npe_audio_input_fields(NpeAudioInput *input,NpeAudioMode5Fields *fields) {
    if(npe_audio_input_fields_deferred(input,fields))return -1;
    if(input->flags89&0x20){
        NpeAudioCursor cursor;
        if(npe_audio_input_candidate_cursor(input,&cursor)||
           npe_audio_input_refill(input,input->word_cursor)||
           npe_audio_input_candidate_restore(input,(NpeAudioCursor){0x8800u,cursor.remainder})||
           npe_audio_input_candidate_produce(input))return -1;
    }
    return 0;
}

int npe_audio_mode5_fields(const NpeAudioResource *resource,
                           unsigned frame_index, NpeAudioMode5Fields *fields) {
    size_t frame_start, bit_position = 0;
    unsigned i, bit;
    if (!fields || !mode5_resource_valid(resource) || frame_index >= resource->header_count)
        return -1;
    frame_start = (size_t)frame_index * NPE_AUDIO_MODE5_FRAME_BYTES;
    for (i = 0; i < NPE_AUDIO_MODE5_FIELDS; ++i) {
        unsigned value = 0;
        for (bit = 0; bit < mode5_widths[i]; ++bit) {
            size_t position = frame_start + bit_position / 8;
            /* Original2C:0E1E swaps every word in the even-sized read.
               For odd frame counts, the last encoded byte is paired with
               the retained resource byte beyond21*count; do not trim it. */
            size_t swapped = position ^ 1u;
            value = (value << 1) |
                    ((resource->bytes[4 + swapped] >>
                      (7 - bit_position % 8)) & 1u);
            ++bit_position;
        }
        fields->values[i] = (uint8_t)value;
    }
    return bit_position == NPE_AUDIO_MODE5_FRAME_BYTES * 8 ? 0 : -1;
}
