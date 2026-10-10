#ifdef NPE_BATTLE_EXECUTION_STATE
#include <string.h>
#include "battle_execution_native.h"
static const ExecutionCase *execution_case(const char *name){
    for(unsigned i=0;i<sizeof(execution_cases)/sizeof(*execution_cases);++i)
        if(!strcmp(name,execution_cases[i].name))return &execution_cases[i];
    return NULL;
}
static void execution_state_check(const NpeWorld *w,const char *name,
                                  unsigned route,unsigned phase){
    const ExecutionCase *c=execution_case(name);if(!c)return;
    assert(phase<c->phases);
    const uint8_t state[]={w->battle_action_ready,w->battle_execution_slot};
    if(memcmp(state,c->state[phase],2))fprintf(stderr,"%s phase%u ready/slot C%u/%u native%u/%u\n",
        name,phase,state[0],state[1],c->state[phase][0],c->state[phase][1]);
    assert(!memcmp(state,c->state[phase],2));
    const uint32_t addresses[]={0x527aa8,0x527acc};
    for(unsigned i=0;i<2;++i){uint8_t b;assert(!npe_world_resident_read(w,addresses[i],&b,1)&&b==state[i]);}
    char path[180];snprintf(path,sizeof(path),"tmp/battle-execution-c-%s-%s-state-%u.bin",name,route?"main":"direct",phase);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(state,1,2,out)==2&&!fclose(out));
}
static void execution_frame_check(const NpeWorld *w,const char *name,unsigned frame,
                                  unsigned round,unsigned group){
    const ExecutionCase *c=execution_case(name);assert(w);
    /* Four legacy enemy paths have pixel references but no new control
       capture; their existing full pixel/numeric assertions still run. */
    if(!c)return;
    unsigned index=round*28+frame;assert(index<c->frames);
    (void)group;
    const uint8_t state[]={w->battle_action_ready,w->battle_execution_slot};
    if(memcmp(state,c->lcd[index],2))fprintf(stderr,"%s commit%u ready/slot C%u/%u original service sequence%u/%u\n",
        name,index,state[0],state[1],c->lcd[index][0],c->lcd[index][1]);
    assert(!memcmp(state,c->lcd[index],2));
    char path[180];snprintf(path,sizeof(path),"tmp/battle-execution-c-%s-action-%u.bin",name,index);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(state,1,2,out)==2&&!fclose(out));
}
#endif
