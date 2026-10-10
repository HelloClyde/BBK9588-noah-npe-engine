#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "resident_heap_owner_native.h"

static uint8_t memory[131072],expected[131072];
static unsigned heap_failures,owner_failures;
static uint16_t read_word(void *context,uint32_t address){
    (void)context;assert(address>=0x510000&&address+1<0x530000);
    unsigned p=address-0x510000;return (uint16_t)((unsigned)memory[p]<<8|memory[p+1]);
}
static void write_word(void *context,uint32_t address,uint16_t value){
    (void)context;assert(address>=0x510000&&address+1<0x530000);
    unsigned p=address-0x510000;memory[p]=(uint8_t)(value>>8);memory[p+1]=(uint8_t)value;
}
static void heap_failed(void *context,uint16_t request){
    (void)context;assert(request==256);++heap_failures;
}
static void owner_failed(void *context){(void)context;++owner_failures;}
static void load(const char *path,uint8_t *bytes,size_t size){
    FILE *file=fopen(path,"rb");assert(file);
    assert(fread(bytes,1,size,file)==size&&fgetc(file)==EOF);fclose(file);
}
int main(void){
    const NpeResidentHeapOwnerService service={{NULL,read_word,write_word,heap_failed},NULL,owner_failed};
    for(unsigned c=0;c<sizeof(heap_owner_cases)/sizeof(*heap_owner_cases);++c){
        const HeapOwnerCase *current=&heap_owner_cases[c];
        heap_failures=owner_failures=0;memset(memory,0xa5,sizeof(memory));
        npe_resident_heap_initialize(&service.heap);
        if(current->no_handles)for(uint32_t a=0x5127ea;a<0x51280a;a+=2)write_word(NULL,a,65535);
        if(current->no_space){write_word(NULL,0x5129a0,0x51);write_word(NULL,0x5129a2,0x4a50);}
        int result=current->prefix?npe_resident_heap_launcher_buffers(&service):
            npe_resident_heap_allocate_owned(&service,0x52574c,current->request,0x525186);
        uint8_t trace[6];load(current->trace,trace,sizeof(trace));
        assert(heap_failures==trace[1]&&owner_failures==trace[3]&&result==trace[5]);
        load(current->ram,expected,sizeof(expected));
        for(unsigned i=0;i<sizeof(memory);++i)if(memory[i]!=expected[i]){
            fprintf(stderr,"%s at%06x: C%02x original%02x\n",current->name,0x510000+i,memory[i],expected[i]);assert(0);
        }
        if(current->prefix){
            static const uint32_t h[]={0x52574c,0x52574e,0x525760,0x525626};
            for(unsigned i=0;i<4;++i){
                assert(read_word(NULL,h[i])==i+1);
                assert(read_word(NULL,0x525186+4*i)==0x51);
                assert(read_word(NULL,0x525188+4*i)==0x4a46+262*i);
                assert(read_word(NULL,0x514a40+262*i)==0x8001+i);
            }
        }else if(!result){
            assert(!read_word(NULL,0x52574c));
            assert(read_word(NULL,0x525186)==0xa5a5&&read_word(NULL,0x525188)==0xa5a5);
        }
        fprintf(stderr,"Original resource owner %s: full128KiB RAM, live handle/pointer and both failure boundaries match\n",current->name);
    }
    assert(npe_resident_heap_allocate_owned(NULL,0,0,0)==-1);
    fprintf(stderr,"Launcher prefix: four original256-byte owners allocated and locked without seeding captured addresses\n");
    return 0;
}
