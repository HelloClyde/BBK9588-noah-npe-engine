#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    const char *route;unsigned salt,map,progress;
    uint8_t before[192],after[192];
} TerrainLifetimeCase;
#include "terrain_record_lifetime_native.h"
static uint8_t *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");assert(f&&!fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>0&&!fseek(f,0,SEEK_SET));uint8_t *bytes=malloc((size_t)n);
    assert(bytes&&fread(bytes,1,(size_t)n,f)==(size_t)n&&!fclose(f));*size=(size_t)n;return bytes;
}
static unsigned differences(const uint8_t *a,const uint8_t *b){
    unsigned count=0;
    for(unsigned i=0;i<192;++i)if(a[i]!=b[i]){
        if(count<8)fprintf(stderr,"terrain +%u original=%u C=%u\n",i,a[i],b[i]);
        ++count;
    }return count;
}
int main(int argc,char **argv){
    assert(argc==3);size_t size;uint8_t *bytes=read_file(argv[1],&size);
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,size,error,sizeof(error)));
    uint64_t hash=npe_game_fingerprint(&file);unsigned failures=0,checks=0;
    for(unsigned bound=0;bound<2;++bound)
        for(unsigned i=0;i<sizeof(terrain_lifetime_cases)/sizeof(*terrain_lifetime_cases);++i){
            const TerrainLifetimeCase *c=&terrain_lifetime_cases[i];NpeWorld world;
            assert(!npe_world_init(&file,&world,c->map,0,0));world.exit_choice=1;world.music_enabled=0;
            if(bound)assert(!npe_world_resources_open(&file,&world));
            const int load=strstr(c->route,"load")!=NULL;
            if(load){world.story_progress=(uint16_t)c->progress;assert(!npe_world_save(&file,&world,argv[2]));}
            int seed=bound?npe_rpg_runtime_write(&file,0x52724e,c->before,192):
                           npe_world_resident_write(&world,0x52724e,c->before,192);
            ++checks;
            if(seed){fprintf(stderr,"%s salt%u bound%u missing World terrain owner\n",c->route,c->salt,bound);++failures;continue;}
            world.game_mode=load?6:7;world.story_progress=4;
            assert(!(load?npe_world_restore(&file,&world,argv[2]):npe_world_restart_game(&file,&world)));
            uint8_t actual[192],resident[192];
            int owner=npe_world_resident_read(&world,0x52724e,resident,192);
            if(owner){fprintf(stderr,"%s salt%u bound%u missing World terrain owner\n",c->route,c->salt,bound);++failures;}
            else{
                if(bound){assert(!npe_rpg_runtime_read(&file,0x52724e,actual,192));assert(!memcmp(actual,resident,192));}
                else memcpy(actual,resident,192);
                unsigned diff=differences(c->after,actual);failures+=diff!=0;
                printf("%s salt%u bound%u terrainDiff%u\n",c->route,c->salt,bound,diff);
            }
            if(bound)npe_rpg_runtime_close(&file);
        }
    assert(hash==npe_game_fingerprint(&file));npe_free(&file);
    printf("%u original terrain lifecycle comparisons, %u failures; own NPESAV22 unchanged\n",checks,failures);
    return failures?1:0;
}
