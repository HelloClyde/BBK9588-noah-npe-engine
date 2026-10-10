#include "../sound_sdl.h"
#ifdef main
#undef main
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
/* Use the exact callback registered by the production SDL player. */
#include "../sound_sdl.c"
typedef struct {
    NpeSoundPlayer *player;NpeWorld *world;unsigned requests,animations;unsigned sections[8];
    uint32_t prepared_state;unsigned variance,attribute;
} Trace;
static void request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    Trace *t=context;assert(t->requests<8 && !callback);
    assert(t->world->random_state==t->prepared_state);
    assert(t->world->battle_variance_roll==t->variance && t->world->battle_attribute_roll==t->attribute);
    assert(!npe_sound_play_range(t->player,offset,length));
    t->sections[t->requests++]=(unsigned)t->player->selected_section;
}
static void animation(void *context,const NpeWorld *w,unsigned action,unsigned target,unsigned phase){
    (void)target;Trace *t=context;assert(w==t->world);
    if(phase!=NPE_BATTLE_ANIMATION_BEGIN)return;
    if(action==NPE_BATTLE_ANIMATION_ATTACK){assert(t->requests==1 && t->sections[0]==36);++t->animations;}
}
static void callback_frame(NpeSoundPlayer *p,uint8_t bytes[1280]){
    uint8_t silent[12800];assert(p->wait_bytes<=sizeof(silent));
    while(p->wait_bytes){size_t n=p->wait_bytes;assert(n<=sizeof(silent));sound_callback(p,silent,(int)n);}
    sound_callback(p,bytes,1280);assert(!p->decode_failed && p->cursor==320);
}
static void setup(const NpeFile *f,NpeWorld *w){
    assert(!npe_world_init(f,w,9,29,13));assert(!npe_world_resources_open((NpeFile *)f,w));
    w->game_mode=3;w->story_event_id=1;
    const uint8_t zero[2]={0,0};assert(!npe_world_resident_write(w,0x527626,zero,2));
    assert(npe_world_prepare_scripted_battle(f,w)==1);
    w->hp[0]=w->max_hp[0]=900;w->other_stats[0][1]=999;w->attack[0]=12;
    w->battle_enemy_hps[0]=w->battle_enemy_max_hps[0]=900;
    w->battle_enemy_speeds[0]=1;w->battle_enemy_attacks[0]=1;
    memset(w->battle_enemy_skill_ids,0,sizeof(w->battle_enemy_skill_ids));
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;battle_sound_file(argv[1],&f);
    NpeSoundPlayer p;npe_sound_open(&p,&f);assert(p.device);SDL_PauseAudioDevice(p.device,1);
    uint8_t section_pcm[1280],range_pcm[1280];
    const unsigned sections[]={30,31,32,33,35,36};
    for(unsigned i=0;i<sizeof(sections)/sizeof(*sections);++i){
        unsigned section=sections[i];NpeAudioResource resource;
        assert(!npe_audio_resource(&f,section,&resource));
        assert(!npe_sound_play_section(&p,section,0));callback_frame(&p,section_pcm);
        assert(!npe_sound_play_range(&p,f.sections[section].offset,(int32_t)f.sections[section].size));
        assert(p.active==NPE_SOUND_TRACKS && p.selected_section==(int)section && !p.loop);
        assert(p.resources[p.active].bytes==resource.bytes);callback_frame(&p,range_pcm);
        assert(!memcmp(section_pcm,range_pcm,sizeof(range_pcm)));
    }
    /* A valid compressed range need not appear in the NPE section directory. */
    NpeFile relocated=f;size_t count=f.sections[36].size;
    relocated.bytes=malloc(f.size+count);assert(relocated.bytes);
    memcpy(relocated.bytes,f.bytes,f.size);memcpy(relocated.bytes+f.size,f.bytes+f.sections[36].offset,count);
    relocated.size=f.size+count;p.file=&relocated;
    assert(!npe_sound_play_range(&p,(uint32_t)f.size,(int32_t)count));assert(p.selected_section==-1);
    callback_frame(&p,range_pcm);p.file=&f;free(relocated.bytes);
    assert(npe_sound_play_range(&p,f.sections[36].offset,-1)==-1 && p.active==-1);
    assert(npe_sound_play_range(&p,UINT32_MAX,508)==-1 && p.active==-1);
    assert(npe_sound_play_range(&p,f.sections[36].offset,507)==-1 && p.active==-1);
    NpeWorld w;setup(&f,&w);
    npe_sound_select(&p,&w);assert(w.sound_range_observer);
    w.battle_execution_slot=1;assert(npe_world_battle_sound(&w)==1);assert(p.selected_section==36);
    /* Mutable physical descriptors select another current-NPE sound. */
    uint8_t descriptor[8];uint32_t offset=f.sections[33].offset,size=f.sections[33].size;
    for(unsigned i=0;i<4;++i){descriptor[i]=(uint8_t)(offset>>((3-i)*8));descriptor[4+i]=(uint8_t)(size>>((3-i)*8));}
    assert(!npe_world_record_write(&w,1,0,114,descriptor,8));
    assert(npe_world_battle_sound(&w)==1 && p.selected_section==33);
    offset=f.sections[36].offset;size=f.sections[36].size;
    for(unsigned i=0;i<4;++i){descriptor[i]=(uint8_t)(offset>>((3-i)*8));descriptor[4+i]=(uint8_t)(size>>((3-i)*8));}
    assert(!npe_world_record_write(&w,1,0,114,descriptor,8));
    uint32_t first=w.random_state*UINT32_C(0x514f111f)+1;
    uint32_t second=first*UINT32_C(0x514f111f)+1;
    Trace trace={&p,&w,0,0,{0},second,((first>>16)&0x7fff)%6,((second>>16)&0x7fff)%100};
    w.sound_context=&trace;w.sound_observer=NULL;w.sound_range_observer=request;
    w.battle_animation_observer=animation;w.battle_animation_context=&trace;
    assert(npe_world_battle_attack(&w,0)==NPE_BATTLE_CONTINUE);
    assert(trace.requests==1 && trace.animations==1);
    w.music_enabled=0x101;unsigned requests=trace.requests;
    assert(npe_world_battle_attack(&w,0)==NPE_BATTLE_CONTINUE);assert(trace.requests==requests);
    npe_sound_close(&p);npe_free(&f);SDL_Quit();
    puts("Current-NPE ranges/relocated range, six candidate PCM prefixes, live descriptors and post-preparation RNG/pre-animation request passed; no original PCM claim");
    return 0;
}
