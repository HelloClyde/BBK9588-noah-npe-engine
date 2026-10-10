#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[64],after[64];} ObjectLoadCase;
#include "object_record_load_native.h"
static void compare(const NpeWorld *w,const uint8_t expected[64]){
    uint8_t actual[64];assert(!npe_world_resident_read(w,0x52759e,actual,64));
    for(unsigned i=0;i<64;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"object byte%u C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<sizeof(object_load_cases)/sizeof(*object_load_cases);++i){
        const ObjectLoadCase *c=&object_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));
        assert(!npe_world_resident_write(&w,0x52759e,c->before,64));
        assert(!npe_world_load_object_records(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        const uint8_t cross[]={0x12,0x34,0x56,0x78};uint8_t actual[4];
        assert(!npe_rpg_runtime_write(&f,0x5275a5,cross,4));
        assert(!npe_rpg_runtime_read(&f,0x5275a5,actual,4)&&!memcmp(actual,cross,4));
        NpeMapObject object;assert(!npe_world_object(&w,&map,1,&object));
        assert(object.id==0x3456&&w.object_records[0][7]==0x12);
        w.object_records[7][4]=0x23;w.object_records[7][5]=0xcd;
        uint8_t high=0x81;assert(!npe_rpg_runtime_write(&f,0x5275da,&high,1));
        assert(!npe_world_object(&w,&map,7,&object)&&object.item_index==0x81cd&&object.raw==NULL);
        assert(npe_world_object(&w,&map,8,&object)==-1);
        NpeWorld previous=w;assert(npe_world_resident_write(&w,0x5275dd,cross,2));
        assert(!memcmp(&w,&previous,sizeof(w)));
        assert(!npe_rpg_runtime_write(&f,0x52742b,cross,4));
        assert(w.object_done[0]==0x34&&w.object_done[1]==0x56&&w.object_done[2]==0x78);
        previous=w;assert(npe_world_resident_write(&w,0x5275dd,cross,2));
        assert(!memcmp(&w,&previous,sizeof(w))&&!npe_rpg_runtime_failures(&f));
        npe_rpg_runtime_close(&f);
        fprintf(stderr,"Object load%u %s map%u:64 original bytes and live writes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/object-record-self-save.dat",*changed="tmp/object-record-cache-save.dat";
    assert(!npe_world_save(&f,&w,save));
    assert(!npe_world_resident_write(&w,0x52759e,object_load_cases[0].before,64));
    assert(!npe_world_save(&f,&w,changed));uint8_t *a,*b;size_t as,bs;
    assert(!read_file(save,&a,&as)&&!read_file(changed,&b,&bs));
    assert(as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,object_load_cases[0].after);
    for(unsigned i=0;i<200;++i)w.object_done[i]=(uint8_t)(i*37+0x81);
    assert(!npe_world_save(&f,&w,save));memset(w.object_done,0,200);
    assert(!npe_world_restore(&f,&w,save));
    for(unsigned i=0;i<200;++i)assert(w.object_done[i]==(uint8_t)(i*37+0x81));
    assert(!npe_world_resident_write(&w,0x52759e,object_load_cases[0].before,64));
    assert(!npe_world_restart_game(&f,&w));compare(&w,object_load_cases[14].after);
    assert(!remove(save)&&!remove(changed));
    unsigned bound=0;uint8_t byte;
    for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Objects:16 original loads/1024 bytes,3748 World bytes,live partial/cross-slot writes,warm restore/restart,own save passed");return 0;
}
