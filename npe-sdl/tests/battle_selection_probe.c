#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    const char *name;unsigned flag,count,context,chase,seed,key,slot;
    unsigned before[3];uint8_t enemies[8];unsigned monster[3],after[3],after_count;
    uint32_t after_rng;
} SelectionCase;
#include "battle_selection_native.h"
static void word(const NpeFile *f,uint32_t address,unsigned v){
    uint8_t raw[]={(uint8_t)(v>>8),(uint8_t)v};assert(!npe_rpg_runtime_write(f,address,raw,2));
}
int main(int argc,char **argv){
    assert(argc==2);FILE *fp=fopen(argv[1],"rb");assert(fp&&!fseek(fp,0,SEEK_END));
    long length=ftell(fp);assert(length>0&&!fseek(fp,0,SEEK_SET));uint8_t *bytes=malloc((size_t)length);
    assert(bytes&&fread(bytes,1,(size_t)length,fp)==(size_t)length&&!fclose(fp));
    NpeFile f;char error[128];assert(!npe_parse(&f,bytes,(size_t)length,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned c=0;c<sizeof(selection_cases)/sizeof(selection_cases[0]);++c){
        const SelectionCase *s=&selection_cases[c];NpeWorld w;assert(!npe_world_init(&f,&w,9,29,13));
        assert(!npe_world_resources_open(&f,&w));w.story_event_id=1;w.event_context_id=(uint16_t)s->context;
        uint8_t flag=(uint8_t)s->flag,num=(uint8_t)s->count,records[56]={0};
        assert(!npe_rpg_runtime_write(&f,0x527a9a,&flag,1)&&!npe_rpg_runtime_write(&f,0x527af0,&num,1));
        assert(!npe_rpg_runtime_write(&f,0x527916,s->enemies,8));
        records[s->slot*14+11]=(uint8_t)s->monster[1];records[s->slot*14+12]=(uint8_t)s->monster[0];records[s->slot*14+13]=(uint8_t)s->monster[2];
        assert(!npe_rpg_runtime_write(&f,0x52761a,records,56));uint8_t key=(uint8_t)s->key;
        assert(!npe_rpg_runtime_write(&f,0x5277fc,&key,1));
        for(unsigned i=0;i<3;++i){word(&f,0x52792e + i*2,s->before[i]);w.battle_enemy_ids[i]=(uint8_t)(17+i);w.battle_enemy_hps[i]=0xa5a5;}
        if(s->context>0&&s->context<32768)word(&f,0x527840+(s->context-1u)*14u,s->chase);
        npe_world_seed(&w,s->seed);assert(!npe_world_select_battle_enemies(&f,&w));
        assert(w.battle_formation_count==s->after_count&&w.battle_scripted_entry==s->flag&&w.random_state==s->after_rng);
        for(unsigned i=0;i<3;++i){assert(w.battle_selected_ids[i]==s->after[i]&&!w.battle_enemy_hps[i]&&w.battle_enemy_ids[i]==17+i);}
        uint8_t raw[6];assert(!npe_rpg_runtime_read(&f,0x52792e,raw,6));
        for(unsigned i=0;i<3;++i)assert((unsigned)raw[i*2]*256+raw[i*2+1]==s->after[i]);
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);fprintf(stderr,"%s:three word IDs,NUM,flag,HP and RNG match original\n",s->name);
    }
    NpeWorld w={0};const uint8_t initial[]={0x12,0x34,0x56,0x78,0x9a,0xbc},cross[]={0xfe,0xdc,0xba,0x98};
    assert(!npe_world_resident_write(&w,0x52792e,initial,6)&&!npe_world_resident_write(&w,0x52792f,cross,4));
    assert(w.battle_selected_ids[0]==0x12fe&&w.battle_selected_ids[1]==0xdcba&&w.battle_selected_ids[2]==0x98bc);
    NpeWorld before=w;assert(npe_world_resident_write(&w,0x527933,cross,2)&&!memcmp(&w,&before,sizeof(w)));
    unsigned bound=0;uint8_t byte;for(uint32_t a=0x510000;a<0x530000;++a)bound+=!npe_world_resident_read(&w,a,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("32 original selectors match;3748 World bytes;full-word/partial/cross-word writes and independent loaded IDs passed");return 0;
}
