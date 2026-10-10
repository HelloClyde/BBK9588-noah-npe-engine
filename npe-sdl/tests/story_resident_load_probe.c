#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[90],after[90];} StoryResidentLoadCase;
#include "story_resident_load_native.h"
#include "object_bulk_native.h"
static void compare(const NpeWorld *w,const uint8_t expected[90]){
    uint8_t actual[90];assert(!npe_world_resident_read(w,0x5274f4,actual,90));
    for(unsigned i=0;i<90;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"story byte%u C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<sizeof(story_resident_load_cases)/sizeof(*story_resident_load_cases);++i){
        const StoryResidentLoadCase *c=&story_resident_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));
        assert(!npe_world_resident_write(&w,0x5274f4,c->before,90));
        assert(!npe_world_load_story_event_cache(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        const uint8_t cross[]={0x12,0x34,0x56,0x78};uint8_t actual[4];
        const uint32_t addresses[]={0x5274f3,0x5274f9};
        for(unsigned j=0;j<2;++j){assert(!npe_rpg_runtime_write(&f,addresses[j],cross,4));
            assert(!npe_rpg_runtime_read(&f,addresses[j],actual,4)&&!memcmp(actual,cross,4));}
        assert(w.object_done[199]==0x12&&w.story_events[0].next_id==0x3456&&w.story_event_padding[0]==0x12);
        NpeMapStoryEvent event;w.story_events[14].next_id=0x23cd;uint8_t high=0x81;
        assert(!npe_rpg_runtime_write(&f,0x527548,&high,1));
        assert(npe_world_story_event(&w,271,&event)==1&&event.next_id==0x81cd&&!event.raw);
        NpeWorld prior=w;assert(npe_world_resident_write(&w,0x5275dd,cross,2));assert(!memcmp(&w,&prior,sizeof(w)));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Story resident load%u %s map%u:90 original bytes and shared writes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/story-resident-self-save.dat",*changed="tmp/story-resident-cache-save.dat";
    assert(!npe_world_save(&f,&w,save));
    assert(!npe_world_resident_write(&w,0x5274f4,story_resident_load_cases[0].before,90));
    assert(!npe_world_save(&f,&w,changed));uint8_t *a,*b;size_t as,bs;
    assert(!read_file(save,&a,&as)&&!read_file(changed,&b,&bs));
    assert(as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,story_resident_load_cases[0].after);
    assert(!npe_world_resident_write(&w,0x5274f4,story_resident_load_cases[0].before,90));
    assert(!npe_world_restart_game(&f,&w));compare(&w,story_resident_load_cases[14].after);
    assert(!remove(save)&&!remove(changed));
    for(unsigned i=0;i<sizeof(object_bulk_cases)/sizeof(*object_bulk_cases);++i){
        const unsigned *c=object_bulk_cases[i];NpeMap map;
        assert(!npe_world_init(&f,&w,80,27,20)&&!npe_map(&f,79,&map));
        unsigned initial=npe_map_tile(&map,26,20);assert(initial!=53);
        uint8_t id[]={(uint8_t)(c[0]>>8),(uint8_t)c[0]},flag=(uint8_t)c[1];
        assert(!npe_world_resident_write(&w,0x52759e,id,2));
        assert(!npe_world_resident_write(&w,0x52742c+c[0]-1,&flag,1));
        assert(!npe_world_load_tiles(&w,&map));
        assert(npe_world_tile(&w,&map,26,20)==(c[2]==53?53:initial));
        fprintf(stderr,"Original bulk id%u flag%u:terrain result matched\n",c[0],c[1]);
    }
    /* Diagnostic file copy: our own save must retain a live ID101 pickup
       although the native bulk loader intentionally does not clear it. */
    NpeMap map;const uint8_t *object_bytes;assert(!npe_map(&f,79,&map)&&!npe_map_object(&map,0,&object_bytes));
    uint8_t original_id[2];memcpy(original_id,object_bytes,2);
    uint8_t *mutable_object=(uint8_t *)object_bytes;mutable_object[0]=0;mutable_object[1]=101;
    assert(!npe_world_init(&f,&w,80,27,20));w.direction=3;NpeMapObject object;
    assert(npe_world_object_action(&f,&w,&object)==1&&w.object_done[100]==1);
    assert(!npe_world_save(&f,&w,save));NpeWorld loaded;
    assert(!npe_world_load(&f,&loaded,save)&&loaded.object_done[100]==1&&npe_world_tile(&loaded,&map,26,20)==53);
    assert(!remove(save));memcpy(mutable_object,original_id,2);
    unsigned bound=0;uint8_t byte;
    for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Story resident:16 original loads/1440 bytes,3748 World bytes,partial/cross-slot writes,warm restore/restart,unchanged own save passed");return 0;
}
