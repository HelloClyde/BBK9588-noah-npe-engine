#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../npe.h"

typedef struct {
    const char *name;
    unsigned party,count,calls;
    uint8_t combine[2],before[3],after[3];
#ifdef NPE_BATTLE_TYPE_WORDS
    uint16_t before_types[3],after_types[3];
#endif
} GroupPrepareCase;
#ifdef NPE_SELECTOR_CAPTURE
#include "battle_selector_capture_native.h"
#define NATIVE_DIRECTORY "tmp/battle-selector-capture-native/"
#define ACTUAL_PREFIX "tmp/battle-selector-capture-c-"
#elif defined(NPE_BATTLE_TYPE_WORDS)
#include "battle_group_type_words_native.h"
#define NATIVE_DIRECTORY "tmp/battle-group-type-words-native/"
#define ACTUAL_PREFIX "tmp/battle-group-type-words-c-"
#else
#include "battle_group_prepare_native.h"
#define NATIVE_DIRECTORY "tmp/battle-group-prepare-native/"
#define ACTUAL_PREFIX "tmp/battle-group-prepare-c-"
#endif

static void read_records(const char *name,const char *phase,uint8_t *records) {
    char path[160];snprintf(path,sizeof(path),NATIVE_DIRECTORY "%s-%s-records.bin",name,phase);
    FILE *in=fopen(path,"rb");assert(in);
    assert(fread(records,1,888,in)==888 && fgetc(in)==EOF && !fclose(in));
}
static void save_result(const char *name,const char *kind,const uint8_t *bytes,unsigned size) {
    char path[160];snprintf(path,sizeof(path),ACTUAL_PREFIX "%s-%s.bin",name,kind);
    FILE *out=fopen(path,"wb");assert(out);
    assert(fwrite(bytes,1,size,out)==size && !fclose(out));
}
static void image(const NpeFile *f,const NpeWorld *w,uint8_t *bytes) {
    for(unsigned slot=0;slot<6;++slot) {
        uint8_t runtime[148];assert(!npe_world_battle_record_image(w,slot,bytes+slot*148));
        assert(!npe_rpg_runtime_read(f,0x526450+148*slot,runtime,148));
        assert(!memcmp(runtime,bytes+slot*148,148));
    }
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in);
    assert(!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0);
    assert(!fseek(in,0,SEEK_SET));uint8_t *bytes=malloc((size_t)size);assert(bytes);
    assert(fread(bytes,1,(size_t)size,in)==(size_t)size && !fclose(in));
    NpeFile f;char error[128];assert(!npe_parse(&f,bytes,(size_t)size,error,sizeof(error)));
    const uint32_t addresses[]={0x527a9e,0x527aae,0x527ac0};
    for(unsigned ci=0;ci<sizeof(group_prepare_cases)/sizeof(*group_prepare_cases);++ci) {
        const GroupPrepareCase *c=&group_prepare_cases[ci];NpeWorld w={0};
        uint8_t before[888],expected[888],actual[888];
        read_records(c->name,"before",before);read_records(c->name,"after",expected);
        w.game_mode=3;w.battle_party_flags_saved=1;w.battle_record_party_count=(uint8_t)c->party;
        w.battle_formation_count=w.battle_enemy_count=(uint8_t)c->count;
        for(unsigned m=0;m<3;++m)w.party_active[m]=(uint8_t)(m<c->party);
        for(unsigned slot=0;slot<6;++slot)assert(!npe_world_record_write(&w,1,slot,0,before+148*slot,148));
        assert(!npe_world_resources_open(&f,&w));
        for(unsigned k=0;k<3;++k)assert(!npe_rpg_runtime_write(&f,addresses[k],c->before+k,1));
#ifdef NPE_BATTLE_TYPE_WORDS
        const uint32_t type_addresses[]={0x527aa0,0x527aa6,0x527aaa};
        for(unsigned k=0;k<3;++k){uint8_t value[2]={(uint8_t)(c->before_types[k]>>8),(uint8_t)c->before_types[k]};
            assert(!npe_rpg_runtime_write(&f,type_addresses[k],value,2));}
#endif
        image(&f,&w,actual);assert(!memcmp(actual,before,888));
        for(unsigned k=0;k<c->calls;++k)
            assert(!(c->combine[k]==2?npe_world_battle_capture_selector(&w):c->combine[k]?npe_world_battle_prepare_combine(&w):npe_world_battle_prepare_group(&w)));
        image(&f,&w,actual);
        for(unsigned i=0;i<888;++i)if(actual[i]!=expected[i]) {
            fprintf(stderr,"%s slot%u byte%u C%u original%u\n",c->name,i/148,i%148,actual[i],expected[i]);break;
        }
        assert(!memcmp(actual,expected,888));save_result(c->name,"records",actual,888);
        uint8_t controls[3];
        for(unsigned k=0;k<3;++k) {
            assert(!npe_rpg_runtime_read(&f,addresses[k],controls+k,1));
            assert(controls[k]==c->after[k]);
        }
        save_result(c->name,"controls",controls,3);
#ifdef NPE_BATTLE_TYPE_WORDS
        uint8_t types[6];
        for(unsigned k=0;k<3;++k){
            assert(!npe_rpg_runtime_read(&f,type_addresses[k],types+2*k,2));
            assert((unsigned)types[2*k]*256+types[2*k+1]==c->after_types[k]);}
        save_result(c->name,"types",types,6);
#endif
#ifndef NPE_SELECTOR_CAPTURE
        assert(w.battle_pending_mask==((1u<<c->party)-1u));
#endif
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"%s: six original records and shared targets/kind/actor passed\n",c->name);
    }
    npe_free(&f);puts("18 original group/combine preparations,108 full records and54 shared controls passed");return 0;
}
