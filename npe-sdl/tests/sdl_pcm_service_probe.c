#define SDL_MAIN_HANDLED
#include "../sound_sdl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../sound_sdl.c"

static void silence(NpeSoundPlayer *player,size_t bytes){
    uint8_t output[511];
    while(bytes){size_t n=bytes<sizeof(output)?bytes:sizeof(output);
        sound_callback(player,output,(int)n);for(size_t i=0;i<n;++i)assert(!output[i]);bytes-=n;}
}
static void start(NpeSoundPlayer *player,unsigned section){
    assert(!npe_sound_play_section(player,section,0));
    if(player->phase==NPE_SOUND_CLEANUP_WAIT)silence(player,player->wait_bytes);
    assert(player->phase==NPE_SOUND_START_WAIT);silence(player,player->wait_bytes);
    assert(player->phase==NPE_SOUND_PLAYING&&!player->cursor&&npe_audio_clock_ms(&player->clock)==20);
    assert(!player->pcm_service.flags8e&&!player->pcm_service.time_snapshot&&
           player->pcm_service.reload_words[0]==320&&player->pcm_service.reload_words[1]==320&&
           player->pcm_halves_pending==3u);
}
int main(int argc,char **argv){
    SDL_SetMainReady();
    assert(argc==3);FILE *input=fopen(argv[1],"rb");assert(input);
    assert(!fseek(input,0,SEEK_END));long size=ftell(input);assert(size>0);
    rewind(input);uint8_t *bytes=malloc((size_t)size);assert(bytes);
    assert(fread(bytes,1,(size_t)size,input)==(size_t)size);fclose(input);
    NpeFile file;char error[160];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    NpeSoundPlayer player;npe_sound_open(&player,&file);assert(player.device);
    SDL_PauseAudioDevice(player.device,1);unsigned total=0,restored=0;
    for(unsigned section=16;section<=37;++section){
        start(&player,section);char path[1024];
        assert(snprintf(path,sizeof(path),"%s/section-%u.pcm",argv[2],section)>0);
        FILE *reference=fopen(path,"rb");assert(reference);
        unsigned count=player.resources[player.active].header_count;
        for(unsigned frame=0;frame<count;++frame){
            NpeSoundPlayer whole=player,split=player;
            uint8_t joined[1280],parts[1280];sound_callback(&whole,joined,sizeof(joined));
            sound_callback(&split,parts,1);
            assert(split.pcm_service.time_snapshot==frame*20&&
                   npe_audio_clock_ms(&split.clock)==(frame?frame*20+10:20));
            assert(!(split.pcm_service.flags8e&0xc000)&&split.pcm_service.reload_words[0]==320);
            if(split.decoder.input.flags89&0x20){
                assert(!split.decoder.input.word_cursor);++restored;
            }
            sound_callback(&split,parts+1,95);sound_callback(&split,parts+96,543);
            sound_callback(&split,parts+639,641);
            assert(!memcmp(joined,parts,sizeof(joined)));
            assert(!memcmp(&whole.decoder,&split.decoder,sizeof(whole.decoder)));
            assert(whole.clock.value==split.clock.value&&whole.clock.increment==split.clock.increment);
            assert(whole.pcm_service.time_snapshot==(frame?frame*20+10:0)&&
                   whole.pcm_service.time_snapshot==split.pcm_service.time_snapshot);
            assert(whole.pcm_service.cursor.address==split.pcm_service.cursor.address&&
                   whole.pcm_service.cursor.remainder==split.pcm_service.cursor.remainder);
            assert(whole.pcm_service.reload_words[0]==320&&whole.pcm_service.reload_words[1]==320);
            assert(whole.pcm_service.flags8e==split.pcm_service.flags8e&&!whole.pcm_service.flags8e);
            assert(whole.cursor==(frame+1u)*320u&&whole.decoder.next_frame==frame+1u&&!whole.decode_failed);
            int16_t pcm[160];
            for(unsigned i=0;i<160;++i){int low=fgetc(reference),high=fgetc(reference);
                assert(low!=EOF&&high!=EOF);pcm[i]=(int16_t)(uint16_t)(low|high<<8);}
            if(memcmp(pcm,whole.frame,sizeof(pcm)))fprintf(stderr,"section%u candidate PCM frame%u differs\n",section,frame);
            assert(!memcmp(pcm,whole.frame,sizeof(pcm)));player=whole;
        }
        assert(fgetc(reference)==EOF&&!ferror(reference));fclose(reference);
        assert(player.finished&&player.phase==NPE_SOUND_END_WAIT&&player.wait_bytes==3200);
        uint64_t time=player.clock.value;silence(&player,3200);
        assert(player.phase==NPE_SOUND_STOPPED&&player.driver_configured&&player.clock.value==time);
        total+=count;printf("section%u %u complete SDL candidate frames and split half-service states passed\n",section,count);
    }
    assert(total==10486&&restored>0);
    printf("SDL PCM service:22 current-NPE tracks/%u frames/%u refill-half observations; preserved corrected candidate PCM, split callbacks, snapshot/acknowledgement, finite EOF and restart passed; not original mask-ROM PCM\n",total,restored);
    npe_sound_close(&player);npe_free(&file);SDL_Quit();return 0;
}
