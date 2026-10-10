#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
typedef struct {
    const char *name;unsigned count;
    struct {uint16_t address,value;} initial[32];
    unsigned calls,marker;uint16_t arguments[12];
} BattleSoundNative;
#include "battle_sound_native.h"
typedef struct {uint8_t bytes[131072];unsigned writes;} Memory;
static uint16_t read_word(void *context,uint32_t a){
    Memory *m=context;assert(a>=0x510000 && a+1<0x530000);
    a-=0x510000;return (uint16_t)((unsigned)m->bytes[a]*256+m->bytes[a+1]);
}
static void write_word(void *context,uint32_t a,uint16_t value){
    Memory *m=context;assert(a>=0x510000 && a+1<0x530000);
    a-=0x510000;m->bytes[a]=(uint8_t)(value>>8);m->bytes[a+1]=(uint8_t)value;++m->writes;
}
static void compare_request(const BattleSoundNative *n,int result,const NpeBattleSoundRequest *r){
    assert(result==(int)n->calls);
    if(result){
        uint32_t file=(uint32_t)n->arguments[0]*65536+n->arguments[1];
        uint32_t offset=(uint32_t)n->arguments[2]*65536+n->arguments[3];
        uint32_t bits=(uint32_t)n->arguments[4]*65536+n->arguments[5];
        int32_t length=bits<=INT32_MAX?(int32_t)bits:(int32_t)(bits-UINT32_C(0x80000000))+INT32_MIN;
        assert(r->filename==file && r->offset==offset && r->length==length);
        for(unsigned i=6;i<12;++i)assert(!n->arguments[i]);
    }else assert(r->filename==0xdeadbeef && r->offset==0xfacefeed && r->length==-123);
}
int main(int argc,char **argv){
    assert(argc==3);Memory *m=calloc(1,sizeof(*m));assert(m);
    uint8_t *expected=malloc(sizeof(m->bytes));assert(expected);
    unsigned count=sizeof(battle_sound_native)/sizeof(*battle_sound_native);
    for(unsigned i=0;i<count;++i){
        const BattleSoundNative *n=&battle_sound_native[i];memset(m,0,sizeof(*m));
        for(unsigned j=0;j<n->count;++j)write_word(m,0x520000u+n->initial[j].address,n->initial[j].value);
        m->writes=0;NpeResidentHeap h={m,read_word,write_word,NULL};
        NpeBattleSoundRequest request={0xdeadbeef,0xfacefeed,-123};
        compare_request(n,npe_battle_sound_request(&h,&request),&request);
        char path[1024];snprintf(path,sizeof(path),"%s/%s.ram.bin",argv[2],n->name);
        FILE *f=fopen(path,"rb");assert(f);assert(fread(expected,1,sizeof(m->bytes),f)==sizeof(m->bytes));
        assert(fgetc(f)==EOF);fclose(f);
        if(memcmp(m->bytes,expected,sizeof(m->bytes)))fprintf(stderr,"selector RAM differs: %s\n",n->name);
        assert(!memcmp(m->bytes,expected,sizeof(m->bytes)));
        assert(m->bytes[0x17a98]==n->marker && m->bytes[0x17a99]==0xb7);
    }
    NpeFile file;battle_sound_file(argv[1],&file);
    NpeWorld world;assert(!npe_world_init(&file,&world,9,29,13));assert(!npe_world_resources_open(&file,&world));
    const uint32_t addresses[]={0x52794a,0x527972,0x52796a,0x527962,
                                0x52795a,0x527952,0x527942,0x52793a};
    for(unsigned i=0;i<8;++i){
        uint8_t pair[8];assert(!npe_rpg_runtime_read(&file,addresses[i],pair,8));
        uint32_t size=0,offset=0;
        for(unsigned j=0;j<4;++j){size=size*256+pair[j];offset=offset*256+pair[4+j];}
        assert(size==file.sections[30+i].size && offset==file.sections[30+i].offset);
    }
    for(unsigned i=0;i<count;++i){
        const BattleSoundNative *n=&battle_sound_native[i];
        for(unsigned j=0;j<n->count;++j){
            uint8_t bytes[]={(uint8_t)(n->initial[j].value>>8),(uint8_t)n->initial[j].value};
            assert(!npe_rpg_runtime_write(&file,0x520000u+n->initial[j].address,bytes,2));
        }
        NpeBattleSoundRequest request={0xdeadbeef,0xfacefeed,-123};
        compare_request(n,npe_rpg_runtime_battle_sound(&file,&request),&request);
        uint8_t marker[2];assert(!npe_rpg_runtime_read(&file,0x527a98,marker,2));
        assert(marker[0]==n->marker && marker[1]==0xb7);
    }
    /* Complete music word must be live in both byte orders, without truncation. */
    uint8_t word[]={1,1},byte=0;assert(!npe_rpg_runtime_write(&file,0x527a50,word,2));
    assert(world.music_enabled==0x101);assert(!npe_world_resident_write(&world,0x527a50,&byte,1));
    assert(world.music_enabled==1);assert(!npe_rpg_runtime_read(&file,0x527a50,word,2));
    assert(word[0]==0 && word[1]==1);
    unsigned bound=0;for(uint32_t a=0x510000;a<0x530000;++a)bound+=!npe_world_resident_read(&world,a,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES);npe_free(&file);free(m);free(expected);
    puts("33 unchanged PRG selectors: full128KiB RAM/arguments and production runtime requests match;3748 World bytes");
    return 0;
}
