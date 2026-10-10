#ifndef NPE_AUDIO_H
#define NPE_AUDIO_H

#include "npe.h"

enum { NPE_AUDIO_MODE5_FRAME_BYTES = 21, NPE_AUDIO_MODE5_FIELDS = 37 };
enum { NPE_AUDIO_MODE5_PCM_SAMPLES = 160 };
enum { NPE_AUDIO_INPUT_WORDS = 512, NPE_AUDIO_SOURCE_WORDS = 2048 };
enum { NPE_AUDIO_OUTPUT_REPEAT = 4, NPE_AUDIO_OUTPUT_RATE = 32000,
       NPE_AUDIO_GAIN_BLOCK_SAMPLES = 80 };
typedef struct {
    uint64_t value; /* Original4A52/54/56:48-bit milliseconds with16 fractional bits. */
    uint32_t increment; /* Original4A58/5A:rounded per-service Q16 milliseconds. */
} NpeAudioClock;
/* Original08:385C,37F4,3820,37CE,37BA. These are driver service time,
   independent of callback sizes, physical DAC position and wall time. */
int npe_audio_clock_configure(NpeAudioClock *,uint16_t rate,uint16_t words);
void npe_audio_clock_advance(NpeAudioClock *);
void npe_audio_clock_retract(NpeAudioClock *);
void npe_audio_clock_add_ms(NpeAudioClock *,uint32_t milliseconds);
uint32_t npe_audio_clock_ms(const NpeAudioClock *);
typedef struct { uint8_t current, target; } NpeAudioGain;
/* PRG08:3536 advances the resident current level once per call, even for
   zero samples. Its two rounded/saturated MACs run before08:372A repeats
   each output word four times. This is separate from the candidate codec. */
int npe_audio_gain_block(NpeAudioGain *gain,int16_t *samples,size_t count);
int16_t npe_audio_gain_sample(int16_t sample,unsigned level);

typedef struct {
    uint16_t half_index, half_samples, output_words;
} NpeAudioOutput;
/* Original2E:7FE4 half scheduling around the codec/transfer boundaries.
   decoded_samples is used only at half_index>=2. Return1 for a prepared
   half (including a zero-length half),0 for a zero codec return, or-1.
   The immutable source and separate host half preserve the original gain
   results without overwriting the candidate decoder's raw PCM diagnostic.
   flags8a.7 is set from the gain word BEFORE that call advances its level. */
int npe_audio_output_half(NpeAudioOutput *output,NpeAudioGain *gain,
                          uint16_t *flags8a,const int16_t *pcm,size_t capacity,
                          uint16_t decoded_samples,int16_t *half,size_t half_capacity);

typedef struct {
    int16_t current[2], target[2];
    uint16_t repeat_argument;
} NpeAudioDacRamp;
/* One original08:33FA/3462 loop iteration: left write before right write.
   Return changed-channel bits (1=left,2=right), zero at the target, or -1.
   Only a right write runs REP's argument+1 NOPs. This instruction count
   has no assumed relation to SDL sample rate or host milliseconds. */
int npe_audio_dac_ramp_step(NpeAudioDacRamp *ramp,uint32_t *delay_nops);

typedef struct {
    uint8_t values[NPE_AUDIO_MODE5_FIELDS];
} NpeAudioMode5Fields;

typedef struct {uint32_t address;uint16_t remainder;} NpeAudioCursor;
typedef struct {
    uint16_t header_bits, decoded_samples;
    uint32_t frame_bits, frame_ms_q12, total_ms, elapsed_ms, start_bits, length_bits;
} NpeAudioStreamPlan;
/* Original26:8D2A around seven B816 positions and three B7AA returns.
   Positions are initial,before0,after0,before1,after1,before2,after2.
   All32-bit products/shifts wrap as on the guest; range parameters are ms.
   This does not implement those missing codec services. */
int npe_audio_stream_plan(const NpeAudioHeader *header,const NpeAudioCursor positions[7],
                          const uint16_t samples[3],uint32_t start_ms,uint32_t length_ms,
                          NpeAudioStreamPlan *plan);

typedef struct {
    uint32_t start_bytes,length_bytes,remaining,seek_offset;
    uint16_t bit_remainder,read_request;
    int seek_required;
} NpeAudioSeekPlan;
/* Original25:E002..E1C6 converts the probed bit range to even24-bit byte
   addresses. An equal-to-remaining start takes the seek/zero-read path.
   This plans the codec reset/restore boundary; it does not implement it. */
int npe_audio_seek_plan(const NpeAudioStreamPlan *stream,uint32_t remaining,
                        uint32_t resource_offset,NpeAudioSeekPlan *plan);

/* Original2A:87EA/31:0276 input window and two producer buffers. Words
   already have the original byte-lane normalization. Driver flags mirror
   DPRAM F012/F014. This does not supply the missing B816 codec cursor. */
