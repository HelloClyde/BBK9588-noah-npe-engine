#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cancel_step(void *context,const uint8_t *pixels,unsigned parameter){
    unsigned *frames=context;
    if(pixels){++*frames;return 0;}
    assert(parameter==15);return 1;
}
static int identical(const char *a,const char *b){
    FILE *fa=fopen(a,"rb"),*fb=fopen(b,"rb");assert(fa&&fb);
    int ca,cb;do{ca=fgetc(fa);cb=fgetc(fb);assert(ca==cb);}while(ca!=EOF);
    assert(!fclose(fa)&&!fclose(fb));return 1;
}
int main(int argc,char **argv){assert(argc==2);
    FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long size=ftell(in);
    assert(size>0&&!fseek(in,0,SEEK_SET));uint8_t *bytes=malloc((size_t)size);
    assert(bytes&&fread(bytes,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    NpeWorld world;NpeMap map,empty;assert(!npe_world_init(&file,&world,126,20,20));
    assert(!npe_map(&file,125,&map)&&!npe_map(&file,0,&empty));
    NpeScriptWalkCache event;assert(npe_world_script_walk_event(&world,257,&event)==1);
    assert(event.marker==1&&event.x==20&&event.y==12&&event.next_mode==5&&event.next_id==181);
    assert(!npe_world_script_walk_event(&world,256,&event)&&!npe_world_script_walk_event(&world,5,&event));
    NpeMapScriptWalkEvent raw;assert(npe_map_script_walk_event(&map,1,&raw)==1);
    uint8_t old[4];memcpy(old,raw.raw,4);((uint8_t *)raw.raw)[0]=0x7e;((uint8_t *)raw.raw)[3]=0xab;
    assert(npe_map_script_walk_event(&map,1,&raw)==1&&raw.x==20);
    assert(!npe_world_load_script_walk_events(&world,&map)&&world.script_walk_events[0].x==20);
    memcpy((uint8_t *)raw.raw,old,4);
    for(unsigned i=0;i<4;++i)world.script_walk_events[i]=(NpeScriptWalkCache){
        .x=(uint16_t)(400+i),.y=(uint16_t)(500+i),.next_id=(uint16_t)(600+i),.marker=(uint8_t)(70+i),.next_mode=24};
    NpeScriptWalkCache resident[4];memcpy(resident,world.script_walk_events,sizeof(resident));
    assert(!npe_world_load_script_walk_events(&world,&empty)&&!memcmp(resident,world.script_walk_events,sizeof(resident)));
    assert(!npe_world_load_script_walk_events(&world,&map));
    assert(world.script_walk_events[0].x==20&&!memcmp(resident+1,world.script_walk_events+1,3*sizeof(*resident)));
    unsigned count_index=652+9*3+2;uint8_t count=map.tail[count_index];
    ((uint8_t *)map.tail)[count_index]=5;NpeWorld before=world;
    assert(npe_world_load_script_walk_events(&world,&map)<0&&!memcmp(&before,&world,sizeof(world)));
    ((uint8_t *)map.tail)[count_index]=count;
    const char *a="tmp/script-walk-cache-a.npes",*b="tmp/script-walk-cache-b.npes";
    assert(!npe_world_save(&file,&world,a));NpeWorld changed=world;
    memcpy(changed.script_walk_events,resident,sizeof(resident));assert(!npe_world_save(&file,&changed,b)&&identical(a,b));
    NpeWorld cold;assert(!npe_world_load(&file,&cold,a));
    assert(cold.script_walk_events[0].x==20&&cold.script_walk_events[1].x==0);
    assert(!npe_world_restore(&file,&changed,a));
    assert(changed.script_walk_events[0].x==20&&!memcmp(changed.script_walk_events+1,resident+1,3*sizeof(*resident)));
    memcpy(resident,changed.script_walk_events,sizeof(resident));
    assert(!npe_world_restart_game(&file,&changed)&&!memcmp(changed.script_walk_events,resident,sizeof(resident)));
    assert(!remove(a)&&!remove(b));
    assert(!npe_world_init(&file,&world,1,43,2));world.game_mode=19;world.story_event_id=1;world.music_enabled=0;world.random_state=11;
    world.script_walk_events[0]=(NpeScriptWalkCache){.x=299,.y=1,.next_mode=1};before=world;
    assert(npe_world_apply_script_walk_event(&file,&world)==NPE_SCRIPT_WALK_NONTERMINATING&&!memcmp(&before,&world,sizeof(world)));
    world.script_walk_events[0].x=32811;world.script_walk_events[0].y=32769;
    assert(npe_world_apply_script_walk_event(&file,&world)==1&&world.x==43&&world.y==2&&world.direction==2&&world.game_mode==1&&world.random_state==11);
    world=before;world.script_walk_events[0]=(NpeScriptWalkCache){.x=46,.y=1,.next_mode=14,.next_id=1};
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];unsigned frames=0;
    assert(npe_world_script_walk_with_display(&file,&world,pixels,cancel_step,&frames)==-2);
    assert(frames==6&&world.x==44&&world.y==2&&world.game_mode==19&&world.story_progress==1&&world.map_init_pending);
    npe_free(&file);
    puts("Script walk four-slot cache: partial reload, reserved byte, source isolation, warm restore/restart, cache-independent NPESAV19, failure transaction, signed word/nonterminating numeric state and host cancellation passed");
    return 0;
}
