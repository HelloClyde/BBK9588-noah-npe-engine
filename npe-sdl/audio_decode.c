#include "audio.h"
#include <string.h>

#include "audio_tables.inc"

static int32_t word16(int64_t value) {
    uint16_t bits = (uint16_t)value;
    return bits < 0x8000u ? bits : (int32_t)bits - 65536;
}

static int64_t word32(int64_t value) {
    uint32_t bits = (uint32_t)value;
    return bits < UINT32_C(0x80000000) ? bits : (int64_t)bits - INT64_C(4294967296);
}

static int32_t shift32(int64_t value, unsigned bits) {
    int64_t signed_value = word32(value);
    int64_t divisor = INT64_C(1) << bits;
    if (signed_value >= 0) return (int32_t)(signed_value / divisor);
    return (int32_t)(-((-signed_value + divisor - 1) / divisor));
}

static int64_t shift64(int64_t value, unsigned bits) {
    int64_t divisor = INT64_C(1) << bits;
    if (value >= 0) return value / divisor;
    return -((-value + divisor - 1) / divisor);
}

static const uint16_t field_offsets[NPE_AUDIO_MODE5_FIELDS] = {
    0x462, 0x464, 0x466, 0x468, 0x46c, 0x470, 0x498, 0x4c0,
    0x472, 0x49a, 0x4c2, 0x474, 0x49c, 0x4c4, 0x476, 0x49e,
    0x4c6, 0x478, 0x4a0, 0x4c8, 0x46a, 0x46e, 0x484, 0x4ac,
    0x4d4, 0x486, 0x4ae, 0x4d6, 0x488, 0x4b0, 0x4d8, 0x48a,
    0x4b2, 0x4da, 0x48c, 0x4b4, 0x4dc
};

static int field_value(const NpeAudioMode5Fields *fields, unsigned offset) {
    unsigned i;
    for (i = 0; i < NPE_AUDIO_MODE5_FIELDS; ++i)
        if (field_offsets[i] == offset) return fields->values[i];
    return -1;
}

static void lsf_vector(const NpeAudioMode5Fields *fields,
                       NpeAudioMode5Decoder *decoder, int32_t output[10]) {
    int32_t vector[10], values[10], lower = 204;
    unsigned i;
    memcpy(vector, (int32_t[10]){
        codebook0[fields->values[0] * 3],
        codebook0[fields->values[0] * 3 + 1],
        codebook0[fields->values[0] * 3 + 2],
        codebook1[fields->values[1] * 3],
        codebook1[fields->values[1] * 3 + 1],
        codebook1[fields->values[1] * 3 + 2],
        codebook2[fields->values[2] * 4],
        codebook2[fields->values[2] * 4 + 1],
        codebook2[fields->values[2] * 4 + 2],
        codebook2[fields->values[2] * 4 + 3]
    }, sizeof(vector));
    for (i = 0; i < 10; ++i) {
        int32_t prediction = shift32((int64_t)decoder->lsf_predictor[i] *
                                     lsf_predictor_gain[i], 15);
        int32_t current = word16(vector[i] + prediction);
        decoder->lsf_predictor[i] = (int16_t)current;
        values[i] = word16(current + lsf_bias[i]);
    }
    for (i = 9; i > 0; --i)
        if (values[i] < values[i - 1]) {
            int32_t tmp = values[i];
            values[i] = values[i - 1];
            values[i - 1] = tmp;
        }
    for (i = 0; i < 10; ++i) {
        if (values[i] < word16(lower)) values[i] = word16(lower);
        lower = (values[i] + (int32_t)i + 204) & 0xffff;
        output[i] = values[i];
    }
}

static void lpc_advance(int64_t workspace[47], const int32_t cosine[10]) {
    unsigned i;
    for (i = 0; i < 5; ++i) {
        unsigned offset = i * 4;
        int64_t even_state = workspace[(0x48 + offset) / 4];
        int64_t even_lag = workspace[(0x8c + offset) / 4];
        int64_t even_head = workspace[(0x30 + offset) / 4];
        int64_t odd_state = workspace[(0x60 + offset) / 4];
        int64_t odd_lag = workspace[(0xa4 + offset) / 4];
        int64_t odd_head = workspace[(0x18 + offset) / 4];
        workspace[(0x8c + offset) / 4] = even_state;
        workspace[(0x34 + offset) / 4] = word32(
            even_head + even_lag - word32(shift64(
                word32(even_state) * cosine[2 * i], 14)));
        workspace[(0xa4 + offset) / 4] = odd_state;
        workspace[(0x1c + offset) / 4] = word32(
            odd_head + odd_lag - word32(shift64(
                word32(odd_state) * cosine[2 * i + 1], 14)));
        workspace[(0x48 + offset) / 4] = even_head;
        workspace[(0x60 + offset) / 4] = odd_head;
    }
}

