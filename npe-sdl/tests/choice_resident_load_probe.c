#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[30],after[30];} ChoiceLoadCase;
#include "choice_resident_load_native.h"
static void compare(const NpeWorld *w,const uint8_t expected[30]){
    uint8_t actual[30];assert(!npe_world_resident_read(w,0x52781c,actual,30));
    for(unsigned i=0;i<30;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"choice byte%u C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<16;++i){const ChoiceLoadCase *c=&choice_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map)&&!npe_world_resident_write(&w,0x52781c,c->before,30));
        assert(!npe_world_load_choice_events(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        const uint8_t cross[]={0x12,0x34,0x56,0x78};uint8_t actual[4];
        const uint32_t addresses[]={0x527821,0x527839};
        for(unsigned j=0;j<2;++j){assert(!npe_rpg_runtime_write(&f,addresses[j],cross,4));
            assert(!npe_rpg_runtime_read(&f,addresses[j],actual,4)&&!memcmp(actual,cross,4));}
        assert(w.choice_event_records[0][5]==0x12&&w.choice_event_records[1][0]==0x34);
        assert(w.choice_event_records[4][5]==0x12&&w.chase_records[0][0]==0x34&&w.chase_records[0][1]==0x56);
        NpeWorld prior=w;assert(npe_world_resident_write(&w,0x527855,cross,2)&&!memcmp(&w,&prior,sizeof(w)));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Choice load%u %s map%u:30 original bytes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/choice-own-save.dat",*changed="tmp/choice-cache-save.dat";
    assert(!npe_world_save(&f,&w,save));assert(!npe_world_resident_write(&w,0x52781c,choice_load_cases[0].before,30));
    assert(!npe_world_save(&f,&w,changed));uint8_t *a,*b;size_t as,bs;
    assert(!read_file(save,&a,&as)&&!read_file(changed,&b,&bs)&&as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,choice_load_cases[0].after);
    assert(!npe_world_resident_write(&w,0x52781c,choice_load_cases[0].before,30));
    assert(!npe_world_restart_game(&f,&w));compare(&w,choice_load_cases[14].after);
    assert(!remove(save)&&!remove(changed));
    unsigned bound=0;uint8_t byte;for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Choice resident:16 original loads/480 bytes,3748 World bytes,cross-slot/chase writes,warm restore/restart,unchanged own save passed");return 0;
}
