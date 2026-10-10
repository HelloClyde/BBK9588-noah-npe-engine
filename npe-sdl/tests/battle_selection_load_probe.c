#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[8],after[8];} SelectionLoadCase;
#include "battle_selection_load_native.h"
static const uint32_t addresses[]={0x52792e,0x52792f,0x527930,0x527931,0x527932,0x527933,0x527a9a,0x527af0};
static void put(NpeWorld *w,const uint8_t bytes[8]){
    for(unsigned i=0;i<8;++i)assert(!npe_world_resident_write(w,addresses[i],bytes+i,1));
}
static void compare(const NpeWorld *w,const uint8_t bytes[8]){
    for(unsigned i=0;i<8;++i){uint8_t byte;assert(!npe_world_resident_read(w,addresses[i],&byte,1)&&byte==bytes[i]);}
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<16;++i){const SelectionLoadCase *c=&selection_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));put(&w,c->before);
        assert(!npe_world_load_battle_events(&w,&map)&&!npe_world_load_random_enemy_records(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));for(unsigned j=0;j<8;++j){uint8_t byte;assert(!npe_rpg_runtime_read(&f,addresses[j],&byte,1)&&byte==c->after[j]);}
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"%s map%u:eight original selector bytes retained\n",c->entry,c->map);
    }
    NpeWorld w,cold;assert(!npe_world_new_game(&f,&w));
    const char *base="tmp/battle-selection-own-base.dat",*altered="tmp/battle-selection-own-altered.dat";
    assert(!npe_world_save(&f,&w,base));put(&w,selection_load_cases[0].before);assert(!npe_world_save(&f,&w,altered));
    uint8_t *a,*b;size_t as,bs;assert(!read_file(base,&a,&as)&&!read_file(altered,&b,&bs)&&as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,base));compare(&w,selection_load_cases[0].after);
    assert(!npe_world_restart_game(&f,&w));compare(&w,selection_load_cases[14].after);
    assert(!npe_world_load(&f,&cold,base));const uint8_t zero[8]={0};compare(&cold,zero);
    assert(!remove(base)&&!remove(altered));assert(npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("16 original loads/128 selector bytes;warm restore/restart retain;NPESAV22 unchanged and cold transient state passed");return 0;
}
