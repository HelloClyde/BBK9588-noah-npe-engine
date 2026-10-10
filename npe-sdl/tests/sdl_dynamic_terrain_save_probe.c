#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
static NpeWorld *live;
static const NpeWorld *expected;
static uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
static unsigned submits;
static int probe_wait(SDL_Event *,int);
static void probe_present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/dynamic-terrain-save-sdl-");}
static Uint32 probe_ticks(void){return 0;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetPrefPath probe_pref
#define SDL_GetTicks probe_ticks
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout probe_wait
#define SDL_RenderPresent probe_present
#define npe_sound_select probe_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetPrefPath
#undef SDL_GetTicks
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef npe_sound_select
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){
    /* Music is a session setting. Isolate this terrain startup check from
       playback waits; it does not test saved music or audio timing. */
    live=w;w->music_enabled=0;w->exit_choice=1;npe_sound_select(p,w);
}
static void probe_present(SDL_Renderer *r){
    SDL_RenderPresent(r);assert(live&&live->game_mode==1&&live->map_number==expected->map_number&&!submits);
    const NpeFile *f=live->battle_file;
    assert(npe_rpg_runtime_owner(f)==live&&npe_rpg_runtime_map_current(f,expected->map_number)==1);
    uint8_t records[192];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!npe_rpg_runtime_read(f,0x52724e,records,192)&&!memcmp(records,expected->tile_record_bytes,192));
    unsigned count=expected->terrain_snapshot_count;
    if(expected->terrain_buffer_snapshot_ready && count<NPE_MAP_BUFFER_WORDS)count=NPE_MAP_BUFFER_WORDS;
    for(unsigned i=0;i<count;++i){uint16_t tile;
        assert(npe_rpg_runtime_map_tile(f,expected->map_number,(uint16_t)i,&tile)==1&&tile==expected->terrain_snapshot_cells[i]);
    }
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(pixels);++i){unsigned v=pixels[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));
    }
    printf("own cold SDL --continue:192 record bytes,%u grid words,C pixels=%u,SDL pixels=%u\n",count,d,sd);
    assert(!d&&!sd);++submits;
}
static int probe_wait(SDL_Event *event,int timeout){
    (void)timeout;assert(submits==1);memset(event,0,sizeof(*event));event->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){
    assert(argc==3);NpeFile f;uint8_t *bytes,*slot;size_t length,slot_length;char error[128],pixel_path[512];
    NpeWorld *saved=calloc(1,sizeof(*saved));assert(saved);
    assert(!read_file(argv[1],&bytes,&length)&&!npe_parse(&f,bytes,length,error,sizeof(error)));
    assert(!npe_world_load(&f,saved,argv[2])&&saved->terrain_snapshot_ready);expected=saved;
    assert(!read_file(argv[2],&slot,&slot_length));
    char *path=game_save_path(&f,0);assert(path);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(slot,1,slot_length,out)==slot_length&&!fclose(out));free(slot);
    snprintf(pixel_path,sizeof(pixel_path),"%s.pixels",argv[2]);
    FILE *in=fopen(pixel_path,"rb");assert(in&&fread(pixels,1,sizeof(pixels),in)==sizeof(pixels)&&!fclose(in));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    char *args[]={"npe-sdl","--continue",argv[1],NULL};assert(!run(3,args)&&submits==1);
    assert(!remove(path));free(path);npe_free(&f);free(saved);
    puts("actual main cold own-save startup,resource attachment,live heap and first SDL submission passed;reference is own pre-save frame");
    return 0;
}
