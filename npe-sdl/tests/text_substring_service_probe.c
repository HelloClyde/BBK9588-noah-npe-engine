#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {uint8_t ram[131072];unsigned writes;} Memory;
static uint8_t *load(const char *name,size_t *size){
    FILE *f=fopen(name,"rb");assert(f&&!fseek(f,0,SEEK_END));long length=ftell(f);
    assert(length>=0&&!fseek(f,0,SEEK_SET));uint8_t *p=malloc((size_t)length+1);
    assert(p&&fread(p,1,(size_t)length,f)==(size_t)length&&!fclose(f));*size=(size_t)length;return p;
}
static unsigned offset(uint32_t a){assert(a>=0x510000&&a<0x52ffff);return a-0x510000;}
static uint16_t read_word(void *context,uint32_t a){Memory *m=context;unsigned i=offset(a);return (uint16_t)((unsigned)m->ram[i]<<8|m->ram[i+1]);}
static void write_word(void *context,uint32_t a,uint16_t v){Memory *m=context;unsigned i=offset(a);m->ram[i]=(uint8_t)(v>>8);m->ram[i+1]=(uint8_t)v;++m->writes;}
static int resident_read(void *context,uint32_t a,uint8_t *p,unsigned n){return npe_world_resident_read(context,a,p,n);}
static int resident_write(void *context,uint32_t a,const uint8_t *p,unsigned n){return npe_world_resident_write(context,a,p,n);}
static void compare(const uint8_t *actual,const uint8_t *expected,const char *stage){
    unsigned n=0;for(unsigned i=0;i<131072;++i)if(actual[i]!=expected[i]){
        if(n<16)fprintf(stderr,"%s %06x original=%02x C=%02x\n",stage,0x510000+i,expected[i],actual[i]);
        ++n;
    }assert(!n);
}
int main(int argc,char **argv){
    assert(argc==9);size_t size,other,npe_size;
    uint8_t *before=load(argv[1],&size),*after=load(argv[2],&other);assert(size==131072&&other==size);
    uint32_t source=(uint32_t)strtoul(argv[4],NULL,0),destination=(uint32_t)strtoul(argv[5],NULL,0);
    uint16_t start=(uint16_t)strtoul(argv[6],NULL,0),count=(uint16_t)strtoul(argv[7],NULL,0),expected=(uint16_t)strtoul(argv[8],NULL,0);
    Memory *memory=calloc(1,sizeof(*memory));assert(memory);memcpy(memory->ram,before,size);
    NpeResidentHeap heap={memory,read_word,write_word,NULL};
    uint16_t result=npe_text_substring_service(&heap,destination,source,start,count);assert(result==expected);compare(memory->ram,after,"flat service");
    uint8_t *data=load(argv[3],&npe_size);NpeFile file;NpeWorld world;char error[128];
    assert(!npe_parse(&file,data,npe_size,error,sizeof(error))&&!npe_world_init(&file,&world,1,7,10));
    NpeResidentBindings bindings={&world,resident_read,resident_write};
    assert(!npe_rpg_runtime_open_idle(&file,&bindings)&&!npe_rpg_runtime_map_load(&file,&world,0));
    assert(!npe_rpg_runtime_write(&file,0x510000,before,131072));
    assert(!npe_rpg_runtime_read(&file,0x510000,memory->ram,131072));compare(memory->ram,before,"initial import");
    uint32_t cursor=npe_rpg_runtime_cursor(&file);unsigned reads=npe_rpg_runtime_reads(&file);
    assert(!npe_rpg_runtime_text_substring(&file,destination,source,start,count,&result)&&result==expected);
    assert(!npe_rpg_runtime_read(&file,0x510000,memory->ram,131072));compare(memory->ram,after,"World bindings");
    assert(cursor==npe_rpg_runtime_cursor(&file)&&reads==npe_rpg_runtime_reads(&file));
    printf("substring start%u count%u result%u: two complete128KiB RAM comparisons, cursor/IO unchanged\n",start,count,result);
    npe_free(&file);free(before);free(after);free(memory);return 0;
}
