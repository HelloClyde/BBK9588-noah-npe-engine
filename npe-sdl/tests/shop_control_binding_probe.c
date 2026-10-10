#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shop_control_binding_native.h"
#include "shop_control_load_native.h"
static uint8_t *read_file(const char *path,size_t *size){FILE *f=fopen(path,"rb");assert(f&&!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0&&!fseek(f,0,SEEK_SET));uint8_t *p=malloc((size_t)n);assert(p&&fread(p,1,(size_t)n,f)==(size_t)n&&!fclose(f));*size=(size_t)n;return p;}
int main(int argc,char **argv){
    assert(argc==2||argc==3);size_t size;uint8_t *bytes=read_file(argv[1],&size);NpeFile f;char error[128];NpeWorld *w=calloc(1,sizeof(*w));assert(w&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    unsigned failures=0;
    const uint32_t addresses[]={0x527936,0x527937,0x527938,0x527939,0x527a0a,0x527a0c,0x527a0e,0x527a46,0x527a48,0x527a4a,0x527a4c,0x527a4d,0x527a68,0x527a82,0x527a83,0x527ab6,0x527ab7,0x527abc,0x527abd,0x527abe,0x527abf,0x527ac2,0x527ac6,0x527ac7};
    unsigned missing=0;for(unsigned i=0;i<sizeof(addresses)/sizeof(*addresses);++i){uint8_t value;missing+=npe_world_resident_read(w,addresses[i],&value,1)!=0;}
    printf("shared controls:24 additional bytes,%u missing\n",missing);failures+=missing!=0;
    for(unsigned i=0;i<sizeof(shop_ui_cases)/sizeof(*shop_ui_cases);++i){const ShopUiCase *c=&shop_ui_cases[i];
        assert(!npe_world_init(&f,w,1,1,1)&&!npe_world_resources_open(&f,w));
        w->game_mode=4;w->event_context_id=c->selling?10:13;w->shop_kind=w->shop_ready=c->selling?2:1;w->money=1000;
        w->item_record_id=(uint8_t)c->seed.id;memcpy(w->item_record_bytes,c->seed.bytes,142);
        memset(w->condition_words,0,sizeof(w->condition_words));for(unsigned j=0;j<c->stock_count;++j)w->condition_words[c->stock[j][0]]=(uint16_t)c->stock[j][1];
        assert(!npe_world_shop_begin(&f,w));
        if(c->afterFlag<256){uint8_t value=(uint8_t)c->afterFlag;assert(!npe_rpg_runtime_write(&f,0x527ac2,&value,1));}
        if(c->afterKind<256){uint8_t value=(uint8_t)c->afterKind;assert(!npe_rpg_runtime_write(&f,0x527a0e,&value,1));}
        w->shop_quantity=1;assert(npe_world_shop_trade(&f,w)==1);
        const ShopUiState *expected=&c->phase[c->count-1].after;
        unsigned different=w->money!=expected->money||w->condition_words[c->item]!=expected->owned;
        printf("%s money=%u/%u owned=%u/%u different=%u\n",c->name,w->money,expected->money,w->condition_words[c->item],expected->owned,different);
        failures+=different;npe_rpg_runtime_close(&f);
    }
    if(!missing){
        assert(!npe_world_init(&f,w,1,1,1)&&!npe_world_resources_open(&f,w));
        for(unsigned i=0;i<sizeof(addresses)/sizeof(*addresses);++i){uint8_t value=(uint8_t)(i*37+17),actual;
            assert(!npe_rpg_runtime_write(&f,addresses[i],&value,1)&&!npe_world_resident_read(w,addresses[i],&actual,1)&&actual==value);
            value^=0xa5;assert(!npe_world_resident_write(w,addresses[i],&value,1)&&!npe_rpg_runtime_read(&f,addresses[i],&actual,1)&&actual==value);
        }
        uint8_t controls[]={0,2,0x12,0x34,0xab,0xcd},actual[6];
        assert(!npe_world_resident_write(w,0x527934,controls,6)&&w->game_mode==2&&w->story_event_id==0x1234&&w->event_context_id==0xabcd);
        assert(!npe_rpg_runtime_read(&f,0x527934,actual,6)&&!memcmp(controls,actual,6));
        uint8_t half=0xee;assert(!npe_world_resident_write(w,0x527937,&half,1)&&w->story_event_id==0x12ee);
        NpeWorld *before=malloc(sizeof(*before));assert(before);*before=*w;
        uint8_t invalid[7]={0};assert(npe_world_resident_write(w,0x527934,invalid,7)==-1&&!memcmp(before,w,sizeof(*w)));free(before);
        uint8_t followup[]={25,0xa5},next[]={0xff,0xff},zero=0;
        assert(!npe_rpg_runtime_write(&f,0x527a0a,followup,2)&&!npe_rpg_runtime_write(&f,0x527a82,next,2)&&!npe_rpg_runtime_write(&f,0x527a68,&zero,1));
        w->mode2_ready=1;assert(npe_world_finish_mode2(w)==1&&w->game_mode==1&&w->story_event_id==0xffff);
        assert(!npe_rpg_runtime_read(&f,0x527a0b,&half,1)&&half==0xa5);npe_rpg_runtime_close(&f);
        puts("24 control bytes:bidirectional live binding,BE partial/contiguous writes,rejection,unmapped neighbour and raw dialogue continuation passed");
        if(argc==3){NpeWorld *cold=calloc(1,sizeof(*cold));assert(cold);
            for(unsigned i=0;i<sizeof(shop_control_load)/sizeof(*shop_control_load);++i){const ShopControlLoad *c=shop_control_load+i;
                assert(!npe_world_init(&f,w,9,13,4)&&!npe_world_save(&f,w,argv[2]));
                w->event_context_id=(uint16_t)c->context;w->mode2_context=(uint8_t)c->npcContext;
                w->shop_kind=(uint8_t)c->kind;w->shop_transaction_sell=(uint8_t)c->sellFlag;
                w->shop_selected_item=(uint16_t)c->selected;w->shop_first_item=(uint16_t)c->first;w->shop_last_item=(uint16_t)c->last;
                w->shop_row=(uint8_t)c->row;w->shop_ui=(uint8_t)c->ui;w->shop_quantity=(uint8_t)c->quantity;w->shop_choice=(uint8_t)c->choice;
                w->shop_description_start=(uint16_t)c->descStart;w->shop_description_end=(uint16_t)c->descEnd;
                assert(!npe_world_restore(&f,w,argv[2])&&w->shop_transaction_sell==c->sellFlag&&w->shop_kind==c->kind&&w->event_context_id==c->context);
                uint8_t flag;assert(!npe_world_resident_read(w,0x527ac2,&flag,1)&&flag==c->sellFlag);
                const uint32_t locations[]={0x527938,0x527a68,0x527a0e,0x527ac2,0x527a4c,0x527ac6,0x527ab6,0x527a46,0x527a4a,0x527a48,0x527a0c,0x527abe,0x527abc};
                const unsigned widths[]={2,1,1,1,2,2,2,1,1,1,1,2,2};
                const unsigned values[]={c->context,c->npcContext,c->kind,c->sellFlag,c->selected,c->first,c->last,c->row,c->ui,c->quantity,c->choice,c->descStart,c->descEnd};
                for(unsigned j=0;j<13;++j){uint8_t raw[2];assert(!npe_world_resident_read(w,locations[j],raw,widths[j]));
                    assert((widths[j]==2?(unsigned)raw[0]*256+raw[1]:raw[0])==values[j]);}
                assert(!npe_world_load(&f,cold,argv[2])&&!cold->shop_transaction_sell);
                assert(!npe_world_restart_game(&f,w)&&w->shop_transaction_sell==c->sellFlag);
            }free(cold);puts("6 original warm-load cases:13 fields each;6 early constructor flags,including255;6 own cold decodes passed");
        }
    }
    npe_free(&f);free(w);printf("shop control baseline:4 original transactions,%u failures\n",failures);return failures?1:0;
}
