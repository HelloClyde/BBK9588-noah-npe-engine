#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NPE_RESOURCE_ENDING
#include "rpg_ending_native.h"
#elif defined(NPE_STATUS_RESOURCE_CYCLE)
#include "battle_status_resource_native.h"
#else
#include "rpg_resource_lifecycle_native.h"
#endif

static uint8_t memory[131072],expected[131072],trace[18];
static NpeFile file;
static uint32_t cursor;
static unsigned reads,seeks,failures;
static const RpgResourceCase *current;
static uint16_t read_word(void *context,uint32_t address){
    (void)context;assert(address>=0x510000&&address+1<0x530000);
    unsigned p=address-0x510000;return (uint16_t)((unsigned)memory[p]<<8|memory[p+1]);
}
static void write_word(void *context,uint32_t address,uint16_t value){
    (void)context;assert(address>=0x510000&&address+1<0x530000);
    unsigned p=address-0x510000;memory[p]=(uint8_t)(value>>8);memory[p+1]=(uint8_t)value;
}
static void write_long(uint32_t address,uint32_t value){
    write_word(NULL,address,(uint16_t)(value>>16));write_word(NULL,address+2,(uint16_t)value);
}
static void record_word(unsigned offset,uint16_t value){
    assert(offset+1<sizeof(trace));trace[offset]=(uint8_t)(value>>8);trace[offset+1]=(uint8_t)value;
}
static void record_long(unsigned offset,uint32_t value){
    record_word(offset,(uint16_t)(value>>16));record_word(offset+2,(uint16_t)value);
}
static void failed(void *context,uint16_t request){
    (void)context;assert(request==32768);record_word(16,(uint16_t)++failures);
}
static void owner_failed(void *context){(void)context;assert(0);}
static void seek(void *context,uint32_t position){
    (void)context;cursor=position;record_long(0,cursor);record_word(4,(uint16_t)++seeks);
}
static void read_guest(void *context,uint32_t address,uint16_t count){
    (void)context;unsigned supplied=count;
    if(supplied>current->cap)supplied=current->cap;
    if(cursor>=file.size)supplied=0;
    else if(supplied>file.size-cursor)supplied=(unsigned)(file.size-cursor);
    record_word(6,(uint16_t)++reads);record_word(8,count);record_word(10,(uint16_t)supplied);record_long(12,address);
    assert(address>=0x510000&&address+supplied<=0x530000);
    if(supplied)memcpy(memory+address-0x510000,file.bytes+cursor,supplied);
    cursor+=supplied;record_long(0,cursor);
}
static void load(const char *path,uint8_t *bytes,size_t size){
    FILE *input=fopen(path,"rb");assert(input);
    assert(fread(bytes,1,size,input)==size&&fgetc(input)==EOF);fclose(input);
}
int main(int argc,char **argv){
    assert(argc==2);FILE *input=fopen(argv[1],"rb");assert(input);
    assert(!fseek(input,0,SEEK_END));long size=ftell(input);assert(size>0);
    uint8_t *bytes=malloc((size_t)size);assert(bytes);rewind(input);
    assert(fread(bytes,1,(size_t)size,input)==(size_t)size);fclose(input);
    char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    const NpeResidentHeap heap={NULL,read_word,write_word,failed};
    const NpeResidentHeapOwnerService owners={heap,NULL,owner_failed};
    const NpeRpgResourceService resources={heap,NULL,seek,read_guest};
    for(unsigned c=0;c<sizeof(rpg_resource_cases)/sizeof(*rpg_resource_cases);++c){
        current=&rpg_resource_cases[c];memset(memory,0xa5,sizeof(memory));memset(trace,0,sizeof(trace));
        cursor=0;reads=seeks=failures=0;
        npe_resident_heap_initialize(&heap);assert(npe_resident_heap_launcher_buffers(&owners)==1);
        assert(!npe_rpg_map_buffer_allocate(&heap));
        write_long(0x5279be,file.sections[9].size+(current->high_length?65536u:0));
        write_long(0x5279c2,file.sections[9].offset);
        if(current->no_handles)for(uint32_t a=0x5127ea;a<0x51280a;a+=2)write_word(NULL,a,65535);
        assert(!npe_rpg_battle_resources_allocate(&resources));
#ifdef NPE_RESOURCE_ENDING
        write_word(NULL,0x527aa4,(uint16_t)current->flags);
        if(current->zero_map)write_word(NULL,0x527a6a,0);
        if(current->zero_battle)write_word(NULL,0x527a88,0);
        write_word(NULL,0x527934,21);write_word(NULL,0x527936,0xbeef);write_word(NULL,0x527ab4,0);
        for(unsigned i=0;i<current->repeats;++i)assert(!npe_rpg_finish_game_service(&heap));
        if(current->reallocate)assert(!npe_rpg_map_buffer_allocate(&heap));
#endif
#ifdef NPE_STATUS_RESOURCE_CYCLE
        for(unsigned cycle=0;cycle<current->cycles;++cycle){
            assert(!npe_rpg_battle_resources_release(&heap));
            assert(read_word(NULL,0x527aa4)==0x00a5);
            assert(!npe_rpg_battle_resources_allocate(&resources));
        }
        if(current->released)assert(!npe_rpg_battle_resources_release(&heap));
#endif
        if(current->cleanup){
            if(current->cleanup==2){
                static const uint32_t handles[]={0x52574c,0x52574e,0x525760,0x525626};
                for(unsigned i=0;i<4;++i)write_word(NULL,handles[i],0);
            }
            assert(!npe_resident_heap_launcher_cleanup(&heap));
        }
        load(current->ram,expected,sizeof(expected));
        for(unsigned i=0;i<sizeof(memory);++i)if(memory[i]!=expected[i]){
            fprintf(stderr,"%s at%06x: C%02x original%02x\n",current->name,0x510000+i,memory[i],expected[i]);assert(0);
        }
        uint8_t native_trace[18];load(current->trace,native_trace,sizeof(native_trace));
        assert(!memcmp(trace,native_trace,sizeof(trace)));
#if defined(NPE_STATUS_RESOURCE_CYCLE) || defined(NPE_RESOURCE_ENDING)
        const char *prefix=
#ifdef NPE_RESOURCE_ENDING
            "tmp/ending-return/c-";
#else
            "tmp/battle-status-resource-c-";
#endif
        char path[180];snprintf(path,sizeof(path),"%s%s-ram.bin",prefix,current->name);
        FILE *out=fopen(path,"wb");assert(out&&fwrite(memory,1,sizeof(memory),out)==sizeof(memory)&&!fclose(out));
        snprintf(path,sizeof(path),"%s%s-trace.bin",prefix,current->name);
        out=fopen(path,"wb");assert(out&&fwrite(trace,1,sizeof(trace),out)==sizeof(trace)&&!fclose(out));
#endif
        /* Complete RAM already includes the released or reallocated pointer. */
#ifdef NPE_RESOURCE_ENDING
        assert(read_word(NULL,0x527a6a)==(current->zero_map?0:5));
        assert(read_word(NULL,0x527a88)==(current->zero_battle?0:6));
        assert(read_word(NULL,0x527aa4)==current->flags);
        assert(read_word(NULL,0x527934)==7&&read_word(NULL,0x527936)==0xbeef&&read_word(NULL,0x527ab4)==1);
#else
        assert(read_word(NULL,0x527a6a)==5);
        assert(read_word(NULL,0x527a88)==(current->no_handles?0:6));
#ifdef NPE_STATUS_RESOURCE_CYCLE
        assert(read_word(NULL,0x527aa4)==(current->released?0x00a5:0x01a5));
#else
        assert(read_word(NULL,0x527aa4)==0x01a5);
#endif
#endif
        if(current->cleanup==1){
            assert(read_word(NULL,0x52574c)==1&&read_word(NULL,0x52574e)==2);
            assert(read_word(NULL,0x525760)==3&&read_word(NULL,0x525626)==4);
            assert(read_word(NULL,NPE_RESIDENT_HEAP_BITMAP)==0x8600);
        }
        fprintf(stderr,"Original ending resource service %s: complete128KiB RAM/18-byte IO trace match, no captured heap headers\n",current->name);
    }
    assert(npe_rpg_map_buffer_allocate(NULL)==-1&&npe_resident_heap_launcher_cleanup(NULL)==-1);
    assert(npe_rpg_battle_resources_allocate(NULL)==-1&&npe_rpg_battle_packed_load(NULL)==-1);
    assert(npe_rpg_resources_release(NULL)==-1);
    npe_free(&file);
    fprintf(stderr,"Ending resource service:%u original allocation/read/cleanup cases verified\n",
            (unsigned)(sizeof(rpg_resource_cases)/sizeof(*rpg_resource_cases)));return 0;
}
