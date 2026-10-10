#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    unsigned index,word_index,a,b,failure_mode,failure_event;
    uint32_t directory;
    uint8_t npc[250],conditions[200];
} NpcLifetimeState;
typedef struct {const char *route;unsigned salt;NpcLifetimeState before,after;} NpcLifetimeCase;
#include "npc_condition_lifetime_native.h"
static uint8_t *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");assert(f&&!fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>0&&!fseek(f,0,SEEK_SET));uint8_t *bytes=malloc((size_t)n);
    assert(bytes&&fread(bytes,1,(size_t)n,f)==(size_t)n&&!fclose(f));*size=(size_t)n;return bytes;
}
static void seed(NpeWorld *w,const NpcLifetimeState *s){
    w->npc_guard_index=(uint16_t)s->index;w->npc_guard_word_index=(uint16_t)s->word_index;
    w->npc_guard_a=(uint8_t)s->a;w->npc_guard_b=(uint8_t)s->b;
    memcpy(w->npc_state,s->npc,250);memcpy(w->condition5_bytes,s->conditions,200);
}
static int compare(const NpeWorld *w,const NpcLifetimeState *s){
    return w->npc_guard_index!=s->index||w->npc_guard_word_index!=s->word_index||
        w->npc_guard_a!=s->a||w->npc_guard_b!=s->b||memcmp(w->npc_state,s->npc,250)||
        memcmp(w->condition5_bytes,s->conditions,200);
}
int main(int argc,char **argv){
    assert(argc==3);size_t size;uint8_t *bytes=read_file(argv[1],&size);NpeFile file;char error[128];
    assert(!npe_parse(&file,bytes,size,error,sizeof(error)));unsigned failures=0,checks=0;
    for(unsigned bound=0;bound<2;++bound)for(unsigned i=0;i<sizeof(npc_lifetime_cases)/sizeof(*npc_lifetime_cases);++i){
        const NpcLifetimeCase *c=npc_lifetime_cases+i;NpeWorld w;
        assert(!npe_world_init(&file,&w,1,0,0));w.music_enabled=0;w.exit_choice=1;
        if(bound)assert(!npe_world_resources_open(&file,&w));
        const int load=!strcmp(c->route,"title-load");
        /* The independently sampled original slot contains zero NPC/condition arrays. */
        if(load)assert(!npe_world_save(&file,&w,argv[2]));
        seed(&w,&c->before);
        if(bound){uint8_t mode=(uint8_t)c->before.failure_mode,event[2]={(uint8_t)(c->before.failure_event>>8),(uint8_t)c->before.failure_event};
            assert(!npe_rpg_runtime_write(&file,0x527a08,&mode,1)&&!npe_rpg_runtime_write(&file,0x527a80,event,2));}
        w.game_mode=7;assert(!(load?npe_world_restore(&file,&w,argv[2]):npe_world_restart_game(&file,&w)));
        unsigned different=(unsigned)compare(&w,&c->after);++checks;
        if(bound){uint8_t b[4];assert(!npe_rpg_runtime_read(&file,0x527a08,b,1));different+=b[0]!=c->after.failure_mode;
            assert(!npe_rpg_runtime_read(&file,0x527a80,b,2));different+=((unsigned)b[0]*256+b[1])!=c->after.failure_event;
            assert(!npe_rpg_runtime_read(&file,0x5279f2,b,4));different+=((uint32_t)b[0]<<24|(uint32_t)b[1]<<16|(uint32_t)b[2]<<8|b[3])!=c->after.directory;
            uint8_t npc[250],conditions[200];assert(!npe_rpg_runtime_read(&file,0x526fba,npc,250)&&!npe_rpg_runtime_read(&file,0x527186,conditions,200));
            assert(!memcmp(npc,w.npc_state,250)&&!memcmp(conditions,w.condition5_bytes,200));}
        printf("%s salt%u bound%u differences%u\n",c->route,c->salt,bound,different);failures+=different!=0;
        if(bound)npe_rpg_runtime_close(&file);
    }
    /* Session guards do not enter own slots; cold load supplies zero guards. */
    NpeWorld plain,seeded,cold;assert(!npe_world_init(&file,&plain,1,0,0));seeded=plain;
    unsigned bound_bytes=0;uint8_t byte;
    for(uint32_t address=0x510000;address<0x550000;++address)
        bound_bytes+=!npe_world_resident_read(&plain,address,&byte,1);
    assert(bound_bytes==NPE_WORLD_RESIDENT_BYTES);
    seeded.npc_guard_index=65535;seeded.npc_guard_word_index=0x8123;seeded.npc_guard_a=255;seeded.npc_guard_b=1;
    char guard_path[512];int path_size=snprintf(guard_path,sizeof(guard_path),"%s.guards",argv[2]);
    assert(path_size>0&&(size_t)path_size<sizeof(guard_path));
    assert(!npe_world_save(&file,&plain,argv[2])&&!npe_world_save(&file,&seeded,guard_path));
    size_t an,bn;uint8_t *a=read_file(argv[2],&an),*b=read_file(guard_path,&bn);
    assert(an==bn&&!memcmp(a,b,an));assert(!npe_world_load(&file,&cold,guard_path));
    assert(!cold.npc_guard_index&&!cold.npc_guard_word_index&&!cold.npc_guard_a&&!cold.npc_guard_b);
    free(a);free(b);npe_free(&file);
    printf("%u original warm lifecycle comparisons, %u failures; %u bound bytes, %zu unchanged own save bytes and cold guard initialization passed\n",checks,failures,bound_bytes,an);
    return failures?1:0;
}
