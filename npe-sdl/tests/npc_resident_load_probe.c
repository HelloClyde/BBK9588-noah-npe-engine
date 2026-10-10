#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {unsigned map;uint8_t before[280],after[280];} NpcLoadCase;
#include "npc_resident_load_native.h"
int main(int argc,char **argv){
    assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&file);
    for(unsigned i=0;i<sizeof(load_cases)/sizeof(*load_cases);++i){
        const NpcLoadCase *c=&load_cases[i];NpeWorld w={0};NpeMap map;uint8_t actual[280];
        assert(!npe_map(&file,c->map-1,&map));
        assert(!npe_world_resident_write(&w,0x526ca2,c->before,280));
        assert(!npe_world_load_npc_records(&w,&map));
        assert(!npe_world_resident_read(&w,0x526ca2,actual,280));
        for(unsigned j=0;j<280;++j)if(actual[j]!=c->after[j]){
            fprintf(stderr,"map%u byte%u:C%u original%u\n",c->map,j,actual[j],c->after[j]);assert(0);
        }
        /* Unaligned cross-slot writes, opaque padding, byte255 coordinates,
           invalid whole-range writes must retain the authoritative image. */
        assert(!npe_world_resources_open(&file,&w));
        uint8_t cross[]={201,202,203,204,255,255};
        assert(!npe_rpg_runtime_write(&file,0x526caf,cross,sizeof(cross)));
        memcpy(actual+13,cross,sizeof(cross));uint8_t readback[280];
        assert(!npe_rpg_runtime_read(&file,0x526ca2,readback,280));assert(!memcmp(actual,readback,280));
        assert(npe_world_resident_write(&w,0x526db9,cross,2));
        assert(!npe_world_resident_read(&w,0x526ca2,readback,280));assert(!memcmp(actual,readback,280));
        uint8_t xy[]={255,255},direction=255;
        assert(!npe_rpg_runtime_write(&file,0x526cab,xy,2));
        assert(!npe_rpg_runtime_write(&file,0x526caa,&direction,1));
        const uint8_t *raw;unsigned x,y;assert(!npe_world_npc(&w,&map,0,&raw));
        npe_world_npc_position(&w,raw,0,&x,&y);assert(x==255&&y==255&&raw[6]==255);
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
        fprintf(stderr,"NPC load%u map%u:280 original bytes and shared writes match\n",i,c->map);
    }
    /* Running UI restore rebuilds NPC metadata and patrol positions from
       NPE while preserving live padding; this does not change NPESAV22. */
    NpeWorld w;assert(!npe_world_new_game(&file,&w));
    const char *save="tmp/npc-resident-self-save.dat";
    assert(!npe_world_save(&file,&w,save));
    uint8_t changed[280];for(unsigned i=0;i<280;++i)changed[i]=(uint8_t)(i*37+129);
    assert(!npe_world_resident_write(&w,0x526ca2,changed,280));
    assert(!npe_world_restore(&file,&w,save));
    uint8_t restored[280];assert(!npe_world_resident_read(&w,0x526ca2,restored,280));
    for(unsigned i=0;i<280;++i)assert(restored[i]==load_cases[4].after[i]);
    assert(!npe_world_resident_write(&w,0x526ca2,changed,280));
    assert(!npe_world_restart_game(&file,&w));
    assert(!npe_world_resident_read(&w,0x526ca2,restored,280));
    for(unsigned i=0;i<280;++i)assert(restored[i]==load_cases[10].after[i]);
    assert(!remove(save));
    assert(!npe_world_new_game(&file,&w));w.direction=0;
    memset(changed,0,sizeof(changed));changed[0]=0x12;changed[1]=0x34;
    changed[9]=(uint8_t)(w.view.offset_x+w.view.screen_x);
    changed[10]=(uint8_t)(w.view.offset_y+w.view.screen_y+1);
    assert(!npe_world_resident_write(&w,0x526ca2,changed,280));
    unsigned id=0;assert(npe_world_facing_npc(&file,&w,&id)==1&&id==0x1234);
    uint8_t progress[]={0,9,0,9};assert(!npe_world_resident_write(&w,0x526ca6,progress,4));
    assert(!npe_world_facing_npc(&file,&w,&id));
    assert(npe_game_fingerprint(&file)==fingerprint);npe_free(&file);
    puts("NPC resident:12 native loads, shared byte writes, own UI restore passed");return 0;
}
