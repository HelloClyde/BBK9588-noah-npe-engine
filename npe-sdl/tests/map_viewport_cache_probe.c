#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {uint8_t ram[131072];unsigned reads,writes;} Memory;
static uint8_t *load(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");assert(f && !fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>=0 && !fseek(f,0,SEEK_SET));uint8_t *bytes=malloc((size_t)n+1);
    assert(bytes && fread(bytes,1,(size_t)n,f)==(size_t)n);fclose(f);*size=(size_t)n;return bytes;
}
static unsigned offset(uint32_t address){assert(address>=0x510000 && address<0x52ffff);return address-0x510000;}
static uint16_t read_word(void *context,uint32_t address){
    Memory *m=context;unsigned i=offset(address);++m->reads;
    return (uint16_t)((unsigned)m->ram[i]<<8|m->ram[i+1]);
}
static void write_word(void *context,uint32_t address,uint16_t word){
    Memory *m=context;unsigned i=offset(address);++m->writes;
    m->ram[i]=(uint8_t)(word>>8);m->ram[i+1]=(uint8_t)word;
}
int main(int argc,char **argv){
    assert(argc==4);size_t n,b,file_size;
    uint8_t *before=load(argv[1],&n),*after=load(argv[2],&b);assert(n==131072 && b==n);
    Memory *m=calloc(1,sizeof(*m));assert(m);memcpy(m->ram,before,n);
    NpeResidentHeap heap={m,read_word,write_word,NULL};assert(!npe_map_viewport_cache_refresh(&heap));
    assert(m->writes==105);
    for(unsigned i=0;i<n;++i)if(m->ram[i]!=after[i]){
        fprintf(stderr,"RAM mismatch %06x: C=%02x original=%02x\n",0x510000+i,m->ram[i],after[i]);return 1;
    }
    uint8_t *bytes=load(argv[3],&file_size);NpeFile file;char error[128];
    assert(!npe_parse(&file,bytes,file_size,error,sizeof(error)));
    assert(!npe_rpg_runtime_open_idle(&file,NULL));
    assert(!npe_rpg_runtime_write(&file,0x510000,before,131072));
    unsigned reads=npe_rpg_runtime_reads(&file);uint32_t cursor=npe_rpg_runtime_cursor(&file);
    assert(!npe_rpg_runtime_viewport_refresh(&file));
    assert(!npe_rpg_runtime_read(&file,0x510000,m->ram,131072));assert(!memcmp(m->ram,after,131072));
    assert(reads==npe_rpg_runtime_reads(&file) && cursor==npe_rpg_runtime_cursor(&file));
    printf("original full 128KiB RAM matched;105 cache writes; production runtime matched\n");
    npe_free(&file);free(before);free(after);free(m);return 0;
}
