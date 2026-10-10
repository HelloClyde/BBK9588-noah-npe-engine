#include "../sound_sdl.h"
#ifdef main
#undef main
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "battle_sound_file.h"
#include "../sound_sdl.c"
static void cache_pair(NpeFile *f,unsigned slot,unsigned section){
    uint8_t bytes[4];uint32_t size=f->sections[section].size,offset=f->sections[section].offset;
    for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(size>>((3-i)*8));
    assert(!npe_rpg_runtime_write(f,0x5278f2+4*slot,bytes,4));
    for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(offset>>((3-i)*8));
    assert(!npe_rpg_runtime_write(f,0x5278feu+4*slot,bytes,4));
}
static void prefix(NpeSoundPlayer *p,uint8_t bytes[1280]){
    uint8_t wait[12800];while(p->wait_bytes){assert(p->wait_bytes<=sizeof(wait));sound_callback(p,wait,(int)p->wait_bytes);}
    sound_callback(p,bytes,1280);assert(!p->decode_failed && p->cursor==320);
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;battle_sound_file(argv[1],&f);NpeWorld w;
    assert(!npe_world_init(&f,&w,9,29,13));assert(!npe_world_resources_open(&f,&w));
    NpeSoundPlayer p;npe_sound_open(&p,&f);assert(p.device);SDL_PauseAudioDevice(p.device,1);
    npe_sound_select(&p,&w);uint8_t section[1280],range[1280];
    for(unsigned slot=0;slot<3;++slot){
        assert(!npe_sound_play_section(&p,20+slot,1));prefix(&p,section);
        w.map_music_slot=(uint8_t)slot;w.music_enabled=1;npe_world_sound_map_resume(&w);
        assert(p.active==NPE_SOUND_TRACKS && p.loop==1 && p.selected_section==(int)(20+slot));
        prefix(&p,range);assert(!memcmp(section,range,sizeof(range)));
    }
    /* Change the cached offsets/lengths without changing the section directory. */
    cache_pair(&f,1,22);w.map_music_slot=1;npe_world_sound_map_resume(&w);
    assert(p.selected_section==22 && p.loop==1);
    uint8_t slot=2;assert(!npe_rpg_runtime_write(&f,0x527a04,&slot,1));
    cache_pair(&f,2,20);npe_world_sound_map_resume(&w);assert(p.selected_section==20);
    const uint16_t disabled[]={0,2,0x100,0x101,0xffff};
    for(unsigned i=0;i<sizeof(disabled)/sizeof(*disabled);++i){
        w.music_enabled=disabled[i];p.cursor=123;npe_world_sound_map_resume(&w);assert(p.cursor==123);
        npe_world_music_set(&w,1);assert(w.music_enabled==1 && p.cursor==0 && p.selected_section==20 && p.loop);
        npe_world_music_set(&w,0);assert(!w.music_enabled && p.active==-1 && !p.loop);
    }
    /* Original raw slot255 reads its cached RAM aliases. Invalid bytes stop
       candidate playback; they do not select another map resource. */
    cache_pair(&f,255,21);w.map_music_slot=255;w.music_enabled=1;npe_world_sound_map_resume(&w);
    assert(p.selected_section==21 && p.loop);
    const uint8_t invalid[4]={0xff,0xff,0xff,0xff};
    assert(!npe_rpg_runtime_write(&f,0x5278feu+4*255,invalid,4));
    npe_world_sound_map_resume(&w);assert(p.active==-1);
    npe_sound_close(&p);npe_free(&f);SDL_Quit();
    puts("SDL live map cache/slot and full-word gate,3 candidate PCM prefixes,pre-store on/off and raw slot255 passed;no original PCM or EOF callback claim");
    return 0;
}