static int lpc_coefficients(const int32_t lsf[10], int16_t output[11]) {
    int32_t cosine[10];
    int64_t workspace[47] = {0};
    unsigned i;
    for (i = 0; i < 10; ++i) {
        int32_t position = word16(lsf[i]);
        int32_t index = position / 256;
        int32_t fraction;
        if (position < 0 && position % 256) --index;
        if (index < 0 || index > 63) return -1;
        fraction = position - index * 256;
        cosine[i] = word16(lsf_cosine_table[index] +
                           shift64(fraction * (lsf_cosine_table[index + 1] -
                                               lsf_cosine_table[index]) + 127,
                                   8));
    }
    workspace[0x18 / 4] = workspace[0x30 / 4] = 0x400000;
    lpc_advance(workspace, cosine);
    workspace[0x18 / 4] = -0x400000;
    workspace[0x30 / 4] = 0x400000;
    lpc_advance(workspace, cosine);
    output[0] = 0x1000;
    output[1] = (int16_t)word16(shift32(
        workspace[0x2c / 4] + workspace[0x44 / 4], 11));
    for (i = 2; i < 11; ++i) {
        workspace[0x18 / 4] = workspace[0x30 / 4] = 0;
        lpc_advance(workspace, cosine);
        output[i] = (int16_t)word16(shift32(
            workspace[0x2c / 4] + workspace[0x44 / 4], 11));
    }
    return 0;
}

static void decode_pitch(int previous, int code, int *delay, int *fraction) {
    int quotient, remainder;
    if (!previous) {
        if (code >= 192) { *delay = code - 112; *fraction = 0; return; }
        quotient = code / 3; remainder = code % 3;
        *delay = quotient + (remainder == 2 ? 17 : 16);
        *fraction = remainder == 2 ? -1 : remainder;
    } else if (code >= 16) {
        quotient = (code - 16) / 3; remainder = (code - 16) % 3;
        *delay = previous + quotient + (remainder == 2);
        *fraction = remainder == 2 ? -1 : remainder;
    } else {
        quotient = (16 - code) / 3; remainder = (16 - code) % 3;
        *delay = previous - quotient - (remainder == 2);
        *fraction = remainder == 2 ? 1 : -remainder;
    }
}

static int fixed_pulses(const NpeAudioMode5Fields *fields, unsigned subframe,
                        int *anchor, int16_t output[80]) {
    int previous = *anchor;
    unsigned group, pulse;
    memset(output, 0, 80 * sizeof(*output));
    for (group = 0; group < 5; ++group) {
        int code = field_value(fields, 0x4c0 + subframe * 20 + group * 2);
        int position = field_value(fields, 0x470 + subframe * 20 + group * 2);
        int signs = field_value(fields, 0x498 + subframe * 20 + group * 2);
        int index;
        if (code < 0 || position < 0 || signs < 0) return -1;
        if (previous < 0) index = code;
        else if (!group) {
            int adjustment = previous - 7;
            if (adjustment < 0) adjustment = 0;
            if (adjustment > 16) adjustment = 16;
            index = code + adjustment;
        } else {
            int adjustment = previous - 7;
            index = code + (adjustment > 0 ? adjustment : 0);
        }
        if (index >= 32 || position >= 32) return -1;
        if (!group) *anchor = index;
        previous = index;
        for (pulse = 0; pulse < 4; ++pulse) {
            int pos = pulse_positions[(position + (group & 1u) * 32) * 4 + pulse];
            int amplitude = pulse_amplitudes[index];
            if (signs & (1 << (3 - pulse))) amplitude = -amplitude;
            output[pos] = (int16_t)word16(output[pos] + amplitude);
        }
    }
    return 0;
}

static void adaptive_pulses(const int16_t available[240], unsigned available_size,
                            int delay, int fraction, int16_t output[80]) {
    int16_t samples[240 + 80];
    unsigned i, j;
    memcpy(samples, available, available_size * sizeof(*samples));
    for (i = 0; i < 80; ++i) {
        int center = (int)available_size + (int)i - delay;
        int32_t value;
        if (fraction) {
            int64_t accumulator = 0;
            for (j = 0; j < 29; ++j) {
                int tap = interpolation_taps[47 + 3 * ((int)j - 14) + fraction];
                accumulator += samples[center - 14 + (int)j] * tap;
            }
            value = word16(shift32(accumulator + 0x3fff, 15));
        } else value = samples[center];
        samples[available_size + i] = (int16_t)value;
        output[i] = (int16_t)value;
    }
}

static void mix_pulses(const int16_t adaptive[80], const int16_t fixed[80],
                       int gain, int delay, int16_t output[80]) {
    int16_t sharpened[80];
    unsigned i;
    memcpy(sharpened, fixed, sizeof(sharpened));
    if (delay < 80) for (i = 0; i < (unsigned)(80 - delay); ++i) {
        int32_t feedback = word16(shift32(
            (int64_t)sharpened[i] * gain + 0x1fff, 14));
        sharpened[i + delay] = (int16_t)word16(sharpened[i + delay] + feedback);
    }
    for (i = 0; i < 80; ++i) {
        int32_t scaled = word16(shift32(
            (int64_t)adaptive[i] * gain + 0x1fff, 14));
        output[i] = (int16_t)word16(scaled + sharpened[i]);
    }
}

