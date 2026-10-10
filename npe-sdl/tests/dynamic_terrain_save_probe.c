#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");assert(f&&!fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>=0&&!fseek(f,0,SEEK_SET));uint8_t *p=malloc((size_t)n+1);
    assert(p&&fread(p,1,(size_t)n,f)==(size_t)n&&!fclose(f));*size=(size_t)n;return p;
}
static void write_file(const char *path,const uint8_t *data,size_t size){
    FILE *f=fopen(path,"wb");assert(f&&fwrite(data,1,size,f)==size&&!fclose(f));
}
static void word(uint8_t *p,unsigned value){p[0]=(uint8_t)(value>>8);p[1]=(uint8_t)value;}
static void checksum(uint8_t *data,size_t size){
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=36;i<size;++i){h^=data[i];h*=UINT64_C(1099511628211);}
    for(unsigned i=0;i<4;++i)data[24+i]=(uint8_t)((size-36)>>(8*i));
    for(unsigned i=0;i<8;++i)data[28+i]=(uint8_t)(h>>(8*i));
}
static void check(const NpeFile *f,const NpeWorld *w,const NpeMap *map,const uint16_t *grid,unsigned count,const uint8_t records[192]){
    (void)f;
    uint8_t actual[192];assert(!npe_world_resident_read(w,0x52724e,actual,192)&&!memcmp(actual,records,192));
    for(unsigned i=0;i<count;++i)assert(npe_world_tile(w,map,i%map->width,i/map->width)==grid[i]);
}
int main(int argc,char **argv){
    assert(argc==3);size_t size;uint8_t *data=read_file(argv[1],&size);NpeFile f;NpeMap map;char error[128],path[512];
    NpeWorld *w=calloc(1,sizeof(*w)),*cold=calloc(1,sizeof(*cold)),*prior=malloc(sizeof(*prior));assert(w&&cold&&prior);
    assert(!npe_parse(&f,data,size,error,sizeof(error))&&!npe_world_init(&f,w,108,4,13)&&!npe_map(&f,107,&map));
    uint64_t fingerprint=npe_game_fingerprint(&f);w->story_progress=115;w->music_enabled=0;w->exit_choice=1;
    assert(!npe_world_resources_open(&f,w));
    snprintf(path,sizeof(path),"%s.canonical",argv[2]);assert(!npe_world_save(&f,w,path));
    size_t prefix;uint8_t *canonical=read_file(path,&prefix);assert(!memcmp(canonical,"NPESAV22",8));free(canonical);
    unsigned count=map.width*map.height;assert(count>225&&count<NPE_MAP_SNAPSHOT_WORDS);
    uint16_t *grid=malloc(count*sizeof(*grid));assert(grid);
    uint8_t records[192],pixels[NPE_WIDTH*NPE_HEIGHT],decoded[NPE_WIDTH*NPE_HEIGHT];
    /* Metadata alone needs the extension, including non-Boolean flags/tails. */
    assert(!npe_world_resident_read(w,0x52724e,records,192));word(records,114);records[62]=2;records[63]=0xa5;
    assert(!npe_world_resident_write(w,0x52724e,records,192));
    snprintf(path,sizeof(path),"%s.metadata",argv[2]);assert(!npe_world_save(&f,w,path));
    assert(!npe_world_load(&f,cold,path)&&!memcmp(cold->tile_record_bytes,records,192));
    /* Five native updates write225 distinct cells, beyond the old53 shadow. */
    for(unsigned round=0;round<5;++round){
        memset(records,0,192);
        for(unsigned slot=0;slot<3;++slot){word(records+64*slot,1);
            for(unsigned j=0;j<15;++j){word(records+64*slot+2+2*j,1+45*round+15*slot+j);word(records+64*slot+32+2*j,j%3==0?1:j%3==1?10:11);}
            records[64*slot+63]=(uint8_t)(0x80+round+slot);
        }
        assert(!npe_world_resident_write(w,0x52724e,records,192));assert(npe_world_update_tiles(w)==3);
    }
    assert(w->tile_override_count==NPE_TILE_OVERRIDES);
    /* Direct grid writes have no projection observer and must still persist. */
    assert(npe_rpg_runtime_map_tile_write(&f,108,(uint16_t)(count-1),10)==1);
    for(unsigned i=0;i<count;++i)assert(npe_rpg_runtime_map_tile(&f,108,(uint16_t)i,grid+i)==1);
    assert(!npe_world_resident_read(w,0x52724e,records,192)&&!npe_render_world_load(&f,w,pixels));
    assert(!npe_world_save(&f,w,argv[2]));
    uint8_t *wire=read_file(argv[2],&size);assert(!memcmp(wire,"NPESAV23",8)&&size==prefix+196+2*count);
    snprintf(path,sizeof(path),"%s.pixels",argv[2]);write_file(path,pixels,sizeof(pixels));
    assert(!npe_world_load(&f,cold,argv[2]));check(&f,cold,&map,grid,count,records);
    assert(!npe_render_world_load(&f,cold,decoded)&&!memcmp(decoded,pixels,sizeof(pixels)));
    snprintf(path,sizeof(path),"%s.cold-resave",argv[2]);assert(!npe_world_save(&f,cold,path));
    size_t other;uint8_t *again=read_file(path,&other);assert(other==size&&!memcmp(wire,again,size));free(again);
    /* Warm restore preserves live battle/RNG session state while restoring terrain. */
    uint8_t battle[888],after[888];for(unsigned i=0;i<888;++i)battle[i]=(uint8_t)(i*37+19);
    assert(!npe_world_resident_records_write(w,0x526450,battle,888));w->random_state=0xdeadbeef;
    assert(npe_rpg_runtime_map_tile_write(&f,108,1,999)==1);
    assert(!npe_world_restore(&f,w,argv[2])&&w->random_state==0xdeadbeef);
    assert(!npe_world_resident_records_read(w,0x526450,after,888)&&!memcmp(battle,after,888));check(&f,w,&map,grid,count,records);
    npe_rpg_runtime_close(&f);assert(!npe_world_load(&f,w,argv[2])&&!npe_world_resources_open(&f,w));
    check(&f,w,&map,grid,count,records);assert(!npe_render_world_load(&f,w,decoded)&&!memcmp(decoded,pixels,sizeof(pixels)));
    /* Structural failures remain transactional even when their checksums match. */
    uint8_t *bad=malloc(size+1);assert(bad);
    for(unsigned kind=0;kind<4;++kind){
        memcpy(bad,wire,size);size_t length=size;
        if(kind==0)bad[prefix+2]^=1;
        else if(kind==1)--length;
        else if(kind==2)bad[length++]=0x5a;
        else bad[16]^=1;
        checksum(bad,length);snprintf(path,sizeof(path),"%s.bad-%u",argv[2],kind);write_file(path,bad,length);
        *prior=*cold;assert(npe_world_load(&f,cold,path)==-1&&!memcmp(prior,cold,sizeof(*prior)));
    }
    assert(fingerprint==npe_game_fingerprint(&f));npe_free(&f);free(w);free(cold);free(prior);free(grid);free(wire);free(bad);
    printf("own V23:192 metadata bytes,225 native cell updates plus unobserved write,%u grid words,cold/warm/resources/render,resave,V22 and4 transactional failures passed\n",count);
    return 0;
}
