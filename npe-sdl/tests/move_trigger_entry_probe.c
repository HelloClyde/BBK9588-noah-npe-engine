/* Compare the live C collision publication with independently captured
   original collision states. The current NPE is never modified. */
#include "../npe.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
int main(int argc,char **argv){
    assert(argc==3);char path[1024],error[128];unsigned map,x,y,progress,before,mode,event,flag;
    FILE *manifest=fopen(argv[1],"rb"),*config=fopen(argv[2],"rb");
    assert(manifest&&config&&fgets(path,sizeof(path),manifest));
    assert(fscanf(config,"%u %u %u %u %u %u %u %u",&map,&x,&y,&progress,&before,&mode,&event,&flag)==8);
    assert(!fclose(manifest)&&!fclose(config));path[strcspn(path,"\r\n")]=0;
    SDL_RWops *io=SDL_RWFromFile(path,"rb");assert(io);Sint64 length=SDL_RWsize(io);assert(length>0);
    uint8_t *bytes=malloc((size_t)length);assert(bytes);
    assert(SDL_RWread(io,bytes,1,(size_t)length)==(size_t)length&&!SDL_RWclose(io));
    NpeFile file;assert(!npe_parse(&file,bytes,(size_t)length,error,sizeof(error)));
    NpeWorld *world=malloc(sizeof(*world));assert(world);
    assert(!npe_world_init(&file,world,map,x,y));
    world->story_progress=(uint16_t)progress;world->map_init_pending=(uint16_t)before;
    world->game_mode=1;world->story_event_id=0xffff;world->defer_story_events=1;world->encounters_enabled=0;
    uint64_t fingerprint=npe_game_fingerprint(&file);
    int moved=npe_world_step(&file,world,0);
    int passed=moved==NPE_MOVE_EVENT&&world->game_mode==mode&&world->story_event_id==event&&world->map_init_pending==flag;
    printf("{\"passed\":%s,\"move\":%d,\"mode\":%u,\"event\":%u,\"map_init\":%u,\"before_flag\":%u}\n",
        passed?"true":"false",moved,world->game_mode,world->story_event_id,world->map_init_pending,before);
    assert(fingerprint==npe_game_fingerprint(&file));free(world);npe_free(&file);
    return passed?0:1;
}
