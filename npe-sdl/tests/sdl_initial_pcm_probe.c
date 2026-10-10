#define SDL_MAIN_HANDLED
#include "../sound_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../sound_sdl.c"

static void read_fixture(const char *directory,const char *name,const char *suffix,void *data,size_t size){
    char path[1024];assert(snprintf(path,sizeof(path),"%s/%s%s",directory,name,suffix)>0);
    FILE *stream=fopen(path,"rb");assert(stream);
    assert(fread(data,1,size,stream)==size&&fgetc(stream)==EOF&&!fclose(stream));
}
static void drain(NpeSoundPlayer *player){
    while(player->wait_bytes){
        uint8_t quiet[511];size_t count=player->wait_bytes<sizeof(quiet)?player->wait_bytes:sizeof(quiet);
        sound_callback(player,quiet,(int)count);
        for(size_t i=0;i<count;++i)assert(!quiet[i]);
    }
}
int main(int argc,char **argv){
    SDL_SetMainReady();assert(argc==3);
    FILE *stream=fopen(argv[1],"rb");assert(stream);
    assert(!fseek(stream,0,SEEK_END));long size=ftell(stream);assert(size>0);rewind(stream);
    uint8_t *bytes=malloc((size_t)size);assert(bytes);
    assert(fread(bytes,1,(size_t)size,stream)==(size_t)size&&!fclose(stream));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(0));
    NpeSoundPlayer player;npe_sound_open(&player,&file);assert(player.device);
    SDL_PauseAudioDevice(player.device,1);
    const char *names[]={"steady","up","down","mute"};
    const uint8_t levels[4][2]={{7,7},{0,7},{7,0},{0,0}};
    unsigned failed=0;
    for(unsigned case_index=0;case_index<4;++case_index){
        (void)npe_sound_play_section(&player,0,0);drain(&player);
        player.gain=(NpeAudioGain){levels[case_index][0],levels[case_index][1]};
        player.decoder.input.flags8a&=(uint16_t)~0x80u;
        assert(!npe_sound_play_section(&player,20,0));
        uint16_t wanted[12],actual[12]={
            (uint16_t)(player.gain.current<<8|player.gain.target),
            (uint16_t)(player.decoder.input.flags8a&0x80),
            (uint16_t)(player.clock.value>>32),(uint16_t)(player.clock.value>>16),(uint16_t)player.clock.value,
            (uint16_t)(player.clock.increment>>16),(uint16_t)player.clock.increment,
            player.output.half_index,player.output.half_samples,player.output.output_words,
            player.pcm_service.reload_words[0],player.pcm_service.reload_words[1]};
        read_fixture(argv[2],names[case_index],".state",wanted,sizeof(wanted));wanted[1]&=0x80;
        unsigned state_bad=0;for(unsigned i=0;i<12;++i)state_bad+=wanted[i]!=actual[i];
        state_bad+=player.cursor!=0;
        state_bad+=player.wait_bytes!=((wanted[1]&0x80)?0u:12800u);
        drain(&player);assert(player.phase==NPE_SOUND_PLAYING);
        uint8_t expected[1280],whole[1280],split[1280];
        read_fixture(argv[2],names[case_index],"-half0.pcm",expected,640);
        read_fixture(argv[2],names[case_index],"-half1.pcm",expected+640,640);
        NpeSoundPlayer first=player,second=player;
        sound_callback(&first,whole,sizeof(whole));
        sound_callback(&second,split,1);sound_callback(&second,split+1,638);
        sound_callback(&second,split+639,2);sound_callback(&second,split+641,639);
        unsigned pcm_bad=0;for(unsigned i=0;i<sizeof(expected);++i)pcm_bad+=whole[i]!=expected[i];
        unsigned split_bad=memcmp(whole,split,sizeof(whole))!=0||
            memcmp(&first.decoder,&second.decoder,sizeof(first.decoder))!=0||
            first.clock.value!=second.clock.value||first.gain.current!=second.gain.current;
        fprintf(stderr,"initial %s: state=%u PCM=%u splits=%u\n",names[case_index],state_bad,pcm_bad,split_bad);
        failed+=(state_bad||pcm_bad||split_bad);player=first;
    }
    printf("Original initial PCM:4 gain states, two retained halves and split callback outputs; %u failed\n",failed);
    npe_sound_close(&player);npe_free(&file);SDL_Quit();return failed?1:0;
}
