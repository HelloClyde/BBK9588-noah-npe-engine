#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[8],after[8];} RandomEnemyLoadCase;
#include "random_enemy_resident_load_native.h"
static void compare(const NpeWorld *w,const uint8_t expected[8]){
    uint8_t actual[8];assert(!npe_world_resident_read(w,0x527916,actual,8)&&!memcmp(actual,expected,8));
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<16;++i){const RandomEnemyLoadCase *c=&random_enemy_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map)&&!npe_world_resident_write(&w,0x527916,c->before,8));
        assert(!npe_world_load_random_enemy_records(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        const uint8_t cross[]={0x12,0x34,0x56,0x78};uint8_t actual[4];
        assert(!npe_rpg_runtime_write(&f,0x527915,cross,4)&&!npe_rpg_runtime_read(&f,0x527915,actual,4)&&!memcmp(actual,cross,4));
        assert(w.random_enemy_records[0]==0x34&&w.random_enemy_records[1]==0x56&&w.random_enemy_records[2]==0x78);
        NpeWorld prior=w;assert(npe_world_resident_write(&w,0x52791d,cross,2)&&!memcmp(&w,&prior,sizeof(w)));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Random enemy load%u %s map%u:8 original bytes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/random-enemy-own-save.dat",*changed="tmp/random-enemy-cache-save.dat";
    assert(!npe_world_save(&f,&w,save));assert(!npe_world_resident_write(&w,0x527916,random_enemy_load_cases[0].before,8));
    assert(!npe_world_save(&f,&w,changed));uint8_t *a,*b;size_t as,bs;
    assert(!read_file(save,&a,&as)&&!read_file(changed,&b,&bs)&&as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,random_enemy_load_cases[0].after);
    assert(!npe_world_resident_write(&w,0x527916,random_enemy_load_cases[0].before,8));
    assert(!npe_world_restart_game(&f,&w));compare(&w,random_enemy_load_cases[14].after);
    assert(!remove(save)&&!remove(changed));
    unsigned bound=0;uint8_t byte;for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Random enemy resident:16 original loads/128 bytes,3748 World bytes,cross-skill writes,warm restore/restart,unchanged own save passed");return 0;
}
