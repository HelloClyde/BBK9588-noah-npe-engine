#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t ram[131072],*payload;
    size_t cursor,size;
    FILE *requests;
#ifdef NPE_TILE_RECORD_WORLD_BOUND
    NpeWorld *world;
#endif
} Memory;
static uint8_t *read_bytes(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");assert(f && !fseek(f,0,SEEK_END));
    long n=ftell(f);assert(n>=0 && !fseek(f,0,SEEK_SET));
    uint8_t *p=malloc((size_t)n+1);assert(p && fread(p,1,(size_t)n,f)==(size_t)n);
    fclose(f);*size=(size_t)n;return p;
}
static unsigned offset(uint32_t address){
    assert(address>=0x510000 && address<0x52ffff);return address-0x510000;
}
static uint16_t read_word(void *context,uint32_t address){
    Memory *m=context;unsigned i=offset(address);
#ifdef NPE_TILE_RECORD_WORLD_BOUND
    uint8_t bytes[2]={m->ram[i],m->ram[i+1]};
    if(m->world)for(unsigned j=0;j<2;++j)npe_world_resident_read(m->world,address+j,bytes+j,1);
    return (uint16_t)((unsigned)bytes[0]<<8|bytes[1]);
#else
    return (uint16_t)((unsigned)m->ram[i]<<8|m->ram[i+1]);
#endif
}
static void write_word(void *context,uint32_t address,uint16_t value){
    Memory *m=context;unsigned i=offset(address);m->ram[i]=(uint8_t)(value>>8);m->ram[i+1]=(uint8_t)value;
#ifdef NPE_TILE_RECORD_WORLD_BOUND
    if(m->world)for(unsigned j=0;j<2;++j)npe_world_resident_write(m->world,address+j,m->ram+i+j,1);
#endif
}
static void read_local(void *context,uint8_t *bytes,uint16_t count){
    Memory *m=context;assert(m->cursor+count<=m->size);
    fprintf(m->requests,"local %u %zu\n",count,m->cursor);
    memcpy(bytes,m->payload+m->cursor,count);m->cursor+=count;
}
static void read_guest(void *context,uint32_t address,uint16_t count){
    Memory *m=context;
    fprintf(m->requests,"%u %u %zu\n",address,count,m->cursor);
    size_t supplied=count<m->size-m->cursor?count:m->size-m->cursor;
    memcpy(m->ram+offset(address),m->payload+m->cursor,supplied);m->cursor+=supplied;
#ifdef NPE_TILE_RECORD_WORLD_BOUND
    if(m->world)for(unsigned j=0;j<supplied;++j)npe_world_resident_write(m->world,address+j,m->ram+offset(address)+j,1);
#endif
}
int main(int argc,char **argv){
    assert(argc==6 || argc==8);
    size_t size,after_size;
    uint8_t *before=read_bytes(argv[1],&size),*after=read_bytes(argv[2],&after_size);
    assert(size==131072 && after_size==size);
    Memory *m=calloc(1,sizeof(*m));assert(m);memcpy(m->ram,before,size);
    m->payload=read_bytes(argv[3],&m->size);
    m->requests=fopen(argv[5],"wb");assert(m->requests);
    NpeBattleBitmapService service={{m,read_word,write_word,NULL},m,NULL,read_local,read_guest,NULL};
    int grid=!strcmp(argv[4],"grid");
    assert(!(grid?npe_map_grid_stream_service(&service):
        npe_map_tile_record_service(&service,(uint8_t)strtoul(argv[4],NULL,10))));
    assert(m->cursor==m->size);
    for(unsigned i=0;i<size;++i)if(m->ram[i]!=after[i]){
        fprintf(stderr,"RAM mismatch at %06x: C=%02x original=%02x\n",0x510000+i,m->ram[i],after[i]);return 1;
    }
    fclose(m->requests);
#ifdef NPE_TILE_RECORD_WORLD_BOUND
    {
        size_t npe_size;uint8_t *data=read_bytes("tmp/tjfy-world-probe.npe",&npe_size);
        NpeFile file;NpeWorld world;char error[128];
        assert(!npe_parse(&file,data,npe_size,error,sizeof(error))&&!npe_world_init(&file,&world,1,7,10));
        memcpy(m->ram,before,131072);m->cursor=0;m->world=&world;
        for(unsigned i=0;i<131072;++i)npe_world_resident_write(&world,0x510000+i,before+i,1);
        for(unsigned i=0;i<131072;i+=2)assert(read_word(m,0x510000+i)==(uint16_t)((unsigned)before[i]<<8|before[i+1]));
        char path[512];assert(snprintf(path,sizeof(path),"%s.bound",argv[5])<(int)sizeof(path));
        m->requests=fopen(path,"wb");assert(m->requests);
        assert(!(grid?npe_map_grid_stream_service(&service):
            npe_map_tile_record_service(&service,(uint8_t)strtoul(argv[4],NULL,10))));
        assert(m->cursor==m->size&&!fclose(m->requests));
        for(unsigned i=0;i<131072;i+=2){
            uint16_t actual=read_word(m,0x510000+i),expected=(uint16_t)((unsigned)after[i]<<8|after[i+1]);
            if(actual!=expected){fprintf(stderr,"World RAM mismatch %06x C=%04x original=%04x\n",0x510000+i,actual,expected);return 1;}
        }
        uint8_t records[192];assert(!npe_world_resident_read(&world,0x52724e,records,192));
        assert(!memcmp(records,after+0x1724e,192));
        m->world=NULL;npe_free(&file);
        puts("all World bindings: initial and final128KiB RAM plus192 terrain bytes matched");
    }
#endif
    if(argc==8){
        size_t file_size;uint8_t *bytes=read_bytes(argv[6],&file_size);
        NpeFile file;NpeMap map;char error[128];unsigned number=(unsigned)strtoul(argv[7],NULL,10);
        assert(!npe_parse(&file,bytes,file_size,error,sizeof(error)));
        assert(!npe_map(&file,number-1,&map));
        assert(!npe_rpg_runtime_open_idle(&file,NULL));
        assert(!npe_rpg_runtime_write(&file,0x510000,before,131072));
        uint16_t progress=(uint16_t)((unsigned)before[0x17a66]<<8|before[0x17a67]);
        assert(!(grid?npe_rpg_runtime_map_grid(&file,&map):
            npe_rpg_runtime_map_tile_records(&file,&map,progress)));
        assert(!npe_rpg_runtime_read(&file,0x510000,m->ram,131072));
        assert(!memcmp(m->ram,after,131072));
        assert(npe_rpg_runtime_cursor(&file)==(uint32_t)((grid?map.tile_words:map.tail)-file.bytes)+
            (grid?0u:703u)+m->size);
        npe_free(&file);
    }
    printf("full 128KiB RAM and %zu file bytes matched%s\n",m->cursor,argc==8?"; production runtime matched":"");
    free(before);free(after);free(m->payload);free(m);return 0;
}
