#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
typedef struct {
    const char *name;unsigned first_count,second_count;
    struct {uint16_t address,value;} first[20],second[8];uint16_t captures[2][15];
} MapEofNative;
#include "map_sound_eof_native.h"
typedef struct {uint8_t bytes[131072];} Memory;
static uint16_t word(void *context,uint32_t a){
    Memory *m=context;assert(a>=0x510000 && a+1<0x530000);a-=0x510000;
    return (uint16_t)((unsigned)m->bytes[a]*256+m->bytes[a+1]);
}
static void set_word(Memory *m,uint16_t a,uint16_t value){
    m->bytes[0x10000+a]=(uint8_t)(value>>8);m->bytes[0x10001+a]=(uint8_t)value;
}
static void compare(const uint16_t *n,const NpeMapSoundRequest *r){
    assert(r->filename==(uint32_t)n[0]*65536+n[1]);assert(r->offset==(uint32_t)n[2]*65536+n[3]);
    assert(r->length==(uint32_t)n[4]*65536+n[5]);assert(!n[6] && !n[7] && !n[8] && !n[9]);
    assert(r->callback==(uint32_t)n[10]*65536+n[11]);
}
typedef struct {NpeWorld *world;const uint16_t *expected;unsigned requests;} Trace;
static void request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    Trace *t=context;assert(callback==NPE_SOUND_CALLBACK_MAP);assert(t->world->music_enabled==t->expected[12]);
    assert(offset==(uint32_t)t->expected[2]*65536+t->expected[3]);
    assert((uint32_t)length==(uint32_t)t->expected[4]*65536+t->expected[5]);++t->requests;
}
int main(int argc,char **argv){
    assert(argc==3);NpeFile f;battle_sound_file(argv[1],&f);NpeWorld w;
    assert(!npe_world_init(&f,&w,9,29,13));assert(!npe_world_resources_open(&f,&w));
    Memory *m=calloc(1,sizeof(*m));assert(m);uint8_t *native=malloc(sizeof(m->bytes));assert(native);
    for(unsigned i=0;i<sizeof(map_eof_native)/sizeof(*map_eof_native);++i){
        const MapEofNative *n=&map_eof_native[i];memset(m,0,sizeof(*m));Trace t={&w,NULL,0};
        w.sound_context=&t;w.sound_observer=NULL;w.sound_range_observer=request;
        for(unsigned turn=0;turn<2;++turn){
            unsigned count=turn?n->second_count:n->first_count;
            for(unsigned j=0;j<count;++j){
                uint16_t a=turn?n->second[j].address:n->first[j].address;
                uint16_t value=turn?n->second[j].value:n->first[j].value;set_word(m,a,value);
                uint8_t bytes[]={(uint8_t)(value>>8),(uint8_t)value};
                assert(!npe_rpg_runtime_write(&f,0x520000+a,bytes,2));
            }
            NpeResidentHeap h={m,word,NULL,NULL};NpeMapSoundRequest pure,live;
            assert(npe_map_sound_request(&h,&pure)==1);compare(n->captures[turn],&pure);
            assert(npe_rpg_runtime_map_sound(&f,&live)==1);compare(n->captures[turn],&live);
            t.expected=n->captures[turn];uint32_t random=w.random_state;
            assert(npe_world_sound_map_replay(&w)==1 && w.random_state==random);
        }
        assert(t.requests==2);char path[1024];snprintf(path,sizeof(path),"%s/%s.ram.bin",argv[2],n->name);
        FILE *fp=fopen(path,"rb");assert(fp);assert(fread(native,1,sizeof(m->bytes),fp)==sizeof(m->bytes));
        assert(fgetc(fp)==EOF && !fclose(fp));assert(!memcmp(native,m->bytes,sizeof(m->bytes)));
    }
    assert(npe_world_sound_map_replay(NULL)==-1);npe_free(&f);free(native);free(m);
    puts("6 original IRQ paths/12 real map callbacks:full32 parameters,pure full128KiB RAM,runtime and unconditional World replay match;no RNG draw");
    return 0;
}
