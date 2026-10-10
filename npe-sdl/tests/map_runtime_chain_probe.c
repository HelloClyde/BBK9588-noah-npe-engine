#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *load(const char *name,size_t *size){
    FILE *f=fopen(name,"rb");assert(f && !fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>0 && !fseek(f,0,SEEK_SET));uint8_t *p=malloc((size_t)n);
    assert(p && fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*size=(size_t)n;return p;
}
static void door(const NpeFile *file,const NpeWorld *world,unsigned width,uint16_t value){
    uint16_t tile;assert(npe_rpg_runtime_map_tile(file,108,(uint16_t)(8*width+12),&tile)==1 && tile==value);
    NpeMap map;assert(!npe_map(file,107,&map));assert(npe_world_tile(world,&map,12,8)==value);
}
int main(int argc,char **argv){
    assert(argc==3);size_t size;uint8_t *bytes=load(argv[1],&size);
    NpeFile file;NpeMap map;NpeWorld world;char error[128];uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    assert(!npe_parse(&file,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&file);
    assert(!npe_world_init(&file,&world,108,4,13));world.story_progress=115;
    assert(!npe_world_resources_open(&file,&world));assert(!npe_map(&file,107,&map));
    door(&file,&world,map.width,196);
    world.story_progress=116;assert(npe_world_update_tiles(&world)>0);
    door(&file,&world,map.width,10);assert(!npe_render_world(&file,&world,pixels));
    uint8_t flag;assert(!npe_rpg_runtime_read(&file,0x52728c,&flag,1) && flag==1);
    assert(!npe_world_save(&file,&world,argv[2]));
    assert(npe_rpg_runtime_map_tile_write(&file,108,(uint16_t)(8*map.width+12),999)==1);
    assert(!npe_world_restore(&file,&world,argv[2]));door(&file,&world,map.width,10);
    assert(!npe_rpg_runtime_read(&file,0x52728c,&flag,1) && flag==1);
    assert(!npe_render_world(&file,&world,pixels));
    assert(!npe_world_restart_game(&file,&world));assert(world.map_number==1);
    uint16_t tile;assert(npe_rpg_runtime_map_tile(&file,108,0,&tile)==0);
    assert(!npe_render_world(&file,&world,pixels));
    npe_rpg_runtime_close(&file);
    assert(!npe_world_load(&file,&world,argv[2]));
    assert(!npe_world_resources_open(&file,&world));assert(!npe_rpg_runtime_map_load(&file,&world,1));
    door(&file,&world,map.width,10);assert(!npe_rpg_runtime_read(&file,0x52728c,&flag,1) && flag==1);
    assert(!npe_render_world(&file,&world,pixels));assert(fingerprint==npe_game_fingerprint(&file));
    printf("live map heap,door progression,warm/cold own-save and restart rendering passed; NPE unchanged\n");
    npe_free(&file);return 0;
}
