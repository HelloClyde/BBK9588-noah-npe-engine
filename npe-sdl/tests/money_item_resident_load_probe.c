#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *entry;unsigned map;uint8_t before[80],after[80];} MoneyItemResidentLoadCase;
#include "money_item_resident_load_native.h"
static void compare(const NpeWorld *w,const uint8_t expected[80]){
    uint8_t actual[80];assert(!npe_world_resident_read(w,0x5276b0,actual,40));
    assert(!npe_world_resident_read(w,0x5276da,actual+40,40));
    for(unsigned i=0;i<80;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"money/item byte%u C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
static void seed(NpeWorld *w,const uint8_t bytes[80]){
    assert(!npe_world_resident_write(w,0x5276b0,bytes,40));
    assert(!npe_world_resident_write(w,0x5276da,bytes+40,40));
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<16;++i){
        const MoneyItemResidentLoadCase *c=&money_item_load_cases[i];NpeWorld w={0};NpeMap map;
        assert(!npe_map(&f,c->map-1,&map));seed(&w,c->before);
        assert(!npe_world_load_money_events(&w,&map)&&!npe_world_load_item_events(&w,&map));compare(&w,c->after);
        assert(!npe_world_resources_open(&f,&w));
        const uint8_t cross[]={0x12,0x34,0x56,0x78};uint8_t actual[4];
        const uint32_t addresses[]={0x5276b7,0x5276e1,0x52799f};
        for(unsigned j=0;j<2;++j){assert(!npe_rpg_runtime_write(&f,addresses[j],cross,4));
            assert(!npe_rpg_runtime_read(&f,addresses[j],actual,4)&&!memcmp(actual,cross,4));}
        assert(w.item_events[0].next_mode==0x12&&w.item_events[1].quantity==0x3456&&w.item_events[1].item_index==0x7800+c->after[11]);
        assert(w.money_event_padding[0][1]==0x12&&w.money_events[1].amount==0x3456);
        NpeMapMoneyEvent money;NpeMapItemEvent item;
        uint8_t high=0x81;assert(!npe_rpg_runtime_write(&f,0x5276d6,&high,1));
        assert(npe_world_item_event(&w,261,&item)==1&&item.next_mode==w.item_events[4].next_mode&&item.add_item==0x81);
        assert(!npe_rpg_runtime_write(&f,0x5276fa,&high,1));
        assert(npe_world_money_event(&w,261,&money)==1&&money.amount==(uint16_t)(0x8100|c->after[73]));
        const uint8_t cash[]={0x81,0x23,0x45,0x67};assert(!npe_rpg_runtime_write(&f,0x52799e,cash,4)&&w.money==0x81234567);
        assert(!npe_rpg_runtime_write(&f,addresses[2],cross,3)&&w.money==0x81123456);
        w.money=123456;assert(!npe_rpg_runtime_read(&f,0x52799e,actual,4)&&actual[0]==0&&actual[1]==1&&actual[2]==0xe2&&actual[3]==0x40);
        NpeWorld prior=w;assert(npe_world_resident_write(&w,0x5276d7,cross,2)&&!memcmp(&w,&prior,sizeof(w)));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
        fprintf(stderr,"Money/item load%u %s map%u:80 original bytes and shared writes match\n",i,c->entry,c->map);
    }
    NpeWorld w;assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/money-item-resident-save.dat",*changed="tmp/money-item-resident-cache.dat";
    assert(!npe_world_save(&f,&w,save));seed(&w,money_item_load_cases[0].before);
    assert(!npe_world_save(&f,&w,changed));uint8_t *a,*b;size_t as,bs;
    assert(!read_file(save,&a,&as)&&!read_file(changed,&b,&bs)&&as==bs&&!memcmp(a,b,as)&&!memcmp(a,"NPESAV22",8));free(a);free(b);
    assert(!npe_world_restore(&f,&w,save));compare(&w,money_item_load_cases[0].after);
    seed(&w,money_item_load_cases[0].before);assert(!npe_world_restart_game(&f,&w));compare(&w,money_item_load_cases[14].after);
    assert(!remove(save)&&!remove(changed));
    unsigned bound=0;uint8_t byte;for(uint32_t address=0x510000;address<0x530000;++address)bound+=!npe_world_resident_read(&w,address,&byte,1);
    assert(bound==NPE_WORLD_RESIDENT_BYTES&&npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Money/item resident:16 original loads/1280 bytes,3748 World bytes,partial/cross-slot writes,warm restore/restart,unchanged own save passed");return 0;
}
