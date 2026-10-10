#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[288],after[288];} ExitLoadCase;
#include "exit_record_load_native.h"
static void compare(const NpeWorld *w,const uint8_t expected[288]){
    uint8_t actual[288];assert(!npe_world_resident_read(w,0x526b82,actual,288));
    for(unsigned i=0;i<288;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"exit byte%u C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<sizeof(exit_load_cases)/sizeof(*exit_load_cases);++i){
        const ExitLoadCase *c=&exit_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));
        assert(!npe_world_resident_write(&w,0x526b82,c->before,288));
        assert(!npe_world_load_exit_records(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        uint8_t cross[]={0x12,0x34,0x56,0x78},actual[4];
        /* Map-character/exit, exit slots, exit/NPC and NPC/catalog adjacency. */
        const uint32_t addresses[]={0x526b81,0x526b99,0x526ca1,0x526db9};
        for(unsigned j=0;j<4;++j){
            assert(!npe_rpg_runtime_write(&f,addresses[j],cross,4));
            assert(!npe_rpg_runtime_read(&f,addresses[j],actual,4)&&!memcmp(cross,actual,4));
        }
        assert(w.exit_records[0][0]==0x34&&w.npc_records[0][0]==0x34&&w.npc_records[0][1]==0x56);
        assert(w.shop_catalog_present[0]==0x34&&w.shop_catalog_present[1]==0x56);
        NpeWorld before=w;assert(npe_world_resident_write(&w,0x526eb8,cross,2));
        assert(!memcmp(&w,&before,sizeof(w)));
        NpeMapExit exit;w.exit_records[11][12]=0x23;w.exit_records[11][13]=0xcd;
        uint8_t high=0x81;assert(!npe_rpg_runtime_write(&f,0x526c96,&high,1));
        assert(!npe_world_exit(&w,&map,11,&exit)&&exit.next_id==0x81cd);
        assert(npe_world_exit(&w,&map,12,&exit)==-1&&!npe_rpg_runtime_failures(&f));
        npe_rpg_runtime_close(&f);
        fprintf(stderr,"Exit load%u %s map%u:288 original bytes and shared writes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/exit-record-self-save.dat",*changed="tmp/exit-record-cache-save.dat";
    assert(!npe_world_save(&f,&w,save));assert(!npe_world_resident_write(&w,0x526b82,exit_load_cases[0].before,288));
    assert(!npe_world_save(&f,&w,changed));uint8_t *a,*b;size_t as,bs;
    assert(!read_file(save,&a,&as)&&!read_file(changed,&b,&bs));
    assert(as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,exit_load_cases[0].after);
    assert(!npe_world_resident_write(&w,0x526b82,exit_load_cases[0].before,288));
    assert(!npe_world_restart_game(&f,&w));compare(&w,exit_load_cases[14].after);
    assert(!remove(save)&&!remove(changed));
    unsigned bound=0;uint8_t byte;
    for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    printf("Exit cache:16 original loads/4608 bytes,%u World bytes,cross-record writes,warm restore/restart,unchanged own save passed\n",NPE_WORLD_RESIDENT_BYTES);return 0;
}
