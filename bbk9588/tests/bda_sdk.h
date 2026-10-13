#ifndef NPE_TEST_SDK_H
#define NPE_TEST_SDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uintptr_t bda_handle_t;
typedef struct {const char *title;int (*wndproc)(bda_handle_t,u32,u32,u32);int height,width;} bda_frame_desc_t;
typedef struct {int unused;} bda_gui_message_t;
typedef struct {u8 bytes[6];} bda_gui_input_packet_t;
enum {BDA_MSG_DRAW_CONTEXT_ATTACH=100,BDA_MSG_DRAW_CONTEXT_DETACH,
      BDA_MSG_REDRAW_INPUT,BDA_MSG_WINDOW_TIMER,BDA_MSG_TOUCH_COORDINATE=1,BDA_MSG_TOUCH_RELEASE=2};
static u32 mock_ticks;
static u16 mock_x,mock_y;
static u8 mock_keys[6];
static int mock_applied=96,mock_pending=96,mock_stopped,mock_write_error;
static unsigned mock_writes,mock_sizes[256],mock_last_peak;
static int mock_slots=-1,mock_poll_delay;
static unsigned mock_writes_at_poll;
static int mock_vx_y,mock_vx_height,mock_copy_y,mock_copy_height;
static inline void bda_gui_millisecond_timer_start(void){}
static inline void bda_gui_millisecond_timer_stop(void){}
static inline u32 bda_gui_millisecond_count(void){return mock_ticks;}
static inline u32 bda_gui_tick_count_25ms(void){return mock_ticks/25;}
static inline void bda_sys_delay(u32 n){mock_ticks+=n;}
static inline void bda_gui_touch_position(u16 *x,u16 *y){*x=mock_x;*y=mock_y;}
static inline int bda_audio_get_attenuation(void){return mock_applied;}
static inline void bda_audio_set_attenuation(u32 v){mock_pending=(int)v;}
static inline void bda_audio_open_pcm(u32 rate,u32 bits,u32 channels){(void)rate;(void)bits;(void)channels;}
static inline int bda_audio_ready(void){return mock_slots!=0;}
static inline int bda_audio_write(const void *p,unsigned bytes){
    if(mock_write_error)return mock_write_error;
    if(mock_slots==0)return 0;
    if(mock_slots>0)--mock_slots;
    mock_applied=mock_pending;mock_last_peak=0;
    const int16_t *s=p;
    for(unsigned i=0;i<bytes/2;++i){unsigned a=s[i]<0?-(int)s[i]:s[i];if(a>mock_last_peak)mock_last_peak=a;}
    if(mock_writes<256)mock_sizes[mock_writes]=bytes;
    ++mock_writes;return (int)bytes;
}
static inline void bda_audio_stop(void){++mock_stopped;}
static inline int bda_fs_mkdir(const char *s){(void)s;return 0;}
static inline int bda_msgbox(const char *title,const char *message){(void)title;(void)message;return 0;}
static inline int bda_gui_default_proc(bda_handle_t h,u32 m,u32 w,u32 l){return 0;}
static inline void bda_gui_end_draw(bda_handle_t h){}
static inline bda_handle_t bda_gui_current_draw(bda_handle_t h){return 0;}
static inline void bda_gui_draw_vx(bda_handle_t h,int x,int y,const void *p){
    const u8 *v=p;mock_vx_y=y;mock_vx_height=v[10]|(v[11]<<8);
}
static inline void bda_gui_draw_guard_begin(void){}
static inline void bda_gui_draw_guard_end(void){}
static inline void *bda_gui_select_draw_object(bda_handle_t h,void *p){return NULL;}
static inline void bda_gui_context_copy(bda_handle_t a,int x,int y,int w,int z,bda_handle_t b,int xx,int yy,int key){mock_copy_y=y;mock_copy_height=z;}
static inline bda_handle_t bda_gui_register_frame_desc(bda_frame_desc_t *d){return 0;}
static inline void bda_gui_frame_activate(bda_handle_t h,u32 f){}
static inline void *bda_gui_draw_object_create(int n){return NULL;}
static inline bda_handle_t bda_gui_compatible_context_create(bda_handle_t h){return 0;}
static inline int bda_gui_window_timer_start(bda_handle_t h,int n,int ms){return 1;}
static inline void bda_gui_window_timer_stop(bda_handle_t h,int n){}
static inline void bda_gui_frame_stop(bda_handle_t h){}
static inline void bda_gui_frame_release(bda_handle_t h){}
static inline void bda_gui_event_pump_frame_once(bda_gui_message_t *m,bda_handle_t h){mock_writes_at_poll=mock_writes;mock_ticks+=mock_poll_delay;}
static inline void bda_gui_compatible_context_free(bda_handle_t h){}
static inline void bda_gui_close_frame(bda_handle_t h){}
static inline void bda_gui_input_packet(bda_gui_input_packet_t *p){memcpy(p->bytes,mock_keys,sizeof(mock_keys));}
#endif
