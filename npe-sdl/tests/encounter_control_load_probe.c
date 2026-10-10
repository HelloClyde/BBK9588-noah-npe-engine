#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[2],after[2];} EncounterControlLoadCase;
#include "encounter_control_load_native.h"
static void compare(const NpeWorld *w,const uint8_t expected[2]){
    uint8_t raw[2];assert(!npe_world_resident_read(w,0x527a5c,raw,2)&&!memcmp(raw,expected,2));
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<16;++i){const EncounterControlLoadCase *c=&control_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map)&&!npe_world_resident_write(&w,0x527a5c,c->before,2));
        assert(!npe_world_load_random_enemy_records(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        const uint8_t word[]={0x12,0x34},low[]={0xfe};uint8_t actual[2];
        for(unsigned j=0;j<2;++j){uint32_t address=j?0x527a5c:0x527a02;
            assert(!npe_rpg_runtime_write(&f,address,word,2)&&!npe_rpg_runtime_write(&f,address+1,low,1));
            assert(!npe_rpg_runtime_read(&f,address,actual,2)&&actual[0]==0x12&&actual[1]==0xfe);
            NpeWorld before=w;assert(npe_world_resident_write(&w,address+1,word,2)&&!memcmp(&w,&before,sizeof(w)));}
        assert(w.encounter_steps==0x12fe&&w.encounters_enabled==0x12fe);
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Encounter control load%u %s map%u:2 original bytes match\n",i,c->entry,c->map);
    }
    NpeWorld w,loaded;assert(!npe_world_new_game(&f,&w));
    const unsigned counters[]={0,999,1000,1001,32767,32768,65534,65535};
    const char *path="tmp/encounter-control-own-save.dat",*flag="tmp/encounter-control-cache-save.dat";
    for(unsigned i=0;i<8;++i){uint8_t raw[]={(uint8_t)(counters[i]>>8),(uint8_t)counters[i]};
        assert(!npe_world_resident_write(&w,0x527a02,raw,2)&&!npe_world_save(&f,&w,path));
        assert(!npe_world_load(&f,&loaded,path)&&loaded.encounter_steps==counters[i]);
        assert(!npe_world_restore(&f,&w,path)&&w.encounter_steps==counters[i]);compare(&w,control_load_cases[0].after);}
    assert(!npe_world_save(&f,&w,path));w.encounters_enabled=0xcdef;assert(!npe_world_save(&f,&w,flag));
    uint8_t *a,*b;size_t as,bs;assert(!read_file(path,&a,&as)&&!read_file(flag,&b,&bs)&&as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restart_game(&f,&w));compare(&w,control_load_cases[14].after);assert(!w.encounter_steps);
    assert(!remove(path)&&!remove(flag));
    unsigned bound=0;uint8_t byte;for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Encounter control:16 original loads/32 bytes,3748 World bytes,partial writes,8 full-word save/restore counters,NPESAV22 unchanged passed");return 0;
}
