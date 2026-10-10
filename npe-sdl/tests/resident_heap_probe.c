#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "resident_heap_native.h"

/* Independent guest RAM adapter. Include the descriptor/limit fields below
   the arena; words preserve the original high-byte/low-byte guest order. */
static uint8_t memory[0x20000];
static unsigned failures;
static uint16_t failed_request;
static uint16_t read_word(void *context,uint32_t address){
    (void)context;assert(address>=0x510000 && address+1<0x530000);
    unsigned p=address-0x510000;return (uint16_t)((unsigned)memory[p]<<8|memory[p+1]);
}
static void write_word(void *context,uint32_t address,uint16_t value){
    (void)context;assert(address>=0x510000 && address+1<0x530000);
    unsigned p=address-0x510000;memory[p]=(uint8_t)(value>>8);memory[p+1]=(uint8_t)value;
}
static void failed(void *context,uint16_t request){(void)context;++failures;failed_request=request;}
static NpeResidentHeap heap={NULL,read_word,write_word,failed};
static uint32_t read_long(uint32_t address){return (uint32_t)read_word(NULL,address)<<16|read_word(NULL,address+2);}
static void write_long(uint32_t address,uint32_t value){write_word(NULL,address,(uint16_t)(value>>16));write_word(NULL,address+2,(uint16_t)value);}
static void reset(void){memset(memory,0xa5,sizeof(memory));failures=0;failed_request=0;npe_resident_heap_initialize(&heap);}
int main(void){
    const uint32_t head=NPE_RESIDENT_HEAP_HEAD,limit=NPE_RESIDENT_HEAP_LIMIT,bitmap=NPE_RESIDENT_HEAP_BITMAP;
    for(unsigned i=0;i<sizeof(resident_heap_native_cases)/sizeof(*resident_heap_native_cases);++i){
        const ResidentHeapNativeCase *sample=&resident_heap_native_cases[i];reset();
        uint16_t handle=npe_resident_heap_allocate(&heap,sample->request);
        uint32_t pointer=npe_resident_heap_lock(&heap,handle);
        assert(handle==sample->handle && pointer==sample->pointer);
        assert(read_word(NULL,head)==sample->flags && read_long(head+2)==sample->next);
        assert(read_word(NULL,bitmap)==sample->bitmap && !failures);
        fprintf(stderr,"Original heap request%u: handle%u pointer%06x next%06x flags%04x bitmap%04x\n",
            sample->request,handle,(unsigned)pointer,(unsigned)read_long(head+2),read_word(NULL,head),read_word(NULL,bitmap));
    }
    /* Collapse the preceding occupied nodes to one equivalent occupied
       node. This is a source-rule check for the captured tail, not a dump
       of the complete original allocator state or a game replay. */
    for(unsigned i=0;i<sizeof(resident_heap_tail_cases)/sizeof(*resident_heap_tail_cases);++i){
        const ResidentHeapTailCase *sample=&resident_heap_tail_cases[i];reset();
        write_word(NULL,head,0x8001);write_long(head+2,sample->pointer-6);
        write_word(NULL,sample->pointer-6,sample->flags);write_long(sample->pointer-4,0);
        write_word(NULL,bitmap,sample->bitmap);write_long(limit,sample->limit);
        assert(!npe_resident_heap_allocate(&heap,sample->request));
        assert(failures==1 && failed_request==(uint16_t)(sample->request+(sample->request&1u)));
        assert(npe_resident_heap_lock(&heap,0)==sample->pointer);
        assert(read_word(NULL,bitmap)==sample->bitmap);
        fprintf(stderr,"Equivalent captured tail request%u: failed allocation, lock0=%06x\n",sample->request,(unsigned)sample->pointer);
    }
    reset();assert(read_long(limit)==0x523a40 && read_word(NULL,bitmap)==0x8000);
    assert(read_word(NULL,head)==0 && read_long(head+2)==0);
    assert(memory[0x4a3f]==0xa5 && memory[0x13a40]==0xa5);
    assert(npe_resident_heap_allocate(&heap,3)==1);
    assert(read_word(NULL,head)==1 && read_long(head+2)==head+10);
    assert(npe_resident_heap_lock(&heap,1)==0x514a46 && read_word(NULL,head)==0x8001);
    assert(npe_resident_heap_allocate(&heap,2)==2);
    assert(npe_resident_heap_lock(&heap,2)==head+16 && read_long(head+12)==head+18);
    assert(read_word(NULL,bitmap)==0xe000 && !failures);
    assert(!npe_resident_heap_lock(&heap,255));

    reset();assert(npe_resident_heap_allocate(&heap,35329)==1);
    assert(npe_resident_heap_lock(&heap,1)==head+6);
    assert(read_long(head+2)==0x51d448 && read_word(NULL,head)==0x8001);

    reset();assert(npe_resident_heap_allocate(&heap,65535)==1);
    assert(read_long(head+2)==head+6 && npe_resident_heap_lock(&heap,1)==head+6);
    assert(!failures); /* Word rounding wraps to0; no host size rejection. */

    reset();write_long(limit,head+10);
    assert(npe_resident_heap_allocate(&heap,3)==1 && !read_long(head+2));
    assert(!npe_resident_heap_allocate(&heap,1) && failures==1 && failed_request==2);
    assert(!npe_resident_heap_lock(&heap,0));

    reset();write_long(head+2,head+20);write_word(NULL,head+20,8);
    write_long(head+22,head+46);write_word(NULL,head+46,0);write_long(head+48,0);
    write_word(NULL,bitmap,0x8080); /* Reserve descriptor8 as the existing node does. */
    assert(npe_resident_heap_allocate(&heap,9)==1);
    assert(read_word(NULL,head)==0 && npe_resident_heap_lock(&heap,1)==head+52);
    assert(read_long(head+48)==head+62); /* Small hole skipped, later hole split. */

    reset();write_long(head+2,read_long(limit)+2);
    assert(!npe_resident_heap_allocate(&heap,5) && failed_request==6);
    assert(!read_word(NULL,head) && read_word(NULL,bitmap)==0x8000);

    reset();for(unsigned i=0;i<16;++i)write_word(NULL,bitmap+2*i,0xffff);
    assert(!npe_resident_heap_allocate(&heap,3) && failures==1);
    assert(npe_resident_heap_lock(&heap,0)==head+6 && read_word(NULL,head)==0x8000);
    assert(!npe_resident_heap_allocate(&heap,1)); /* Locked zero is not an allocatable free node. */

    reset();write_word(NULL,head,0x4004);write_long(head+2,0);
    assert(npe_resident_heap_lock(&heap,4)==head+6 && read_word(NULL,head)==0xc004);
    assert(!npe_resident_heap_lock(&heap,0));
    fprintf(stderr,"Resident heap: initialization, split/skip, lock0, capacity, bitmap and word wrap verified\n");
    return 0;
}
