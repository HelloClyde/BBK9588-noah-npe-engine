#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void word(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
int main(int argc,char **argv){
    assert(argc==3);FILE *f=fopen(argv[1],"rb");assert(f&&!fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>0&&!fseek(f,0,SEEK_SET));uint8_t *data=malloc((size_t)n);
    assert(data&&fread(data,1,(size_t)n,f)==(size_t)n&&!fclose(f));
    NpeFile file;NpeWorld world,loaded;char error[128],path[512];
    assert(!npe_parse(&file,data,(size_t)n,error,sizeof(error))&&!npe_world_init(&file,&world,108,4,13));
    world.story_progress=115;assert(!npe_world_resources_open(&file,&world));
    snprintf(path,sizeof(path),"%s.canonical",argv[2]);assert(!npe_world_save(&file,&world,path));
    uint8_t records[192],actual[192];assert(!npe_world_resident_read(&world,0x52724e,records,192));
    word(records,115);word(records+2,1);word(records+32,11);records[62]=2;records[63]=0x5a;
    assert(!npe_world_resident_write(&world,0x52724e,records,192));
    snprintf(path,sizeof(path),"%s.metadata",argv[2]);assert(!npe_world_save(&file,&world,path));
    int meta=npe_world_load(&file,&loaded,path);unsigned diff=192;
    if(!meta){assert(!npe_world_resident_read(&loaded,0x52724e,actual,192));diff=0;for(unsigned i=0;i<192;++i)diff+=records[i]!=actual[i];}
    printf("metadata cold load=%d, complete192-byte differences=%u\n",meta,diff);
    records[62]=0;assert(!npe_world_resident_write(&world,0x52724e,records,192));
    assert(npe_world_update_tiles(&world)>0);
    snprintf(path,sizeof(path),"%s.grid",argv[2]);assert(!npe_world_save(&file,&world,path));
    int grid=npe_world_load(&file,&loaded,path);printf("modified live-grid cold load=%d\n",grid);
    npe_free(&file);return meta||diff||grid?1:0;
}
