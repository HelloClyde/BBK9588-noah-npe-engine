#ifndef NPE_BBK_SDL_H
#define NPE_BBK_SDL_H
/* Only the SDL surface actually consumed by NPE, backed by the public SDK. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef int64_t Sint64;
typedef int32_t SDL_Keycode;
typedef struct {int unused;} SDL_Window,SDL_Renderer,SDL_Texture;
typedef FILE SDL_RWops;
typedef struct {int count;} SDL_sem;
typedef unsigned SDL_AudioDeviceID;
typedef struct {int freq;Uint16 format;Uint8 channels;Uint16 samples;void (*callback)(void *,Uint8 *,int);void *userdata;} SDL_AudioSpec;
typedef struct {SDL_Keycode sym;Uint16 mod;} SDL_Keysym;
typedef struct {Uint32 type;Uint8 state,repeat;SDL_Keysym keysym;} SDL_KeyboardEvent;
typedef union SDL_Event {Uint32 type;SDL_KeyboardEvent key;struct {Uint32 type;Uint8 event;} window;struct {Uint32 type;int which;Uint8 button;} cbutton;struct {Uint32 type;void *data1,*data2;} user;} SDL_Event;
enum {SDL_QUIT=0x100,SDL_WINDOWEVENT=0x200,SDL_KEYDOWN=0x300,SDL_CONTROLLERBUTTONDOWN=0x650,SDL_CONTROLLERDEVICEADDED,SDL_CONTROLLERDEVICEREMOVED};
enum {SDL_WINDOWEVENT_EXPOSED=3,SDL_CONTROLLER_BUTTON_B=1,SDL_CONTROLLER_BUTTON_START=6};
enum {SDLK_UNKNOWN=0,SDLK_BACKSPACE=8,SDLK_RETURN=13,SDLK_ESCAPE=27,SDLK_SPACE=32,SDLK_EQUALS='=',SDLK_DELETE=127,
 SDLK_0='0',SDLK_8='8',SDLK_9='9',SDLK_a='a',SDLK_b='b',SDLK_c='c',SDLK_d='d',SDLK_i='i',SDLK_j='j',SDLK_k='k',SDLK_l='l',SDLK_m='m',SDLK_n='n',SDLK_p='p',SDLK_u='u',
 SDLK_UP=0x40000052,SDLK_DOWN=0x40000051,SDLK_LEFT=0x40000050,SDLK_RIGHT=0x4000004f,SDLK_F4=0x4000003d,SDLK_KP_ENTER=0x40000058,SDLK_PAGEUP=0x4000004b,SDLK_PAGEDOWN=0x4000004e};
enum {SDL_INIT_VIDEO=1,SDL_INIT_AUDIO=2,SDL_INIT_GAMECONTROLLER=4,SDL_WINDOWPOS_CENTERED=0,SDL_WINDOW_RESIZABLE=0,SDL_RENDERER_ACCELERATED=1,SDL_RENDERER_PRESENTVSYNC=2,SDL_RENDERER_SOFTWARE=4,SDL_PIXELFORMAT_ARGB8888=0,SDL_TEXTUREACCESS_STREAMING=0,SDL_AUDIO_PLAYING=1,SDL_PRESSED=1,AUDIO_S16SYS=0x8010,KMOD_SHIFT=3};
#define SDL_zero(x) memset(&(x),0,sizeof(x))
#define KMOD_CAPS 0x2000
#define SDL_memset memset
#define SDL_free free
#define SDL_TICKS_PASSED(a,b) ((int32_t)((a)-(b))>=0)
int SDL_Init(Uint32);
int SDL_InitSubSystem(Uint32);
Uint32 SDL_WasInit(Uint32);
void SDL_Quit(void);
Uint32 SDL_GetTicks(void);
uint64_t SDL_GetPerformanceCounter(void);
const char *SDL_GetError(void);
char *SDL_GetBasePath(void);
char *SDL_GetPrefPath(const char *,const char *);
SDL_Window *SDL_CreateWindow(const char *,int,int,int,int,Uint32);
void SDL_DestroyWindow(SDL_Window *);
void SDL_SetWindowTitle(SDL_Window *,const char *);
SDL_Window *SDL_GetKeyboardFocus(void);
SDL_Renderer *SDL_CreateRenderer(SDL_Window *,int,Uint32);
SDL_Renderer *SDL_GetRenderer(SDL_Window *);
void SDL_DestroyRenderer(SDL_Renderer *);
int SDL_RenderSetLogicalSize(SDL_Renderer *,int,int);
SDL_Texture *SDL_CreateTexture(SDL_Renderer *,Uint32,int,int,int);
void SDL_DestroyTexture(SDL_Texture *);
int SDL_UpdateTexture(SDL_Texture *,const void *,const void *,int);
int SDL_RenderClear(SDL_Renderer *);
int SDL_RenderCopy(SDL_Renderer *,SDL_Texture *,const void *,const void *);
void SDL_RenderPresent(SDL_Renderer *);
int SDL_PollEvent(SDL_Event *);
int SDL_WaitEventTimeout(SDL_Event *,int);
int SDL_PushEvent(SDL_Event *);
int SDL_HasEvent(Uint32);
void SDL_PumpEvents(void);
void SDL_Delay(Uint32);
Uint32 SDL_RegisterEvents(int);
SDL_RWops *SDL_RWFromFile(const char *,const char *);
Sint64 SDL_RWsize(SDL_RWops *);
size_t SDL_RWread(SDL_RWops *,void *,size_t,size_t);
size_t SDL_RWwrite(SDL_RWops *,const void *,size_t,size_t);
int SDL_RWclose(SDL_RWops *);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *,int,const SDL_AudioSpec *,SDL_AudioSpec *,int);
void SDL_CloseAudioDevice(SDL_AudioDeviceID);
void SDL_PauseAudioDevice(SDL_AudioDeviceID,int);
void SDL_LockAudioDevice(SDL_AudioDeviceID);
void SDL_UnlockAudioDevice(SDL_AudioDeviceID);
int SDL_GetAudioDeviceStatus(SDL_AudioDeviceID);
SDL_sem *SDL_CreateSemaphore(Uint32);
void SDL_DestroySemaphore(SDL_sem *);
int SDL_SemPost(SDL_sem *);
int SDL_SemTryWait(SDL_sem *);
int SDL_SemWaitTimeout(SDL_sem *,Uint32);
#endif
