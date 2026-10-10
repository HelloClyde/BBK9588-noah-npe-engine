/* Compare prior World ending behavior with the saved original controls. */
#include "../npe.h"
#include "ending_return_native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned stops;
static void observe(void *p,unsigned section,int loop){(void)p;(void)loop;if(!section)++stops;}
int main(int argc,char **argv){
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0);rewind(in);
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile f;char error[128];assert(!npe_parse(&f,bytes,(size_t)size,error,sizeof(error)));
    unsigned failed=0;
    for(unsigned i=0;i<sizeof(ending_cases)/sizeof(*ending_cases);++i){
        if(ending_cases[i].load)continue;
        NpeWorld w;assert(!npe_world_new_game(&f,&w)&&!npe_world_resources_open(&f,&w));
        w.game_mode=21;w.story_event_id=0xbeef;w.map_init_pending=0;w.music_enabled=ending_cases[i].music;
        w.sound_observer=observe;w.sound_context=NULL;stops=0;
        assert(npe_world_finish_game(&w)==1);uint8_t map[2],bits[2];
        assert(!npe_rpg_runtime_read(&f,0x527a6a,map,2));unsigned h=(unsigned)map[0]*256+map[1];
        assert(!npe_rpg_runtime_read(&f,NPE_RESIDENT_HEAP_BITMAP+2u*(h/16),bits,2));
        unsigned allocated=!!(((unsigned)bits[0]*256+bits[1])&(0x8000u>>(h%16)));
        unsigned differences=(w.game_mode!=7)+(w.story_event_id!=0xbeef)+(w.map_init_pending!=1)+allocated+(stops!=0);
        printf("Before ending music%u differences%u init%u mapAllocated%u stops%u\n",w.music_enabled,differences,w.map_init_pending,allocated,stops);
        failed+=differences!=0;npe_rpg_runtime_close(&f);
    }
    assert(failed==2);npe_free(&f);puts("Before fix:two of two original ending control paths differ");return 0;
}
