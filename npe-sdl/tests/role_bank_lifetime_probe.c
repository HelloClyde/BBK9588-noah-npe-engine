#include "../npe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    const char *route;unsigned salt,map;
    uint8_t before_battle[888],before_map[444],after_battle[888],after_map[444],saved_map[444];
} RoleLifetimeCase;
#include "role_bank_lifetime_native.h"
#include "title_role_boot_native.h"
static uint8_t *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");if(!f||fseek(f,0,SEEK_END))return NULL;
    long length=ftell(f);if(length<=0||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    uint8_t *data=malloc((size_t)length);
    if(!data||fread(data,1,(size_t)length,f)!=(size_t)length){free(data);fclose(f);return NULL;}
    fclose(f);*size=(size_t)length;return data;
}
static unsigned differences(const uint8_t *a,const uint8_t *b,unsigned size,const char *label){
    unsigned n=0;for(unsigned i=0;i<size;++i)if(a[i]!=b[i]){
        if(n<16)fprintf(stderr,"%s +%u original=%u C=%u\n",label,i,a[i],b[i]);
        ++n;
    }return n;
}
int main(int argc,char **argv){
    if(argc!=3)return 2;
    size_t size;uint8_t *data=read_file(argv[1],&size);NpeFile file;char error[128];
    if(!data||npe_parse(&file,data,size,error,sizeof(error)))return 2;
    uint64_t hash=npe_game_fingerprint(&file);unsigned failures=0,checks=0;
    for(unsigned bound=0;bound<2;++bound)for(unsigned i=0;i<sizeof(role_lifetime_cases)/sizeof(*role_lifetime_cases)+
            sizeof(title_role_boot_cases)/sizeof(*title_role_boot_cases);++i){
        const unsigned warm_count=sizeof(role_lifetime_cases)/sizeof(*role_lifetime_cases);
        const RoleLifetimeCase *c=i<warm_count?&role_lifetime_cases[i]:&title_role_boot_cases[i-warm_count];
        const int load=strstr(c->route,"load")!=NULL;NpeWorld world;
        if(npe_world_init(&file,&world,c->map,0,0))return 2;
        world.music_enabled=0;world.exit_choice=1;
        if(bound&&npe_world_resources_open(&file,&world))return 2;
        if(load){
            if(npe_world_resident_records_write(&world,0x5269c6,c->saved_map,444))return 2;
            world.game_mode=1;world.story_progress=1;
            if(npe_world_save(&file,&world,argv[2]))return 2;
        }
        if(npe_world_resident_records_write(&world,0x526450,c->before_battle,888)||
           npe_world_resident_records_write(&world,0x5269c6,c->before_map,444))return 2;
        world.battle_record_party_count=1;
        world.game_mode=!strcmp(c->route,"system-load")?6:7;
        world.story_progress=4;
        int result=load?npe_world_restore(&file,&world,argv[2]):npe_world_restart_game(&file,&world);
        if(result)return 2;
        uint8_t battle[888],map[444];
        if(bound){
            if(npe_rpg_runtime_read(&file,0x526450,battle,888)||
               npe_rpg_runtime_read(&file,0x5269c6,map,444))return 2;
        }else if(npe_world_resident_records_read(&world,0x526450,battle,888)||
                 npe_world_resident_records_read(&world,0x5269c6,map,444))return 2;
        unsigned bd=differences(c->after_battle,battle,888,"battle"),md=differences(c->after_map,map,444,"map");
        printf("%s salt%u bound%u battleDiff%u mapDiff%u\n",c->route,c->salt,bound,bd,md);
        failures+=bd||md;checks++;
        if(bound)npe_rpg_runtime_close(&file);
    }
    if(hash!=npe_game_fingerprint(&file))return 2;
    printf("%u role bank lifecycle comparisons, %u failures; own NPESAV22, NPE unchanged\n",checks,failures);
    npe_free(&file);return failures?1:0;
}
