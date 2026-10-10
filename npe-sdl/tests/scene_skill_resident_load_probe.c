#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[96],after[96];} SceneSkillLoadCase;
#include "scene_skill_resident_load_native.h"
static void seed(NpeWorld *w,const uint8_t bytes[96]){
    assert(!npe_world_resident_write(w,0x52754e,bytes,80));
    assert(!npe_world_resident_write(w,0x52790a,bytes+80,12));
    assert(!npe_world_resident_write(w,0x5279a2,bytes+92,4));
}
static void compare(const NpeWorld *w,const uint8_t expected[96]){
    uint8_t actual[96];assert(!npe_world_resident_read(w,0x52754e,actual,80));
    assert(!npe_world_resident_read(w,0x52790a,actual+80,12));
    assert(!npe_world_resident_read(w,0x5279a2,actual+92,4));
    for(unsigned i=0;i<96;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"scene/skill/leader byte%u C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<16;++i){const SceneSkillLoadCase *c=&scene_skill_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));seed(&w,c->before);
        assert(!npe_world_load_scene_events(&w,&map)&&!npe_world_load_skill_events(&w,&map)&&!npe_world_load_party_adjust_event(&w,&map));
        compare(&w,c->after);assert(!npe_world_resources_open(&f,&w));
        const uint8_t cross[]={0x12,0x34,0x56,0x78};uint8_t actual[4];
        const uint32_t addresses[]={0x52754d,0x52759d,0x52790f,0x5279a1};
        for(unsigned j=0;j<4;++j){assert(!npe_rpg_runtime_write(&f,addresses[j],cross,4));
            assert(!npe_rpg_runtime_read(&f,addresses[j],actual,4)&&!memcmp(actual,cross,4));}
        assert(w.story_event_padding[14]==0x12&&w.scene_event_records[0][0]==0x34&&w.scene_event_records[4][15]==0x12);
        assert(w.object_records[0][0]==0x34&&w.skill_event_padding[0]==0x12&&w.skill_events[1].next_id==0x3456&&w.skill_events[1].skill_id==0x78);
        assert((w.money&255)==0x12&&w.party_adjust_event.character_id==0x34&&w.party_adjust_event.next_mode==0x56);
        uint8_t high=0x81;assert(!npe_rpg_runtime_write(&f,0x527918-6,&high,1));
        NpeMapSkillEvent skill;assert(npe_world_skill_event(&w,258,&skill)==1&&skill.next_id==0x3456&&skill.skill_id==0x81);
        NpeWorld prior=w;assert(npe_world_resident_write(&w,0x52791d,cross,2)&&!memcmp(&w,&prior,sizeof(w)));
        NpeMapAnimationEvent scene;assert(npe_world_scene_event(&w,261,&scene)==1&&!scene.raw&&scene.y2==(uint16_t)((c->after[78]<<8)|0x12));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Scene/skill/leader load%u %s map%u:96 original bytes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/scene-skill-own-save.dat",*changed="tmp/scene-skill-cache-save.dat";
    assert(!npe_world_save(&f,&w,save));seed(&w,scene_skill_load_cases[0].before);
    assert(!npe_world_save(&f,&w,changed));uint8_t *a,*b;size_t as,bs;
    assert(!read_file(save,&a,&as)&&!read_file(changed,&b,&bs)&&as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,scene_skill_load_cases[0].after);
    seed(&w,scene_skill_load_cases[0].before);assert(!npe_world_restart_game(&f,&w));compare(&w,scene_skill_load_cases[14].after);
    assert(!remove(save)&&!remove(changed));
    unsigned bound=0;uint8_t byte;for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Scene/skill/leader resident:16 original loads/1536 bytes,3748 World bytes,partial/cross-slot writes,warm restore/restart,unchanged own save passed");return 0;
}
