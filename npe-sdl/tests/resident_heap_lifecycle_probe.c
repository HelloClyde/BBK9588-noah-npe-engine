#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "resident_heap_lifecycle_native.h"

static uint8_t memory[0x20000],expected[NPE_RESIDENT_HEAP_BYTES];
static uint16_t read_word(void *context,uint32_t address){
    (void)context;assert(address>=0x510000 && address+1<0x530000);
    unsigned p=address-0x510000;return (uint16_t)((unsigned)memory[p]<<8|memory[p+1]);
}
static void write_word(void *context,uint32_t address,uint16_t value){
    (void)context;assert(address>=0x510000 && address+1<0x530000);
    unsigned p=address-0x510000;memory[p]=(uint8_t)(value>>8);memory[p+1]=(uint8_t)value;
}
static void compare(const char *name,const char *path,uint32_t address,unsigned size){
    FILE *file=fopen(path,"rb");assert(file);
    assert(fread(expected,1,size,file)==size && fgetc(file)==EOF);fclose(file);
    for(unsigned i=0;i<size;++i)if(memory[address-0x510000+i]!=expected[i]){
        fprintf(stderr,"%s at%06x: C%02x original%02x\n",name,(unsigned)(address+i),
            memory[address-0x510000+i],expected[i]);assert(0);
    }
}
int main(void){
    const NpeResidentHeap heap={NULL,read_word,write_word,NULL};
    for(unsigned i=0;i<sizeof(heap_lifecycle_cases)/sizeof(*heap_lifecycle_cases);++i){
        const HeapLifecycleCase *c=&heap_lifecycle_cases[i];
        memset(memory,0xa5,sizeof(memory));npe_resident_heap_initialize(&heap);
        for(unsigned j=0;j<c->word_count;++j)write_word(NULL,c->words[j].address,c->words[j].value);
        for(unsigned j=0;j<c->operation_count;++j){
            const HeapOperation *op=&c->operations[j];
            if(op->kind==0)npe_resident_heap_unlock(&heap,op->handle);
            else if(op->kind==1)npe_resident_heap_release(&heap,op->handle);
            else npe_resident_heap_compact(&heap);
        }
        compare(c->name,c->files[0],NPE_RESIDENT_HEAP_HEAD,NPE_RESIDENT_HEAP_BYTES);
        compare(c->name,c->files[1],NPE_RESIDENT_HEAP_BITMAP,32);
        compare(c->name,c->files[2],NPE_RESIDENT_HEAP_LIMIT,4);
        fprintf(stderr,"Original lifecycle %s: full%u-byte arena, bitmap and limit match\n",c->name,NPE_RESIDENT_HEAP_BYTES);
    }
    fprintf(stderr,"Resident heap lifecycle:%u independent original service cases verified\n",
        (unsigned)(sizeof(heap_lifecycle_cases)/sizeof(*heap_lifecycle_cases)));
    return 0;
}