typedef struct {
    uint16_t window[NPE_AUDIO_INPUT_WORDS];
    uint16_t source[2][NPE_AUDIO_SOURCE_WORDS];
    unsigned source_index, source_offset;
    uint16_t available, pending[2], debt, flags89, flags8a;
    uint32_t tail_pointer, frame_bits, file_remaining;
    NpeAudioResource resource;
    NpeAudioStreamPlan plan;
    NpeAudioSeekPlan seek;
    uint16_t initial_output_words; /* Original2AE6 before initial PCM service. */
    size_t file_offset;
    unsigned word_cursor, bits_left, frames_read;
    uint16_t bit_word;
    int initialized;
} NpeAudioInput;

enum { NPE_AUDIO_SERVICE_PCM, NPE_AUDIO_SERVICE_AUXILIARY, NPE_AUDIO_SERVICE_PRODUCER };
enum { NPE_AUDIO_SERVICE_NEXT, NPE_AUDIO_SERVICE_READY,
       NPE_AUDIO_SERVICE_YIELD, NPE_AUDIO_SERVICE_RESUME };
/* Original08:3C22/3D70/3EC2 callback gates, AFTER the IRQ entry/EOF and
   physical output checks. Caller serializes the test-and-set. NEXT follows
   the next gate; YIELD/RESUME identify405A/4070 hardware routes, not their
   implementation. A ready callback consumes its request BEFORE invocation.
   flags8e is required only for the PCM callback's8E.1 acknowledgement. */
int npe_audio_service_begin(uint16_t *flags89,uint16_t flags8a,
                           uint16_t flags8d,uint16_t *flags8e,unsigned service);
/* Clear only this callback's current activity bit after its return. */
void npe_audio_service_end(uint16_t *flags89,unsigned service);

enum { NPE_AUDIO_TRANSPORT_RETURN, NPE_AUDIO_TRANSPORT_PCM,
       NPE_AUDIO_TRANSPORT_AUXILIARY, NPE_AUDIO_TRANSPORT_BLOCKED };
typedef struct {
    /*16:EDFA,24:9AB8,24:98FC,24:9B02 are explicit backend boundaries.
      Hooks may change live flags and counters, just as the original calls. */
    int (*adjust)(void *context);
    int (*pause)(void *context);
    int (*refill)(void *context);
    int (*resume)(void *context);
} NpeAudioTransportBackend;
/* Original08:405A; flag10 is set AFTER adjust/pause return. */
int npe_audio_transport_suspend(uint16_t *flags89,uint16_t flags88,
                               const NpeAudioTransportBackend *,void *context);
/* Original08:4070..40B4; return identifies the next software route, not
   a complete IRQ frame or physical transfer engine. Read current flags
   after backend calls; EOF bypasses activity0/1, but not other entry gates. */
int npe_audio_transport_resume(uint16_t *flags89,const uint16_t *flags8a,
    const uint16_t *flags8e,const uint16_t controls[2],
    const NpeAudioTransportBackend *,void *context);
/* Original08:3994 followed by40C0/3EF4 at the producer callback entry.
   Unlike3EC2, this foreground path does not test8A.6 or89.10. Caller
   serializes the test-and-set; successful entry consumes request6. */
int npe_audio_producer_poll_begin(uint16_t *flags89,uint16_t flags8d);

enum { NPE_AUDIO_EOF_IGNORED, NPE_AUDIO_EOF_BUSY, NPE_AUDIO_EOF_READY };
/* Original08:3B48 EOF route and39B6 before the stop services. stack_word
   is the SP value observed by the original comparison AFTER IRQ entry and
   its first push. Other IRQ branches are outside this EOF-only service.
   Early stack/callback guards preserve flags; the busy guards run after
   clearing89.12/13/14. No registered-callback decision is made until stop. */
int npe_audio_eof_gate(uint16_t *flags89,uint16_t flags8d,uint16_t stack_word);
/* Original39F4/3A00 and3B3E callback entry/return. Read the LIVE register
   bit at entry, and clear the active bit in current state on return. */
int npe_audio_eof_callback_begin(uint16_t *flags89);
void npe_audio_eof_callback_end(uint16_t *flags89);
/* Partial08:3292 DPRAM contract: preserve the active callback and mute
   bits, clear remaining89 bits and reset both input producer requests. */
void npe_audio_cleanup_flags(uint16_t *flags89,uint16_t *flags8a);

/* Original2F:66F2 plus24:99F6/9A0A acknowledgements. The saved time is
   the integer32 bits of the48-bit clock, captured BEFORE either half.
   cursor retains B816's last reported pointer even after B84E restores it.
   Transfer reloads are software words4AAC/4AAE, not physical DTU progress. */
