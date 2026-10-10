#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
typedef struct {const char *name;unsigned phase,enabled,roll,group,count;
    struct {uint16_t address,value;} initial[64];unsigned calls,rng_calls;uint16_t arguments[12];} BackgroundNative;
typedef struct {const char *name;unsigned enabled;struct {uint16_t address,value;} first[5],second[5];uint16_t captures[2][15];} BackgroundEofNative;
#include "background_sound_native.h"
typedef struct {uint8_t bytes[131072];} Memory;
static uint16_t word(void *context,uint32_t a){Memory *m=context;assert(a>=0x510000 && a+1<0x530000);a-=0x510000;return (uint16_t)((unsigned)m->bytes[a]*256+m->bytes[a+1]);}
static void set_word(void *context,uint32_t a,uint16_t value){Memory *m=context;assert(a>=0x510000 && a+1<0x530000);a-=0x510000;m->bytes[a]=(uint8_t)(value>>8);m->bytes[a+1]=(uint8_t)value;}
static void set_live(NpeFile *f,Memory *m,uint16_t a,uint16_t value){
    set_word(m,0x520000+a,value);uint8_t b[]={(uint8_t)(value>>8),(uint8_t)value};assert(!npe_rpg_runtime_write(f,0x520000+a,b,2));
}
static void compare(int result,unsigned calls,const uint16_t *n,const NpeRpgSoundRequest *r){
    assert(result==(int)calls);if(!calls){assert(r->filename==0xdeadbeef && r->offset==0xdeadbeef && r->length==0xdeadbeef && r->callback==0xdeadbeef);return;}
    assert(r->filename==(uint32_t)n[0]*65536+n[1]);assert(r->offset==(uint32_t)n[2]*65536+n[3]);
    assert(r->length==(uint32_t)n[4]*65536+n[5]);assert(!n[6]&&!n[7]&&!n[8]&&!n[9]);
    assert(r->callback==(uint32_t)n[10]*65536+n[11]);
}
typedef struct {const uint16_t *expected;unsigned calls;} Trace;
static void range_request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    Trace *t=context;const uint16_t *n=t->expected;
    assert(offset==(uint32_t)n[2]*65536+n[3] && (uint32_t)length==(uint32_t)n[4]*65536+n[5]);
    assert(callback==(uint32_t)n[10]*65536+n[11]);++t->calls;
}
static uint32_t advance(uint32_t n){return n*UINT32_C(0x514f111f)+1;}
static uint32_t seed_for(unsigned roll){for(uint32_t n=0;n<1000000;++n)if(((advance(n)>>16)&32767)==roll)return n;assert(0);return 0;}
static void check_ram(Memory *m,uint8_t *native,const char *directory,const char *name){
    char path[1024];snprintf(path,sizeof(path),"%s/%s.ram.bin",directory,name);FILE *fp=fopen(path,"rb");assert(fp);
    assert(fread(native,1,sizeof(m->bytes),fp)==sizeof(m->bytes) && fgetc(fp)==EOF && !fclose(fp));
    if(memcmp(m->bytes,native,sizeof(m->bytes)))fprintf(stderr,"RAM differs:%s\n",name);
    assert(!memcmp(m->bytes,native,sizeof(m->bytes)));
}
int main(int argc,char **argv){
    assert(argc==4);NpeFile f;battle_sound_file(argv[1],&f);NpeWorld w;assert(!npe_world_init(&f,&w,9,29,13));assert(!npe_world_resources_open(&f,&w));
    Memory *m=calloc(1,sizeof(*m));uint8_t *native=malloc(131072);assert(m&&native);
    for(unsigned i=0;i<11;++i){
        unsigned section=i<4?16+i:23+i-4;uint32_t s=i<4?0x5278c4+4*i:0x527856+4*(i-4);
        uint32_t o=i<4?0x5278d4+4*i:0x527872+4*(i-4);uint8_t a[4],b[4];
        assert(!npe_rpg_runtime_read(&f,s,a,4)&&!npe_rpg_runtime_read(&f,o,b,4));uint32_t size=0,offset=0;
        for(unsigned j=0;j<4;++j){size=size*256+a[j];offset=offset*256+b[j];}
        assert(size==f.sections[section].size && offset==f.sections[section].offset);
    }
    for(unsigned i=0;i<34;++i){
        const BackgroundNative *n=&background_native[i];memset(m,0,sizeof(*m));
        for(unsigned j=0;j<n->count;++j)set_live(&f,m,n->initial[j].address,n->initial[j].value);
        NpeResidentHeap h={m,word,set_word,NULL};NpeRpgSoundRequest r={0xdeadbeef,0xdeadbeef,0xdeadbeef,0xdeadbeef};
        unsigned value=n->phase==0?(n->group==1?0:1+n->roll%3):n->roll;
        int result=!n->phase?npe_scene_sound_request(&h,value,&r):n->phase==1?npe_battle_background_start(&h,value,&r):npe_battle_background_replay(&h,&r);
        compare(result,n->calls,n->arguments,&r);check_ram(m,native,argv[2],n->name);
        r=(NpeRpgSoundRequest){0xdeadbeef,0xdeadbeef,0xdeadbeef,0xdeadbeef};compare(npe_rpg_runtime_background_sound(&f,n->phase,value,&r),n->calls,n->arguments,&r);
        /* Reset the selected pair before checking the actual world caller. */
        for(unsigned j=0;j<n->count;++j)set_live(&f,m,n->initial[j].address,n->initial[j].value);
        Trace t={n->arguments,0};w.sound_context=&t;w.sound_observer=NULL;w.sound_range_observer=range_request;
        uint32_t seed=seed_for(n->roll);npe_world_seed(&w,seed);
        if(!n->phase)npe_world_sound_scene_start(&w,n->group);
        else if(n->phase==1)npe_world_sound_battle_start(&w);
        else assert(npe_world_sound_battle_replay(&w)==(int)n->calls);
        assert(t.calls==n->calls && w.random_state==(n->rng_calls?advance(seed):seed));
        uint8_t selected[8];assert(!npe_rpg_runtime_read(&f,0x527996,selected,8));assert(!memcmp(selected,native+0x17996,8));
    }
    for(unsigned i=0;i<6;++i){
        const BackgroundEofNative *n=&background_eof_native[i];memset(m,0,sizeof(*m));
        for(unsigned turn=0;turn<2;++turn){
            for(unsigned j=0;j<5;++j)set_live(&f,m,turn?n->second[j].address:n->first[j].address,turn?n->second[j].value:n->first[j].value);
            unsigned calls=turn?(n->enabled!=0):1;const uint16_t *a=n->captures[turn];NpeResidentHeap h={m,word,set_word,NULL};
            NpeRpgSoundRequest r={0xdeadbeef,0xdeadbeef,0xdeadbeef,0xdeadbeef};compare(npe_battle_background_replay(&h,&r),calls,a,&r);
            r=(NpeRpgSoundRequest){0xdeadbeef,0xdeadbeef,0xdeadbeef,0xdeadbeef};compare(npe_rpg_runtime_background_sound(&f,2,0,&r),calls,a,&r);
            Trace t={a,0};w.sound_context=&t;w.sound_observer=NULL;w.sound_range_observer=range_request;
            uint32_t random=w.random_state;assert(npe_world_sound_battle_replay(&w)==(int)calls && t.calls==calls && w.random_state==random);
        }
        check_ram(m,native,argv[3],n->name);
    }
    npe_free(&f);free(m);free(native);
    puts("34 original background/scene slices:full128KiB RAM,requests,runtime/world RNG order;6 real IRQ/12 callback invocations/11 playback requests;11 startup directory pairs passed");return 0;
}