static void synthesize(const int16_t mixed[80], const int16_t coefficients[11],
                       int16_t history[10], int16_t output[80]) {
    unsigned i, tap;
    for (i = 0; i < 80; ++i) {
        int64_t accumulator = word32((int64_t)mixed[i] * 8192);
        int32_t value;
        for (tap = 0; tap < 10; ++tap)
            accumulator = word32(accumulator -
                                 (int64_t)history[9 - tap] * coefficients[tap + 1]);
        accumulator = word32(shift32(accumulator, 1) + 0x3ff);
        value = shift32(accumulator, 11);
        if (value > 32767) value = 32767;
        else if (value < -32768) value = -32767;
        output[i] = (int16_t)value;
        memmove(history, history + 1, 9 * sizeof(*history));
        history[9] = (int16_t)value;
    }
}

void npe_audio_mode5_init(NpeAudioMode5Decoder *decoder) {
    if (!decoder) return;
    memset(decoder, 0, sizeof(*decoder));
    memcpy(decoder->previous_lsf, lsf_bias, sizeof(decoder->previous_lsf));
}

static int mode5_decode(NpeAudioMode5Decoder *decoder,
                           const NpeAudioResource *resource,
                           unsigned frame_index,
                           int16_t pcm[NPE_AUDIO_MODE5_PCM_SAMPLES],int deferred) {
    NpeAudioMode5Fields fields;
    NpeAudioMode5Decoder next;
    int32_t lsf[10], subframe_lsf[10];
    int16_t available[240], adaptive[80], fixed[80], mixed[80], coefficients[11];
    int16_t current_excitation[160];
    int anchor = -1, previous_delay = 0;
    unsigned subframe, i;
    if (!decoder || !pcm || !resource || frame_index != decoder->next_frame) return -1;
    next = *decoder;
    if(!frame_index){if(npe_audio_input_open(&next.input,resource))return -1;}
    else if(resource->bytes!=next.input.resource.bytes||resource->size!=next.input.resource.size||
            resource->header_count!=next.input.resource.header_count)return -1;
    if(deferred?npe_audio_input_fields_deferred(&next.input,&fields):
                npe_audio_input_fields(&next.input,&fields))return -1;
    lsf_vector(&fields, &next, lsf);
    for (subframe = 0; subframe < 2; ++subframe) {
        int pitch_code = field_value(&fields, subframe ? 0x46a : 0x468);
        int gain_code = field_value(&fields, 0x46c + subframe * 2);
        int delay, fraction;
        if (pitch_code < 0 || gain_code < 0 || gain_code >= 16) return -1;
        decode_pitch(previous_delay, pitch_code, &delay, &fraction);
        if (delay < 1 || delay + (fraction ? 14 : 0) > (int)(160 + subframe * 80))
            return -1;
        previous_delay = delay;
        for (i = 0; i < 10; ++i) {
            int64_t weighted = (int64_t)lsf[i] *
                (subframe ? 32767 : 16384) +
                (int64_t)next.previous_lsf[i] * (subframe ? 0 : 16383);
            subframe_lsf[i] = word16(shift32(weighted, 15));
        }
        if (lpc_coefficients(subframe_lsf, coefficients)) return -1;
        memcpy(available, next.excitation, 160 * sizeof(*available));
        if (subframe) memcpy(available + 160, current_excitation,
                             80 * sizeof(*current_excitation));
        adaptive_pulses(available, 160 + subframe * 80,
                        delay, fraction, adaptive);
        if (fixed_pulses(&fields, subframe, &anchor, fixed)) return -1;
        mix_pulses(adaptive, fixed, pitch_gains[gain_code], delay, mixed);
        synthesize(mixed, coefficients, next.pcm_history, pcm + subframe * 80);
        memcpy(current_excitation + subframe * 80, mixed,
               80 * sizeof(*mixed));
    }
    memcpy(next.excitation, current_excitation, sizeof(next.excitation));
    for (i = 0; i < 10; ++i) next.previous_lsf[i] = (int16_t)lsf[i];
    next.next_frame = frame_index + 1;
    *decoder = next;
    return 0;
}

int npe_audio_mode5_decode(NpeAudioMode5Decoder *decoder,const NpeAudioResource *resource,
                           unsigned frame_index,int16_t pcm[NPE_AUDIO_MODE5_PCM_SAMPLES]) {
    return mode5_decode(decoder,resource,frame_index,pcm,0);
}
int npe_audio_mode5_decode_deferred(NpeAudioMode5Decoder *decoder,const NpeAudioResource *resource,
                                    unsigned frame_index,int16_t pcm[NPE_AUDIO_MODE5_PCM_SAMPLES]) {
    return mode5_decode(decoder,resource,frame_index,pcm,1);
}
