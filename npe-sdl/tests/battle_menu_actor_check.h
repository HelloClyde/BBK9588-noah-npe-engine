#ifdef NPE_BATTLE_MENU_ACTOR
#include <string.h>
#include "battle_menu_actor_native.h"
static void menu_actor_frame_check(const NpeWorld *world,const char *name,
                                  unsigned frame,unsigned route){
    const MenuActorCase *c=NULL;
    for(unsigned i=0;i<sizeof(menu_actor_cases)/sizeof(*menu_actor_cases);++i)
        if(!strcmp(name,menu_actor_cases[i].name)){c=&menu_actor_cases[i];break;}
    assert(world&&c&&frame<c->frames);
    uint8_t value=world->battle_actor,ram;
    assert(!npe_world_resident_read(world,UINT32_C(0x527a9e),&ram,1)&&ram==value);
    if(value!=c->actors[frame])fprintf(stderr,"%s commit%u menu actor C%u original%u\n",
        name,frame,value,c->actors[frame]);
    assert(value==c->actors[frame]);
    char path[180];snprintf(path,sizeof(path),"tmp/battle-menu-actor-c-%s-%s-commit-%u.bin",
        name,route?"main":"direct",frame);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(&value,1,1,out)==1&&!fclose(out));
}
#endif
