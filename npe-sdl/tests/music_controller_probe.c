#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_sound_file.h"
typedef struct {const char *name;unsigned operation,count;struct {uint16_t address,value;} initial[24];
    unsigned calls,stops;uint16_t arguments[12],observed[3];} MusicControlNative;
#include "music_controller_native.h"
typedef struct {uint8_t bytes[131072];} Memory;
static uint16_t word(void *context,uint32_t a){Memory *m=context;assert(a>=0x510000&&a+1<0x530000);a-=0x510000;return (uint16_t)((unsigned)m->bytes[a]*256+m->bytes[a+1]);}
static void put(void *context,uint32_t a,uint16_t v){Memory *m=context;assert(a>=0x510000&&a+1<0x530000);a-=0x510000;m->bytes[a]=(uint8_t)(v>>8);m->bytes[a+1]=(uint8_t)v;}
typedef struct {NpeResidentHeap heap;NpeWorld *world;NpeFile *file;unsigned calls,stops;uint16_t args[12],observed[3];} Trace;
static void record(Trace *t,const NpeMapSoundRequest *r){
    ++t->calls;uint16_t a[]={0x52,0x6eba,(uint16_t)(r->offset>>16),(uint16_t)r->offset,
        (uint16_t)(r->length>>16),(uint16_t)r->length,0,0,0,0,(uint16_t)(r->callback>>16),(uint16_t)r->callback};
    memcpy(t->args,a,sizeof(a));
}
static void observe_words(Trace *t){
    if(t->world){t->observed[0]=t->world->music_enabled;t->observed[1]=t->world->game_mode;t->observed[2]=t->world->map_init_pending;}
    else {t->observed[0]=t->heap.read_word(t->heap.context,0x527a50);t->observed[1]=t->heap.read_word(t->heap.context,0x527934);t->observed[2]=t->heap.read_word(t->heap.context,0x527ab4);}
}
static void control_event(void *context,unsigned event){
    Trace *t=context;observe_words(t);
    if(event==2){++t->stops;return;}
    assert(event==1);NpeMapSoundRequest r;
    assert((t->file?npe_rpg_runtime_map_sound(t->file,&r):npe_map_sound_request(&t->heap,&r))==1);record(t,&r);
}
static void range_request(void *context,uint32_t offset,int32_t length,uint32_t callback){
    Trace *t=context;observe_words(t);NpeMapSoundRequest r={0x526eba,offset,(uint32_t)length,callback};record(t,&r);
}
static void stop_request(void *context,unsigned section,int loop){Trace *t=context;assert(!section&&!loop);observe_words(t);++t->stops;}
static void init_trace(Trace *t){t->calls=t->stops=0;for(unsigned i=0;i<12;++i)t->args[i]=0xdead;for(unsigned i=0;i<3;++i)t->observed[i]=0xdead;}
static void compare(const Trace *t,const MusicControlNative *c){assert(t->calls==c->calls&&t->stops==c->stops);assert(!memcmp(t->args,c->arguments,sizeof(t->args)));assert(!memcmp(t->observed,c->observed,sizeof(t->observed)));}
static void seed(NpeFile *f,const MusicControlNative *c){for(unsigned i=0;i<c->count;++i){uint16_t v=c->initial[i].value;uint8_t b[]={(uint8_t)(v>>8),(uint8_t)v};assert(!npe_rpg_runtime_write(f,0x520000+c->initial[i].address,b,2));}}
int main(int argc,char **argv){
    assert(argc==3);Memory *m=calloc(1,sizeof(*m));uint8_t *native=malloc(131072);assert(m&&native);
    NpeFile f;battle_sound_file(argv[1],&f);NpeWorld w;assert(!npe_world_init(&f,&w,9,29,13)&&!npe_world_resources_open(&f,&w));
    const uint32_t addresses[]={0x527934,0x527a50,0x527a54,0x527a8a,0x527ab4,0x527936};
    for(unsigned i=0;i<66;++i){const MusicControlNative *c=&music_control_native[i];memset(m,0,sizeof(*m));
        for(unsigned j=0;j<c->count;++j)put(m,0x520000+c->initial[j].address,c->initial[j].value);
        Trace t={.heap={m,word,put,NULL}};init_trace(&t);unsigned pending=word(m,0x527ab4);
        assert(npe_music_controller_apply(&t.heap,c->operation,control_event,&t)==(c->operation!=2||pending!=0));compare(&t,c);
        char path[1024];snprintf(path,sizeof(path),"%s/%s.ram.bin",argv[2],c->name);FILE *fp=fopen(path,"rb");assert(fp);
        assert(fread(native,1,131072,fp)==131072&&fgetc(fp)==EOF&&!fclose(fp));assert(!memcmp(native,m->bytes,131072));
        seed(&f,c);t=(Trace){.file=&f,.world=&w};init_trace(&t);
        assert(npe_rpg_runtime_music_controller(&f,c->operation,control_event,&t)==(c->operation!=2||pending!=0));compare(&t,c);
        for(unsigned a=0;a<sizeof(addresses)/sizeof(*addresses);++a){uint8_t b[2];assert(!npe_rpg_runtime_read(&f,addresses[a],b,2));assert(!memcmp(b,native+addresses[a]-0x510000,2));}
        seed(&f,c);init_trace(&t);w.sound_observer=stop_request;w.sound_range_observer=range_request;w.sound_context=&t;
        uint32_t rng=w.random_state;assert(npe_world_music_controller(&w,c->operation)==(c->operation!=2||pending!=0));compare(&t,c);assert(w.random_state==rng);
        for(unsigned a=0;a<sizeof(addresses)/sizeof(*addresses);++a){uint8_t b[2];assert(!npe_rpg_runtime_read(&f,addresses[a],b,2));assert(!memcmp(b,native+addresses[a]-0x510000,2));}
    }
    /* Complete word and independent byte writes; neighbours remain RAM-owned. */
    uint8_t mode[]={0x81,0x82},pending[]={0xaa,0xbb},adjacent[]={0x93,0x94};
    assert(!npe_rpg_runtime_write(&f,0x527934,mode,2)&&w.game_mode==0x8182);
    assert(!npe_rpg_runtime_write(&f,0x527ab4,pending,2)&&w.map_init_pending==0xaabb);
    assert(!npe_rpg_runtime_write(&f,0x527935,adjacent,1)&&w.game_mode==0x8193);
    assert(!npe_rpg_runtime_write(&f,0x527ab4,adjacent+1,1)&&w.map_init_pending==0x94bb);
    unsigned bound=0;for(uint32_t a=0x510000;a<0x530000;++a){uint8_t b;if(!npe_world_resident_read(&w,a,&b,1))++bound;}assert(bound==NPE_WORLD_RESIDENT_BYTES);
    npe_free(&f);free(m);free(native);puts("66 original controller slices/full128KiB RAM/request ordering,runtime/World fields,full words/byte neighbours,3748 bindings passed");return 0;
}
