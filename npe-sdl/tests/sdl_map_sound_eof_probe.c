#include "../sound_sdl.h"
#ifdef main
#undef main
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
static unsigned ranges;
static int observed_range(const NpeFile *f,uint32_t offset,int32_t size,NpeAudioResource *r){
    ++ranges;return npe_audio_resource_range(f,offset,size,r);
}
#define npe_audio_resource_range observed_range
#include "../sound_sdl.c"
#undef npe_audio_resource_range
#define main npe_app_main
#include "../main.c"
#undef main
static void pair(NpeFile *f,unsigned slot,uint32_t offset,uint32_t size){
    uint8_t bytes[4];for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(size>>((3-i)*8));
    assert(!npe_rpg_runtime_write(f,0x5278f2+4*slot,bytes,4));
    for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(offset>>((3-i)*8));
    assert(!npe_rpg_runtime_write(f,0x5278feu+4*slot,bytes,4));
}
static void deliver(NpeSoundPlayer *p,size_t count){
    uint8_t bytes[12800];while(count){size_t n=count<sizeof(bytes)?count:sizeof(bytes);sound_callback(p,bytes,(int)n);count-=n;}
}
static void begin_short(NpeFile *f,NpeWorld *w,NpeSoundPlayer *p,uint32_t short_offset){
    SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);pair(f,0,short_offset,26);w->map_music_slot=0;w->music_enabled=1;
    npe_world_sound_map_resume(w);assert(p->continuation==SOUND_CONTINUATION_MAP && !p->end_pending);
    while(p->wait_bytes)deliver(p,p->wait_bytes);
    assert(p->phase==NPE_SOUND_PLAYING);deliver(p,1280);
    assert(p->phase==NPE_SOUND_END_WAIT && p->wait_bytes==3200 && !p->end_pending);
    deliver(p,3199);assert(p->wait_bytes==1 && !p->end_pending);
}
static void end_short(NpeSoundPlayer *p){
    deliver(p,1);assert(p->end_pending==NPE_SOUND_CALLBACK_MAP && p->phase==NPE_SOUND_STOPPED && p->finished && p->driver_configured);
}
static void key(void){SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RIGHT;assert(SDL_PushEvent(&e)==1);}
static int drop_end(void *context,SDL_Event *e){return e->type!=*(const Uint32 *)context;}
int main(int argc,char **argv){
    assert(argc==2 && !SDL_Init(SDL_INIT_EVENTS));NpeFile f;battle_sound_file(argv[1],&f);
    uint32_t short_offset=(uint32_t)f.size;uint8_t *grown=realloc(f.bytes,f.size+26);assert(grown);f.bytes=grown;
    memcpy(f.bytes+f.size,f.bytes+f.sections[20].offset,25);f.bytes[f.size+2]=0;f.bytes[f.size+3]=1;f.bytes[f.size+25]=0;f.size+=26;
    NpeWorld w;assert(!npe_world_init(&f,&w,9,29,13));assert(!npe_world_resources_open(&f,&w));
    NpeSoundPlayer p;npe_sound_open(&p,&f);assert(p.device);SDL_PauseAudioDevice(p.device,1);
    npe_sound_select(&p,&w);game_sound_player=&p;
    const uint16_t words[]={0,1,2,0x100,0x101,0xffff};
    for(unsigned route=0;route<6;++route){
        begin_short(&f,&w,&p,short_offset);w.map_music_slot=2;w.music_enabled=words[route];
        pair(&f,2,f.sections[21].offset,f.sections[21].size);unsigned before=ranges;uint32_t random=w.random_state;
        end_short(&p);assert(ranges==before);SDL_Event e;
        if(route==0)assert(npe_sound_pump(&p)==1);
        if(route==1)npe_sound_select(&p,&w);
        if(route==2){key();assert(game_WaitEvent(&e)==1 && e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_RIGHT);}
        if(route==3){key();assert(game_WaitEventTimeout(&e,100)==1 && e.type==SDL_KEYDOWN);}
        if(route==4){key();assert(game_PollEvent(&e)==1 && e.type==SDL_KEYDOWN);}
        if(route==5)game_Delay(1);
        assert(ranges==before+1 && p.selected_section==21 && p.continuation==SOUND_CONTINUATION_MAP);
        assert(!p.end_pending && p.phase==NPE_SOUND_CLEANUP_WAIT && p.wait_bytes==3200 && !p.cursor);
        assert(w.music_enabled==words[route] && w.random_state==random);
        /* Late notifications cannot deliver this same callback twice. */
        while(game_PollEvent(&e))assert(e.type!=SDL_KEYDOWN);
        assert(ranges==before+1 && !npe_sound_pump(&p));
    }
    begin_short(&f,&w,&p,short_offset);end_short(&p);unsigned before=ranges;
    npe_world_sound_stop(&w);SDL_Event e;while(game_PollEvent(&e)){}
    assert(ranges==before && !p.end_pending && p.active==-1 && !p.loop);
    begin_short(&f,&w,&p,short_offset);end_short(&p);before=ranges;
    assert(!npe_sound_play_range(&p,f.sections[36].offset,(int32_t)f.sections[36].size));
    while(game_PollEvent(&e)){}assert(ranges==before+1 && p.selected_section==36 && !p.loop && !p.end_pending);
    begin_short(&f,&w,&p,short_offset);w.map_music_slot=1;pair(&f,1,UINT32_MAX,26);end_short(&p);
    assert(npe_sound_pump(&p)==1 && p.active==-1 && !p.end_pending && !p.loop);
    begin_short(&f,&w,&p,short_offset);end_short(&p);NpeWorld other=w;before=ranges;
    npe_sound_select(&p,&other);while(game_PollEvent(&e)){}assert(ranges==before && !p.end_pending && !p.loop);
    npe_sound_select(&p,&w);
    /* Use the actual SDL audio thread while the UI waits without input.
       Dropped notifications are recovered by the bounded game-thread pump. */
    for(unsigned filtered=0;filtered<2;++filtered){
        SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);pair(&f,0,short_offset,26);w.map_music_slot=0;w.music_enabled=1;
        npe_world_sound_map_resume(&w);w.map_music_slot=2;w.music_enabled=0;
        pair(&f,2,f.sections[21].offset,f.sections[21].size);before=ranges;
        if(filtered)SDL_SetEventFilter(drop_end,&p.end_event);
        SDL_PauseAudioDevice(p.device,0);assert(!game_WaitEventTimeout(&e,450));SDL_PauseAudioDevice(p.device,1);
        SDL_SetEventFilter(NULL,NULL);assert(ranges==before+1 && p.selected_section==21 && !p.end_pending);
    }
    begin_short(&f,&w,&p,short_offset);end_short(&p);Uint32 old_event=p.end_event;
    game_sound_player=NULL;npe_sound_close(&p);SDL_zero(e);e.type=old_event;e.user.data1=&p;
    assert(!npe_sound_handle_event(&p,&e));npe_free(&f);SDL_Quit();
    puts("SDL map EOF:6 game-thread dispatch routes/full music words;byte boundary/live cache/no duplicate,stop/finite/invalid/rebind/close;2 actual audio-thread idle-UI handoffs including dropped wake passed");
    return 0;
}
