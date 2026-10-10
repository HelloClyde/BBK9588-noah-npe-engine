#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
static int app_open(NpeFile *file,NpeWorld *world);
static void app_present(SDL_Renderer *renderer);
static void app_free(NpeFile *file);
#define npe_world_resources_open app_open
#define SDL_RenderPresent app_present
#define npe_free app_free
#define main npe_app_main
#include "../main.c"
#undef main
#undef npe_free
#undef SDL_RenderPresent
#undef npe_world_resources_open
#include "rpg_shutdown_native.h"

typedef struct {const char *name;unsigned flags;int title,battle;} AppCase;
static const AppCase app_cases[]={
    {"title-inactive",166,1,0},{"title-active",422,1,0},
    {"title-two",678,1,0},{"title-high129",33190,1,0},
    {"title-high255",65446,1,0},{"map-inactive",166,0,0},
    {"map-active",422,0,0},{"scripted-active",422,0,1}
};
static const AppCase *app_case;
static NpeFile *app_file;
static NpeWorld *app_world;
static unsigned frames,frees;
static uint16_t word(const NpeFile *file,uint32_t address){
    uint8_t bytes[2];assert(!npe_rpg_runtime_read(file,address,bytes,2));
    return (uint16_t)((unsigned)bytes[0]<<8|bytes[1]);
}
static void put(const NpeFile *file,uint32_t address,unsigned value){
    uint8_t bytes[2]={(uint8_t)(value>>8),(uint8_t)value};
    assert(!npe_rpg_runtime_write(file,address,bytes,2));
}
static uint16_t cleared(unsigned flags){return (uint16_t)(flags>>8==1?flags&255:flags);}
static void persist(const char *route,const char *name,const char *phase,const void *data,size_t size){
    char path[200];snprintf(path,sizeof(path),"tmp/rpg-shutdown-%s-%s-%s.bin",route,name,phase);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(data,1,size,out)==size&&!fclose(out));
}
static void snapshot(const NpeFile *file,uint8_t bytes[46]){
    assert(!npe_rpg_runtime_read(file,0x527aa4,bytes,2));
    assert(!npe_rpg_runtime_read(file,0x527a6a,bytes+2,2));
    assert(!npe_rpg_runtime_read(file,0x527a88,bytes+4,2));
    assert(!npe_rpg_runtime_read(file,0x52797e,bytes+6,4));
    assert(!npe_rpg_runtime_read(file,0x5279a6,bytes+10,4));
    assert(!npe_rpg_runtime_read(file,NPE_RESIDENT_HEAP_BITMAP,bytes+14,32));
}
static void closed(const char *route,const char *name,NpeFile *file,NpeWorld *world,unsigned flags){
    assert(!file->rpg_runtime&&world->battle_resource_flags==cleared(flags));
    uint8_t result[2]={(uint8_t)(world->battle_resource_flags>>8),(uint8_t)world->battle_resource_flags};
    persist(route,name,"closed",result,2);
}
static int app_open(NpeFile *file,NpeWorld *world){
    assert(app_case&&!app_file);int result=npe_world_resources_open(file,world);if(result)return result;
    app_file=file;app_world=world;
    if(app_case->battle){
        world->game_mode=3;world->story_event_id=1;
        assert(npe_world_prepare_scripted_battle(file,world)==1);
        assert(world->battle_enemy_count>0&&world->battle_party_flags_saved);
    }else if(app_case->flags>>8)assert(!npe_world_battle_resources(file,world,1));
    put(file,0x527aa4,app_case->flags);assert(world->battle_resource_flags==app_case->flags);
    return 0;
}
static void app_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);assert(app_file&&app_world);++frames;
    SDL_Event event={0};event.type=SDL_QUIT;assert(SDL_PushEvent(&event)==1);
}
static void app_free(NpeFile *file){
    assert(file==app_file&&app_world&&frames&&!frees);
    uint8_t before[46];snapshot(file,before);
    assert(word(file,0x527aa4)==app_case->flags);
    persist("main",app_case->name,"before",before,sizeof(before));
    npe_free(file);closed("main",app_case->name,file,app_world,app_case->flags);++frees;
    uint8_t result[4]={(uint8_t)(frames>>8),(uint8_t)frames,0,(uint8_t)frees};
    persist("main",app_case->name,"events",result,4);
    app_file=NULL;app_world=NULL;
}
int main(int argc,char **argv){
    assert(argc==2);assert(npe_rpg_runtime_shutdown(NULL)==-1);
    NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeWorld world;
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(npe_rpg_runtime_shutdown(&file)==-1);
    for(unsigned c=0;c<sizeof(rpg_resource_cases)/sizeof(*rpg_resource_cases);++c){
        const RpgResourceCase *test=&rpg_resource_cases[c];
        for(unsigned explicit_shutdown=0;explicit_shutdown<2;++explicit_shutdown){
            const char *route=explicit_shutdown?"runtime":"close";
            memset(&world,0,sizeof(world));world.game_mode=7;
            assert(!npe_world_resources_open(&file,&world));
            assert(!npe_world_battle_resources(&file,&world,1));put(&file,0x527aa4,test->flags);
            if(test->zero_map)put(&file,0x527a6a,0);
            if(test->zero_battle)put(&file,0x527a88,0);
            uint8_t before[46],after[46];snapshot(&file,before);
            persist(route,test->name,"before",before,sizeof(before));
            if(explicit_shutdown){
                for(unsigned i=0;i<test->repeats;++i)assert(!npe_rpg_runtime_shutdown(&file));
                snapshot(&file,after);assert(!memcmp(before+2,after+2,12));
                assert(word(&file,0x527aa4)==cleared(test->flags));
                assert(world.battle_resource_flags==cleared(test->flags));
                persist(route,test->name,"after",after,sizeof(after));
            }
            assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
            closed(route,test->name,&file,&world,test->flags);
            npe_rpg_runtime_close(&file);assert(npe_rpg_runtime_shutdown(&file)==-1);
        }
    }
    npe_free(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(unsigned c=0;c<sizeof(app_cases)/sizeof(*app_cases);++c){
        app_case=&app_cases[c];frames=frees=0;
        char *title_args[]={argv[0],argv[1],NULL};
        char *map_args[]={argv[0],"--walk-preview","1,1,1",argv[1],NULL};
        assert(!npe_app_main(app_case->title?2:4,app_case->title?title_args:map_args));
        assert(frees==1&&frames>0&&!app_file&&!app_world);
        fprintf(stderr,"Actual main SDL quit:%s, frames%u, original exact-one resource flag observed\n",app_case->name,frames);
    }
    fprintf(stderr,"Shutdown runtime:9 explicit/9 close paths; actual main:8 SDL quit paths verified\n");
    return 0;
}
