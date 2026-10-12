#include "bda_sdk.h"
#include <SDL.h>
#include "touch_ui.h"

static SDL_Window window;
static SDL_Renderer renderer;
static SDL_Texture texture;
static bda_handle_t frame,draw,back,owner;
static void *brush;
static Uint32 initialized;
static int timer_started,window_timer_started;
static volatile int detached,dirty,touch_active,touch_pending,escape_suppressed;
static volatile u32 touch_value;
static volatile u32 touch_current;
static u32 previous,repeat_due;
static volatile int escape_pending;
static u32 escape_due;
static int held_key;
static int keyboard_page;
static Uint16 touch_modifiers;
static int touch_held_key,touch_used;
static Uint32 touch_repeat_due;
static u8 vx[24+240*320*2];
static SDL_Event events[32];
static unsigned head,tail;
static char error_text[160];
static bda_frame_desc_t descriptor;
extern uint16_t *npe_bbk_game_mode;
extern void npe_bbk_log_reset(void);

static SDL_AudioSpec audio;
static int audio_open,audio_paused,audio_lock,audio_failed,audio_pumping;
static int audio_attenuation,audio_before_mute,audio_original=-1,audio_applied=-1;
static int16_t source[1024],output[512],silence[512];
static unsigned pcm_count,pcm_offset,resample_phase,source_offset=1024;
static unsigned audio_blocks,audio_bytes,audio_callbacks,audio_peak,audio_short_writes;
static unsigned audio_ready_misses;
static Uint32 audio_last_progress;
static int audio_write_error;
static void audio_pump(void){
    if(!audio_open||audio_paused||audio_lock||audio_pumping)return;
    audio_pumping=1;
    if(!bda_audio_ready()){
        ++audio_ready_misses;
        if(SDL_GetTicks()-audio_last_progress>1500&&!audio_failed){audio_failed=1;dirty=1;snprintf(error_text,sizeof(error_text),"PCM queue stalled");}
        audio_pumping=0;return;
    }
    /* Match the SDK's true-hardware-verified 1024-byte PCM writes. Firmware
       queue readiness supplies backpressure; a fresh 32ms deadline after
       every frame used to add GUI delays and starve the physical stream. */
    for(int burst=0;burst<4&&bda_audio_ready();++burst){
        if(!pcm_offset){
            while(pcm_count<512){
                if(source_offset==1024){
                    audio.callback(audio.userdata,(Uint8 *)source,sizeof(source));
                    source_offset=0;++audio_callbacks;
                }
                int16_t sample=source[source_offset++];
                resample_phase+=22050;
                if(resample_phase>=32000){resample_phase-=32000;output[pcm_count++]=sample;}
            }
            for(unsigned i=0;i<512;++i){unsigned peak=output[i]<0?-(int)output[i]:output[i];if(peak>audio_peak)audio_peak=peak;}
        }
        unsigned bytes=(pcm_count-pcm_offset)*2;
        if(audio_attenuation==96)memset(output+pcm_offset,0,bytes);
        int used=bda_audio_write(output+pcm_offset,bytes);
        if(used<0||(unsigned)used>bytes||(used&1)){
            audio_write_error=used;audio_failed=1;dirty=1;
            snprintf(error_text,sizeof(error_text),"PCM write failed (%d)",used);
            audio_pumping=0;SDL_CloseAudioDevice(1);return;
        }
        if(!used){++audio_short_writes;break;}
        if((unsigned)used!=bytes)++audio_short_writes;
        audio_bytes+=(unsigned)used;
        audio_last_progress=SDL_GetTicks();
        if(audio_failed){audio_failed=0;dirty=1;}
        if(!audio_blocks)audio_applied=bda_audio_get_attenuation();
        pcm_offset+=(unsigned)used/2;
        if(pcm_offset==pcm_count){pcm_count=pcm_offset=0;++audio_blocks;}
    }
    audio_pumping=0;
}
static void release_draw(void){if(draw&&(s32)draw!=-1)bda_gui_end_draw(draw);draw=owner=0;}
static void acquire(bda_handle_t h){if(draw&&owner==h)return;release_draw();draw=bda_gui_current_draw(h);if((s32)draw==-1)draw=0;owner=h;}
static int proc(bda_handle_t h,u32 msg,u32 wp,u32 lp){
    if(msg==BDA_MSG_DRAW_CONTEXT_ATTACH){acquire(h);dirty=1;}
    if(msg==BDA_MSG_DRAW_CONTEXT_DETACH){release_draw();detached=1;}
    if(msg==BDA_MSG_REDRAW_INPUT)dirty=1;
    if(msg==BDA_MSG_WINDOW_TIMER)return 1;
    /* The prefix must reach default_proc so firmware posts coordinates. */
    if(msg==0x21){escape_suppressed=1;escape_pending=0;}
    if(msg==BDA_MSG_TOUCH_COORDINATE){touch_active=1;touch_current=lp;dirty=1;escape_suppressed=1;escape_pending=0;return 1;}
    if(msg==BDA_MSG_TOUCH_RELEASE){touch_active=0;dirty=1;escape_suppressed=1;escape_pending=0;touch_value=lp;touch_pending=1;return 1;}
    return bda_gui_default_proc(h,msg,wp,lp);
}
static void rect(int x,int y,int w,int h,u16 c){for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i){unsigned n=24+(j*240+i)*2;vx[n]=c;vx[n+1]=c>>8;}}
static int confirm_key(void){return npe_bbk_game_mode&&(*npe_bbk_game_mode==2||*npe_bbk_game_mode==5)?SDLK_SPACE:SDLK_RETURN;}
static int touch_key(u32 packed){int k=npe_touch_hit(keyboard_page,packed&65535,packed>>16);
    return k==SDLK_RETURN&&!keyboard_page?confirm_key():k;
}
static void present(void){if(!draw||!back)return;
    npe_touch_draw(vx+24,keyboard_page,touch_modifiers,(96-audio_attenuation)*100/96,audio_failed,
                   touch_active?(int)(touch_current&65535):-1,
                   touch_active?(int)(touch_current>>16):-1);
    bda_gui_draw_vx(back,0,0,vx);bda_gui_draw_guard_begin();void *old=bda_gui_select_draw_object(draw,brush);
    bda_gui_context_copy(back,0,0,240,320,draw,0,0,0);
    bda_gui_select_draw_object(draw,old);bda_gui_draw_guard_end();dirty=0;}
