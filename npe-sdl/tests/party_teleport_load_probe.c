#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[38],after[38];} PartyTeleportLoadCase;
#include "party_teleport_load_native.h"
static void compare(const NpeWorld *w,const uint8_t expected[38]){
    uint8_t actual[38];assert(!npe_world_resident_read(w,0x52788e,actual,38));
    for(unsigned i=0;i<38;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"party/teleport byte%u C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<sizeof(load_cases)/sizeof(*load_cases);++i){
        const PartyTeleportLoadCase *c=&load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));
        assert(!npe_world_resident_write(&w,0x52788e,c->before,38));
        assert(!npe_world_load_teleport_events(&w,&map)&&!npe_world_load_party_events(&w,&map));
        compare(&w,c->after);assert(!npe_world_resources_open(&f,&w));
        uint8_t cross[]={0x12,0x34,0x56,0x78},actual[4];
        assert(!npe_rpg_runtime_write(&f,0x527897,cross,4));
        assert(!npe_rpg_runtime_read(&f,0x527897,actual,4)&&!memcmp(cross,actual,4));
        assert(!npe_rpg_runtime_write(&f,0x5278a1,cross,4));
        assert(!npe_rpg_runtime_read(&f,0x5278a1,actual,4)&&!memcmp(cross,actual,4));
        assert(w.party_events[0].add_member==0x34&&w.party_events[0].character_id==0x56);
        assert(!npe_rpg_runtime_write(&f,0x5278a7,cross,4));
        assert(!npe_rpg_runtime_read(&f,0x5278a7,actual,4)&&!memcmp(cross,actual,4));
        NpeWorld before=w;assert(npe_world_resident_write(&w,0x5278b3,cross,2));
        assert(!memcmp(&w,&before,sizeof(w)));
        uint8_t high=0x81;w.teleport_events[1].destination_map=0x55aa;
        assert(!npe_rpg_runtime_write(&f,0x527898,&high,1));assert(w.teleport_events[1].destination_map==0x81aa);
        w.party_events[2].next_id=0x23cd;
        assert(!npe_rpg_runtime_write(&f,0x5278b2,&high,1));assert(w.party_events[2].next_id==0x81cd);
        NpeMapPartyEvent party;NpeMapTeleportEvent tele;
        assert(npe_world_party_event(&w,0x103,&party)==1&&party.next_id==0x81cd);
        assert(npe_world_teleport_event(&w,0x102,&tele)==1&&tele.destination_map==0x81aa);
        assert(!npe_world_party_event(&w,0x100,&party)&&!npe_world_party_event(&w,4,&party));
        assert(!npe_world_teleport_event(&w,0x100,&tele)&&!npe_world_teleport_event(&w,3,&tele));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Party/teleport load%u %s map%u:38 original bytes/shared writes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/party-teleport-self-save.dat",*changed="tmp/party-teleport-cache-save.dat";
    assert(!npe_world_save(&f,&w,save));assert(!npe_world_resident_write(&w,0x52788e,load_cases[0].before,38));
    assert(!npe_world_save(&f,&w,changed));uint8_t *a,*b;size_t as,bs;
    assert(!read_file(save,&a,&as)&&!read_file(changed,&b,&bs));
    assert(as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,load_cases[0].after);
    assert(!npe_world_resident_write(&w,0x52788e,load_cases[0].before,38));
    assert(!npe_world_restart_game(&f,&w));compare(&w,load_cases[14].after);
    assert(!remove(save)&&!remove(changed));
    unsigned bound=0;uint8_t byte;
    for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Party/teleport:16 original loads/608 bytes,3748 World bytes,warm restore/restart and unchanged own save passed");return 0;
}
