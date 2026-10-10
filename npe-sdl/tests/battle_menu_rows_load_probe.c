#include <assert.h>
#include <stdio.h>
#include <string.h>
#define main npe_app_main
#include "../main.c"
#undef main
#ifdef NPE_BATTLE_TYPE_WORDS
#define ROW_WIDTH 6
#else
#define ROW_WIDTH 2
#endif
typedef struct {const char *entry;uint8_t before[ROW_WIDTH],after[ROW_WIDTH];} MenuRowsLoad;
#ifdef NPE_BATTLE_TYPE_WORDS
#include "battle_type_words_load_native.h"
static const uint32_t row_addresses[]={0x527aa0,0x527aa1,0x527aa6,0x527aa7,0x527aaa,0x527aab};
#define OWN_PREFIX "tmp/battle-type-words-own-"
#elif defined(NPE_BATTLE_EXECUTION_STATE)
#include "battle_execution_load_native.h"
static const uint32_t row_addresses[]={0x527aa8,0x527acc};
#define OWN_PREFIX "tmp/battle-execution-own-"
#elif defined(NPE_SELECTOR_TARGET)
#include "battle_selector_target_load_native.h"
static const uint32_t row_addresses[]={0x527ac0,0x527ac8};
#define OWN_PREFIX "tmp/battle-selector-target-own-"
#elif defined(NPE_BATTLE_RESOURCE_FLAGS)
#include "battle_resource_flags_load_native.h"
static const uint32_t row_addresses[]={0x527aa4,0x527aa5};
#define OWN_PREFIX "tmp/battle-resource-flags-own-"
#elif defined(NPE_BATTLE_STATUS_MEMBER)
#include "battle_status_member_load_native.h"
static const uint32_t row_addresses[]={0x527aac,0x527ac8};
#define OWN_PREFIX "tmp/battle-status-member-own-"
#elif defined(NPE_BATTLE_PHASE_RESIDENT)
#include "battle_phase_load_native.h"
static const uint32_t row_addresses[]={0x527ac8,0x527a9e};
#define OWN_PREFIX "tmp/battle-phase-own-"
#elif defined(NPE_BATTLE_COMMAND_RESIDENT)
#include "battle_command_rows_load_native.h"
static const uint32_t row_addresses[]={0x527a9e,0x527aae};
#define OWN_PREFIX "tmp/battle-command-rows-own-"
#else
#include "battle_menu_rows_load_native.h"
static const uint32_t row_addresses[]={0x527a9c,0x527aa2};
#define OWN_PREFIX "tmp/battle-menu-rows-own-"
#endif
static void put(NpeFile *f,const uint8_t rows[ROW_WIDTH]){
    for(unsigned i=0;i<ROW_WIDTH;++i)assert(!npe_rpg_runtime_write(f,row_addresses[i],rows+i,1));
}
static void compare(const NpeFile *f,const NpeWorld *w,const uint8_t expected[ROW_WIDTH]){
    for(unsigned i=0;i<ROW_WIDTH;++i){uint8_t direct,runtime;
        assert(!npe_world_resident_read(w,row_addresses[i],&direct,1)&&direct==expected[i]);
        assert(!npe_rpg_runtime_read(f,row_addresses[i],&runtime,1)&&runtime==expected[i]);}
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<4;++i){const MenuRowsLoad *c=&menu_rows_load[i];NpeWorld w,cold;
        assert(!npe_world_new_game(&f,&w)&&!npe_world_resources_open(&f,&w));
        const uint8_t zero[ROW_WIDTH]={0};compare(&f,&w,zero);
        char base[128],altered[128];
        snprintf(base,sizeof(base),OWN_PREFIX "%s-base.dat",c->entry);
        snprintf(altered,sizeof(altered),OWN_PREFIX "%s-altered.dat",c->entry);
        assert(!npe_world_save(&f,&w,base));put(&f,c->before);compare(&f,&w,c->before);
        assert(!npe_world_save(&f,&w,altered));uint8_t *a,*b;size_t as,bs;
        assert(!read_file(base,&a,&as)&&!read_file(altered,&b,&bs)&&as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
        if(!strcmp(c->entry,"new")||!strcmp(c->entry,"empty"))assert(!npe_world_restart_game(&f,&w));
        else assert(!npe_world_restore(&f,&w,base));
        compare(&f,&w,c->after);
        assert(!npe_world_load(&f,&cold,base)&&cold.battle_menu_row==0&&cold.battle_item_row==0);
        assert(cold.battle_status_member==0);
        assert(cold.battle_resource_flags==0);
        assert(cold.battle_display_target==0);
        assert(cold.battle_selection_item_type==0&&cold.battle_selection_magic_type==0&&cold.battle_selection_type_copy==0);
        assert(cold.battle_action_ready==0&&cold.battle_execution_slot==0);
#ifdef NPE_BATTLE_COMMAND_RESIDENT
        assert(cold.battle_actor==0&&cold.battle_menu_previous_kind==0);
#endif
#ifdef NPE_BATTLE_PHASE_RESIDENT
        assert(cold.battle_menu_phase==0);
#endif
        uint8_t row=4;assert(!npe_world_resident_write(&w,row_addresses[0],&row,1));row=1;assert(!npe_world_resident_write(&w,row_addresses[1],&row,1));
        uint8_t live[ROW_WIDTH];memcpy(live,c->after,ROW_WIDTH);live[0]=4;live[1]=1;compare(&f,&w,live);
#ifdef NPE_BATTLE_TYPE_WORDS
        const uint32_t addresses[]={0x527aa0,0x527aa6,0x527aaa};
        const uint16_t fields[]={w.battle_selection_item_type,w.battle_selection_magic_type,w.battle_selection_type_copy};
        for(unsigned k=0;k<3;++k){uint8_t pair[2];assert(!npe_rpg_runtime_read(&f,addresses[k],pair,2));
            assert((unsigned)pair[0]*256+pair[1]==fields[k]);
            const uint8_t word[2]={0xfe,(uint8_t)(0x81+k)},high=0x37,low=(uint8_t)k;
            assert(!npe_rpg_runtime_write(&f,addresses[k],word,2));
            assert(!npe_world_resident_write(&w,addresses[k],&high,1));
            assert(!npe_world_resident_write(&w,addresses[k]+1,&low,1));
            assert(!npe_rpg_runtime_read(&f,addresses[k],pair,2)&&pair[0]==high&&pair[1]==low);
            assert(!npe_rpg_runtime_write(&f,addresses[k],word,2));
            assert(!npe_world_resident_read(&w,addresses[k],pair,2)&&!memcmp(pair,word,2));}
#endif
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"%s:original full menu bytes retained;runtime bridge and own save unchanged\n",c->entry);
    }
    NpeWorld blank={0};uint8_t byte;unsigned bound=0;
    for(uint32_t address=0x510000;address<0x530000;++address)
        bound+=!npe_world_resident_read(&blank,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&fingerprint==npe_game_fingerprint(&f));npe_free(&f);
    puts("Four original warm constructors,live menu rows and unchanged NPESAV22 passed");return 0;
}
