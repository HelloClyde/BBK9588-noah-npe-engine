#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
typedef struct {
    const char *name;unsigned operation,enabled,pending,slot,count;
    struct {uint16_t address,value;} initial[24];
    unsigned calls,stops,play_word,stop_word,after_word;uint16_t arguments[12];
} MapSoundNative;
#include "map_sound_native.h"
typedef struct {uint8_t bytes[131072];unsigned writes;} Memory;
static uint16_t read_word(void *context,uint32_t address){
    Memory *m=context;assert(address>=0x510000 && address+1<0x530000);
    address-=0x510000;return (uint16_t)((unsigned)m->bytes[address]*256+m->bytes[address+1]);
}
static void write_word(void *context,uint32_t address,uint16_t value){
    Memory *m=context;assert(address>=0x510000 && address+1<0x530000);
    address-=0x510000;m->bytes[address]=(uint8_t)(value>>8);m->bytes[address+1]=(uint8_t)value;++m->writes;
}
static uint32_t read_long(Memory *m,uint32_t address){
    return (uint32_t)read_word(m,address)*65536+read_word(m,address+2);
}
static void compare_request(const MapSoundNative *n,const NpeMapSoundRequest *r){
    if(!n->calls)return;
    assert(r->filename==(uint32_t)n->arguments[0]*65536+n->arguments[1]);
    assert(r->offset==(uint32_t)n->arguments[2]*65536+n->arguments[3]);
    assert(r->length==(uint32_t)n->arguments[4]*65536+n->arguments[5]);
    assert(r->callback==(uint32_t)n->arguments[10]*65536+n->arguments[11]);
    for(unsigned i=6;i<10;++i)assert(!n->arguments[i]);
}
typedef struct {NpeWorld *world;unsigned calls,stops,play_word,stop_word;uint32_t offset,length;} Trace;
static void range_request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    Trace *t=context;assert(callback==NPE_SOUND_CALLBACK_MAP);++t->calls;t->play_word=t->world->music_enabled;
    t->offset=offset;t->length=(uint32_t)length;
}
static void section_request(void *context,unsigned section,int loop){
    Trace *t=context;assert(!section && !loop);++t->stops;t->stop_word=t->world->music_enabled;
}
int main(int argc,char **argv){
    assert(argc==3);Memory *m=calloc(1,sizeof(*m));assert(m);
    uint8_t *expected=malloc(sizeof(m->bytes));assert(expected);
    const unsigned count=sizeof(map_sound_native)/sizeof(*map_sound_native);
    unsigned native_ram=0,native_requests=0,world_cases=0;
    NpeFile f;battle_sound_file(argv[1],&f);NpeWorld w;
    assert(!npe_world_init(&f,&w,9,29,13));assert(!npe_world_resources_open(&f,&w));
    for(unsigned i=0;i<3;++i){
        uint8_t size[4],offset[4];assert(!npe_rpg_runtime_read(&f,0x5278f2+4*i,size,4));
        assert(!npe_rpg_runtime_read(&f,0x5278feu+4*i,offset,4));
        uint32_t s=0,o=0;for(unsigned j=0;j<4;++j){s=s*256+size[j];o=o*256+offset[j];}
        assert(s==f.sections[20+i].size && o==f.sections[20+i].offset);
    }
    for(unsigned i=0;i<count;++i){
        const MapSoundNative *n=&map_sound_native[i];memset(m,0,sizeof(*m));
        for(unsigned j=0;j<n->count;++j)write_word(m,0x520000+n->initial[j].address,n->initial[j].value);
        m->writes=0;NpeResidentHeap h={m,read_word,write_word,NULL};NpeMapSoundRequest r;
        assert(npe_map_sound_request(&h,&r)==1 && !m->writes);
        assert(r.filename==0x526eba && r.callback==0x31a3f4);
        assert(r.offset==read_long(m,0x5278feu+4*n->slot));
        assert(r.length==read_long(m,0x5278f2+4*n->slot));
        compare_request(n,&r);native_requests+=n->calls!=0;
        /* Replay itself writes no RAM. The captured controller/service slices
           do likewise; settings additionally writes mode/pending/music words. */
        if(n->operation==0 || n->operation==3){
            char path[1024];snprintf(path,sizeof(path),"%s/%s.ram.bin",argv[2],n->name);
            FILE *fp=fopen(path,"rb");assert(fp);
            assert(fread(expected,1,sizeof(m->bytes),fp)==sizeof(m->bytes));assert(fgetc(fp)==EOF);fclose(fp);
            if(memcmp(expected,m->bytes,sizeof(m->bytes)))fprintf(stderr,"RAM differs: %s\n",n->name);
            assert(!memcmp(expected,m->bytes,sizeof(m->bytes)));++native_ram;
        }
        /* Establish all four live input longs, including raw out-of-range
           slot aliases. No slot clamp and no substitution by directory ID. */
        uint8_t zero[4]={0};assert(!npe_rpg_runtime_write(&f,0x5278feu+4*n->slot,zero,4));
        assert(!npe_rpg_runtime_write(&f,0x5278f2+4*n->slot,zero,4));
        for(unsigned j=0;j<n->count;++j){
            uint8_t bytes[]={(uint8_t)(n->initial[j].value>>8),(uint8_t)n->initial[j].value};
            assert(!npe_rpg_runtime_write(&f,0x520000+n->initial[j].address,bytes,2));
        }
        assert(w.map_music_slot==n->slot && w.music_enabled==n->enabled);
        NpeMapSoundRequest live;assert(npe_rpg_runtime_map_sound(&f,&live)==1);
        assert(live.filename==r.filename && live.offset==r.offset && live.length==r.length && live.callback==r.callback);
        compare_request(n,&live);
        Trace trace={&w,0,0,0xdead,0xdead,0,0};w.sound_context=&trace;
        w.sound_observer=section_request;w.sound_range_observer=range_request;
        if(n->operation==0 && n->pending){
            /* Public resume is the already-dispatched request; the external
               controller pending flag is outside this API's scope. */
            npe_world_sound_map_resume(&w);++world_cases;
        }else if(n->operation==1 || n->operation==2){
            npe_world_music_set(&w,n->operation==1);++world_cases;
        }else continue;
        assert(trace.calls==n->calls && trace.stops==n->stops);
        assert(trace.play_word==n->play_word && trace.stop_word==n->stop_word);
        assert(w.music_enabled==n->after_word);
        if(trace.calls)assert(trace.offset==r.offset && trace.length==r.length);
        uint8_t pair[2];assert(!npe_rpg_runtime_read(&f,0x527a04,pair,2));
        assert(pair[0]==n->slot && pair[1]==0x81); /* Only the high byte is World-owned. */
    }
    assert(native_ram==17 && native_requests==17 && world_cases==25);
    npe_free(&f);free(m);free(expected);
    puts("29 original fixtures:17 full128KiB RAM and17 playback argument matches;29 live runtime requests;25 scoped world gates/settings order;3 map directory pairs");
    return 0;
}