int SDL_Init(Uint32 flags){initialized=flags;head=tail=0;detached=touch_active=touch_pending=escape_suppressed=escape_pending=0;previous=repeat_due=0;held_key=touch_held_key=touch_used=0;keyboard_page=0;touch_modifiers=0;audio_attenuation=audio_before_mute=audio_failed=0;npe_bbk_log_reset();if(!timer_started){bda_gui_millisecond_timer_start();timer_started=1;}return 0;}
int SDL_InitSubSystem(Uint32 flags){initialized|=flags;return 0;}
Uint32 SDL_WasInit(Uint32 flags){return initialized&flags;}
void SDL_Quit(void){SDL_CloseAudioDevice(1);if(timer_started){bda_gui_millisecond_timer_stop();timer_started=0;}initialized=0;}
Uint32 SDL_GetTicks(void){return timer_started?bda_gui_millisecond_count():bda_gui_tick_count_25ms()*25;}
uint64_t SDL_GetPerformanceCounter(void){return SDL_GetTicks();}
const char *SDL_GetError(void){return error_text[0]?error_text:"9588 platform service unavailable";}
static char *copy_string(const char *s){size_t n=strlen(s)+1;char *p=malloc(n);if(p)memcpy(p,s,n);return p;}
char *SDL_GetBasePath(void){return copy_string("A:\\NPE\\");}
char *SDL_GetPrefPath(const char *org,const char *app){(void)org;(void)app;bda_fs_mkdir("A:\\NPE");bda_fs_mkdir("A:\\NPE\\saves");return copy_string("A:\\NPE\\saves\\");}
SDL_Window *SDL_CreateWindow(const char *title,int x,int y,int w,int h,Uint32 flags){(void)title;(void)x;(void)y;(void)w;(void)h;(void)flags;bda_frame_desc_t *d=&descriptor;memset(d,0,sizeof(*d));memset(vx,0,sizeof(vx));vx[0]='V';vx[1]='X';for(int i=2;i<6;++i)vx[i]=204;vx[6]=240;vx[10]=64;vx[11]=1;for(int i=14;i<20;++i)vx[i]=204;for(int i=20;i<24;++i)vx[i]=255;rect(0,0,240,320,0x18c3);d->title="NOAH NPE";d->wndproc=proc;d->height=240;d->width=320;frame=bda_gui_register_frame_desc(d);if(!frame||(s32)frame==-1){frame=0;return NULL;}bda_gui_frame_activate(frame,0x100);acquire(frame);brush=bda_gui_draw_object_create(7);if(!draw||!brush||(s32)(u32)brush==-1){SDL_DestroyWindow(&window);return NULL;}back=bda_gui_compatible_context_create(draw);if(!back||(s32)back==-1){back=0;SDL_DestroyWindow(&window);return NULL;}window_timer_started=bda_gui_window_timer_start(frame,1,10);if(!window_timer_started){SDL_DestroyWindow(&window);return NULL;}dirty=1;return &window;}
void SDL_DestroyWindow(SDL_Window *w){(void)w;if(frame){if(window_timer_started)bda_gui_window_timer_stop(frame,1);window_timer_started=0;bda_gui_frame_stop(frame);bda_gui_frame_release(frame);for(int i=0;i<128&&!detached;++i){bda_gui_message_t m;bda_gui_event_pump_frame_once(&m,frame);}if(back)bda_gui_compatible_context_free(back);back=0;release_draw();bda_gui_close_frame(frame);frame=0;}}
void SDL_SetWindowTitle(SDL_Window *w,const char *s){(void)w;(void)s;}
SDL_Window *SDL_GetKeyboardFocus(void){return frame?&window:NULL;}
SDL_Renderer *SDL_CreateRenderer(SDL_Window *w,int i,Uint32 flags){(void)w;(void)i;(void)flags;return &renderer;}
SDL_Renderer *SDL_GetRenderer(SDL_Window *w){(void)w;return &renderer;}
void SDL_DestroyRenderer(SDL_Renderer *r){(void)r;}
int SDL_RenderSetLogicalSize(SDL_Renderer *r,int w,int h){(void)r;return w==240&&h==120?0:-1;}
SDL_Texture *SDL_CreateTexture(SDL_Renderer *r,Uint32 f,int a,int w,int h){(void)r;(void)f;(void)a;return w==240&&h==120?&texture:NULL;}
void SDL_DestroyTexture(SDL_Texture *t){(void)t;}
int SDL_UpdateTexture(SDL_Texture *t,const void *rect_unused,const void *p,int pitch){(void)t;(void)rect_unused;for(int y=0;y<120;++y){const Uint32 *s=(const Uint32 *)((const Uint8 *)p+y*pitch);for(int x=0;x<240;++x){Uint32 c=s[x];u16 rgb=((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31);if(!rgb)rgb=1;unsigned n=24+((y+28)*240+x)*2;vx[n]=rgb;vx[n+1]=rgb>>8;}}dirty=1;return 0;}
int SDL_RenderClear(SDL_Renderer *r){(void)r;return 0;}
int SDL_RenderCopy(SDL_Renderer *r,SDL_Texture *t,const void *src,const void *dst){(void)r;(void)t;(void)src;(void)dst;return 0;}
void SDL_RenderPresent(SDL_Renderer *r){(void)r;present();}
int SDL_PushEvent(SDL_Event *e){unsigned n=(tail+1)%32;if(n==head)return -1;events[tail]=*e;tail=n;return 1;}
int SDL_HasEvent(Uint32 type){for(unsigned i=head;i!=tail;i=(i+1)%32)if(events[i].type==type)return 1;return 0;}
static void modified_key_event(int key,int repeat,Uint16 modifiers){if(!key)return;SDL_Event e;SDL_zero(e);if(key==NPE_TOUCH_QUIT)e.type=SDL_QUIT;else {e.type=SDL_KEYDOWN;e.key.keysym.sym=key;e.key.keysym.mod=modifiers;e.key.state=1;e.key.repeat=repeat;}SDL_PushEvent(&e);}
static void key_event(int key,int repeat){modified_key_event(key,repeat,0);}
static int direction_key(int key){return key==SDLK_UP||key==SDLK_DOWN||key==SDLK_LEFT||key==SDLK_RIGHT;}
static void touch_dispatch(int k){
    if(!k)return;
    if(k==NPE_TOUCH_ABOUT){
        /* Firmware dialogs use GBK: "author:" followed by the author name. */
        bda_msgbox("NOAH NPE","\xd7\xf7\xd5\xdf\xa3\xba\n" "HelloClyde");
        touch_active=touch_pending=touch_used=touch_held_key=escape_pending=0;
        escape_suppressed=1;held_key=0;
        audio_last_progress=SDL_GetTicks();
    }
    else if(k==NPE_TOUCH_PAD)keyboard_page=NPE_UI_PAD;
    else if(k==NPE_TOUCH_ALPHA)keyboard_page=NPE_UI_ALPHA;
    else if(k==NPE_TOUCH_SYMBOLS)keyboard_page=NPE_UI_SYMBOLS;
    else if(k==NPE_TOUCH_FUNCTIONS)keyboard_page=keyboard_page==NPE_UI_FUNCTIONS?NPE_UI_ALPHA:NPE_UI_FUNCTIONS;
    else if(k==NPE_TOUCH_SHIFT)touch_modifiers^=KMOD_SHIFT;
    else if(k==NPE_TOUCH_CAPS)touch_modifiers^=KMOD_CAPS;
    else if(k==NPE_TOUCH_CTRL)touch_modifiers^=KMOD_CTRL;
    else if(k==NPE_TOUCH_ALT)touch_modifiers^=KMOD_ALT;
    else if(k==NPE_TOUCH_VOL_UP||k==NPE_TOUCH_VOL_DOWN||k==NPE_TOUCH_MUTE){
        if(k==NPE_TOUCH_MUTE){if(audio_attenuation==96)audio_attenuation=audio_before_mute;else {audio_before_mute=audio_attenuation;audio_attenuation=96;}}
        else {audio_attenuation+=k==NPE_TOUCH_VOL_DOWN?12:-12;if(audio_attenuation<0)audio_attenuation=0;if(audio_attenuation>96)audio_attenuation=96;}
        if(audio_open)bda_audio_set_attenuation((u32)audio_attenuation);
    } else {
        Uint16 mods=keyboard_page==NPE_UI_PAD?0:touch_modifiers;
        if(keyboard_page==NPE_UI_ALPHA&&(mods&KMOD_SHIFT)&&k>='0'&&k<='9')k=k=='0'?')':"!@#$%^&*("[k-'1'];
        /* The upstream quiz maps letter case via modifiers, but its shifted
           punctuation table has only four entries. Literal symbols cover
           the complete printable ASCII set without losing Shift+1 etc. */
        if(k>=32&&k<=126&&!(k>='a'&&k<='z'))mods&=~KMOD_SHIFT;
        modified_key_event(k,0,mods);
        if(k>=32&&k<=126)touch_modifiers&=~KMOD_SHIFT;
    }
    dirty=1;
}
void SDL_PumpEvents(void){
    /* The firmware poll can block after a consumed wake. Consume one message
       per call; the 10 ms frame timer also wakes idle animation/audio waits. */
    if(frame&&!detached){bda_gui_message_t m;bda_gui_event_pump_frame_once(&m,frame);}
    if(detached&&!SDL_HasEvent(SDL_QUIT)){SDL_Event e;SDL_zero(e);e.type=SDL_QUIT;SDL_PushEvent(&e);}
    Uint32 bits=0;
    if(frame&&!detached){bda_gui_input_packet_t p;bda_gui_input_packet(&p);for(int i=0;i<6;++i)if(p.bytes[i]==1)bits|=1u<<i;}
    Uint32 touch_now=SDL_GetTicks();
    if(touch_active){
        u16 x,y;bda_gui_touch_position(&x,&y);u32 packed=(u32)x|((u32)y<<16);
        if(packed!=touch_current){touch_current=packed;dirty=1;}
        int k=touch_key(touch_current);
        if(direction_key(k)){
            if(k!=touch_held_key){touch_held_key=k;touch_repeat_due=touch_now+280;}
            /* The release message can lag behind a busy GUI queue. The
               hardware bit drops immediately; do not repeat a lifted tap. */
            else if((bits&16)&&SDL_TICKS_PASSED(touch_now,touch_repeat_due)){touch_repeat_due=touch_now+90;key_event(k,touch_used);touch_used=1;}
        }else touch_held_key=0;
    }
    if(touch_pending){int k=touch_key(touch_value);touch_pending=0;if(!touch_used)touch_dispatch(k);touch_used=touch_held_key=0;
        if(k==NPE_TOUCH_ABOUT&&frame&&!detached){
            /* Consume the dialog's dismissal key instead of confirming the game. */
            bda_gui_input_packet_t p;bits=0;bda_gui_input_packet(&p);
            for(int i=0;i<6;++i)if(p.bytes[i]==1)bits|=1u<<i;
            previous=bits;
        }
    }
    if(frame&&!detached){Uint32 now=SDL_GetTicks();
        if(escape_suppressed){if(!touch_active&&!(bits&16))escape_suppressed=0;bits&=~16u;escape_pending=0;}
        Uint32 pressed=bits&~previous;previous=bits;const int physical[]={SDLK_RIGHT,SDLK_LEFT,SDLK_DOWN,SDLK_UP,SDLK_F4,SDLK_RETURN};
        /* Touch also sets the hardware Escape bit. Defer physical F4 until
           release, then allow the queued touch prefix to cancel it. A deadline
           measured from press could expire during a long touch/audio callback. */
        if(pressed&16)escape_pending=1;
        if(escape_pending==1&&!(bits&16)){escape_pending=2;escape_due=now+150;}
        pressed&=~16u;
        if(escape_pending==2&&SDL_TICKS_PASSED(now,escape_due)){escape_pending=0;key_event(SDLK_F4,0);}
        for(int i=0;i<6;++i)if(pressed&(1u<<i))key_event(i==5?confirm_key():physical[i],0);
        int k=0;for(int i=0;i<4;++i)if(bits&(1u<<i)){k=physical[i];break;}if(k!=held_key){held_key=k;repeat_due=now+300;}else if(k&&SDL_TICKS_PASSED(now,repeat_due)){key_event(k,1);repeat_due=now+100;}
    }
    audio_pump();if(dirty)present();
}
int SDL_PollEvent(SDL_Event *e){SDL_PumpEvents();if(head==tail)return 0;*e=events[head];head=(head+1)%32;return 1;}
int SDL_WaitEventTimeout(SDL_Event *e,int timeout){Uint32 start=SDL_GetTicks();for(;;){if(SDL_PollEvent(e))return 1;if(timeout>=0&&SDL_GetTicks()-start>=(Uint32)timeout)return 0;bda_sys_delay(1);}}
void SDL_Delay(Uint32 ms){Uint32 start=SDL_GetTicks();do{SDL_PumpEvents();if(SDL_GetTicks()-start>=ms)break;bda_sys_delay(1);}while(1);}
Uint32 SDL_RegisterEvents(int n){static Uint32 next=0x8000;Uint32 v=next;next+=n;return v;}
SDL_RWops *SDL_RWFromFile(const char *p,const char *m){return fopen(p,m);}
Sint64 SDL_RWsize(SDL_RWops *f){long old=ftell(f);if(old<0||fseek(f,0,SEEK_END))return -1;long size=ftell(f);if(fseek(f,old,SEEK_SET))return -1;return size;}
size_t SDL_RWread(SDL_RWops *f,void *p,size_t s,size_t n){return fread(p,s,n,f);}
size_t SDL_RWwrite(SDL_RWops *f,const void *p,size_t s,size_t n){return fwrite(p,s,n,f);}
int SDL_RWclose(SDL_RWops *f){return fclose(f);}
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *name,int capture,const SDL_AudioSpec *want,SDL_AudioSpec *got,int flags){
    (void)name;(void)flags;
    if(capture||want->freq!=32000||want->channels!=1||want->format!=AUDIO_S16SYS){audio_failed=1;memcpy(error_text,"Unsupported PCM format",23);return 0;}
    if(audio_open)SDL_CloseAudioDevice(1);
    audio=*want;if(got)*got=*want;
    audio_original=bda_audio_get_attenuation();
    bda_audio_open_pcm(22050,16,1);
    bda_audio_set_attenuation((u32)audio_attenuation);
    audio_open=1;audio_paused=1;audio_failed=audio_pumping=audio_lock=0;
    pcm_count=pcm_offset=resample_phase=0;source_offset=1024;
    audio_blocks=audio_bytes=audio_callbacks=audio_peak=audio_short_writes=0;
    audio_ready_misses=0;audio_last_progress=SDL_GetTicks();
    audio_write_error=0;audio_applied=-1;
    fprintf(stderr,"9588 PCM 22050/16/1: original attenuation=%d, requested=%d, block=1024\n",audio_original,audio_attenuation);
    dirty=1;return 1;
}
void SDL_CloseAudioDevice(SDL_AudioDeviceID d){
    (void)d;if(!audio_open)return;
    int restored=-1;
    if(audio_original>=0&&audio_original<=96){
        Uint32 start=SDL_GetTicks();
        while(!bda_audio_ready()&&SDL_GetTicks()-start<250)bda_sys_delay(1);
        if(bda_audio_ready()){
            memset(silence,0,sizeof(silence));bda_audio_set_attenuation((u32)audio_original);
            if(bda_audio_write(silence,sizeof(silence))==(int)sizeof(silence))restored=bda_audio_get_attenuation();
        }
    }
    bda_audio_stop();audio_open=0;pcm_count=pcm_offset=0;
    FILE *log=fopen("A:\\NPE\\AUDIO.LOG","wb");
    if(log){
        fprintf(log,"NPE9588 PCM 22050 Hz / 16-bit / mono\noriginal_attenuation=%d\napplied_initial_attenuation=%d\nrequested_final_attenuation=%d\nrestored_attenuation=%d\nblocks=%u\nbytes=%u\ncallbacks=%u\ninput_peak=%u\nshort_writes=%u\nready_misses=%u\nwrite_error=%d\n",
                audio_original,audio_applied,audio_attenuation,restored,audio_blocks,audio_bytes,audio_callbacks,audio_peak,audio_short_writes,audio_ready_misses,audio_write_error);
        fclose(log);
    }
}
void SDL_PauseAudioDevice(SDL_AudioDeviceID d,int pause){(void)d;audio_paused=pause;}
void SDL_LockAudioDevice(SDL_AudioDeviceID d){(void)d;++audio_lock;}
void SDL_UnlockAudioDevice(SDL_AudioDeviceID d){(void)d;if(audio_lock)--audio_lock;}
int SDL_GetAudioDeviceStatus(SDL_AudioDeviceID d){(void)d;return audio_open&&!audio_paused?SDL_AUDIO_PLAYING:0;}
SDL_sem *SDL_CreateSemaphore(Uint32 n){SDL_sem *s=malloc(sizeof(*s));if(s)s->count=n;return s;}
void SDL_DestroySemaphore(SDL_sem *s){free(s);}
int SDL_SemPost(SDL_sem *s){++s->count;return 0;}
int SDL_SemTryWait(SDL_sem *s){if(!s->count)return 1;--s->count;return 0;}
int SDL_SemWaitTimeout(SDL_sem *s,Uint32 timeout){Uint32 start=SDL_GetTicks();while(SDL_SemTryWait(s)){audio_pump();if(SDL_GetTicks()-start>=timeout)return 1;bda_sys_delay(1);}return 0;}