typedef struct {
    uint32_t time_snapshot;
    NpeAudioCursor cursor;
    uint16_t flags8e, transfer_words, reload_words[2];
} NpeAudioPcmService;
typedef struct {
    /* Prepare2E:7FE4 (including its missing codec/transfer boundaries).
       Zero is successful even when the codec returns zero output words. */
    int (*prepare)(void *context,unsigned half,uint32_t destination);
    int (*cursor)(void *context,NpeAudioCursor *position);
    int (*restore)(void *context,NpeAudioCursor position);
} NpeAudioPcmBackend;
/* Service live half-demand bits14/15 in order; acknowledge, query cursor,
   advance clock, optionally compact/refill and restore, then sticky EOF.
   Return serviced halves0..2, or-1 for unsupported host buffer arguments.
   The caller owns file production and real codec/IRQ/DTU implementations. */
int npe_audio_pcm_service(NpeAudioPcmService *,NpeAudioInput *,NpeAudioOutput *,
                           NpeAudioClock *,const NpeAudioPcmBackend *,void *);

/* Compaction/refill only: consumed is an even-addressed codec cursor's
   word distance from8800. No file producer, PCM or time is inferred. */
int npe_audio_input_refill(NpeAudioInput *input,unsigned consumed_words);
/* Original2F:67FC..6826 sticky EOF decision, on unsigned two-word pointers. */
void npe_audio_input_check_end(NpeAudioInput *input,uint16_t output_words,
                              uint32_t codec_pointer);
/* File IO boundary supplies raw DMCU words and an actual byte count <= the
   request. A final odd byte occupies the last word's high lane. Original
   2B:2894 applies XOR, swaps only complete words and updates driver state.
   The callback does not update file_remaining; the producer owns it. */
typedef size_t (*NpeAudioInputReader)(void *context,unsigned source,
                                    uint16_t *destination,size_t requested);
/* Execute the range's window read after the caller seeks to seek_offset.
   Return0 for reset/no read,1 for a positive read requiring B84E restore,
   -2 for the original zero-read failure, or-1 for invalid host arguments.
   Read counts do not subtract from plan.remaining, matching the original.
   The callback receives source0 and raw words exactly as for the producer. */
int npe_audio_seek_window(const NpeAudioSeekPlan *plan,uint16_t *window,
                          NpeAudioInputReader reader,void *context,uint8_t xor_key);
int npe_audio_input_produce(NpeAudioInput *input,NpeAudioInputReader reader,
                            void *context,uint8_t xor_key);
/* Original25:E24C..E2F8 initial window accounting and first-source adoption.
   file_remaining includes the already loaded initial window before this call.
   PCM prefill and its subsequent status/EOF decision are separate services. */
int npe_audio_input_prime(NpeAudioInput *input,NpeAudioInputReader reader,
                          void *context,uint8_t xor_key);
/* Candidate word reader connects that buffer service to current-NPE input.
   Its cursor and168-bit frame size are candidate facts, not B816 proof. */
int npe_audio_input_open(NpeAudioInput *input,const NpeAudioResource *resource);
int npe_audio_input_fields(NpeAudioInput *input,NpeAudioMode5Fields *fields);
/* Candidate adapter: defer compaction to the PCM service rather than doing
   it inside the codec. Cached partial-word bits survive a window restore. */
int npe_audio_input_fields_deferred(NpeAudioInput *,NpeAudioMode5Fields *);
int npe_audio_input_candidate_cursor(NpeAudioInput *,NpeAudioCursor *);
int npe_audio_input_candidate_restore(NpeAudioInput *,NpeAudioCursor);
int npe_audio_input_candidate_produce(NpeAudioInput *);

/* Candidate MobiVoice mode-5 field layout, not a verified mask-ROM decoder.
   The format-13 payload is swapped in 16-bit pairs across frame boundaries.
   This extracts compressed fields only; it does not produce PCM. */
int npe_audio_mode5_fields(const NpeAudioResource *resource,
                           unsigned frame_index, NpeAudioMode5Fields *fields);

typedef struct {
    int16_t lsf_predictor[10], previous_lsf[10];
    int16_t excitation[160], pcm_history[10];
    unsigned next_frame;
    NpeAudioInput input;
} NpeAudioMode5Decoder;

/* Candidate-compatible standalone decoder. Initialize once per resource,
   then decode frame indices in order. Output is signed 16-bit mono PCM.
   Candidate equivalence does not establish identity with the NP560 ROM. */
void npe_audio_mode5_init(NpeAudioMode5Decoder *decoder);
/* Same candidate algorithm; window refill is performed by its caller. */
int npe_audio_mode5_decode_deferred(NpeAudioMode5Decoder *,const NpeAudioResource *,
                                    unsigned,int16_t[NPE_AUDIO_MODE5_PCM_SAMPLES]);
int npe_audio_mode5_decode(NpeAudioMode5Decoder *decoder,
                           const NpeAudioResource *resource,
                           unsigned frame_index,
                           int16_t pcm[NPE_AUDIO_MODE5_PCM_SAMPLES]);

#endif
