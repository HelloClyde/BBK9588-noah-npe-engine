#include "../npe.h"
#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {const char *name;uint32_t money;unsigned owned,quantity,sell;uint32_t after_money;unsigned after_owned,applied;} ShopCase;
#include "shop_native.h"
static uint8_t *load(const char *name,size_t *size){FILE *f=fopen(name,"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0&&!fseek(f,0,SEEK_SET));
    uint8_t *p=malloc((size_t)n);assert(p&&fread(p,1,(size_t)n,f)==(size_t)n&&!fclose(f));*size=(size_t)n;return p;}
int main(int argc,char **argv){assert(argc==2);size_t size;uint8_t *bytes=load(argv[1],&size);NpeFile file;char error[128];
    assert(!npe_parse(&file,bytes,size,error,sizeof(error)));NpeItem item;assert(!npe_native_item(&file,45,&item)&&item.price==10);
    for(unsigned i=0;i<sizeof(shop_cases)/sizeof(*shop_cases);++i){const ShopCase *c=&shop_cases[i];NpeWorld world={0};
        world.money=c->money;world.condition_words[45]=(uint16_t)c->owned;NpeWorld before=world;
        int applied=npe_world_trade_item(&file,&world,45,(uint8_t)c->quantity,(int)c->sell);
        fprintf(stderr,"%s applied=%d money=%u owned=%u\n",c->name,applied,world.money,world.condition_words[45]);
        assert(applied==(int)c->applied&&world.money==c->after_money&&world.condition_words[45]==c->after_owned);
        assert(world.item_record_id==45);
        NpeItem cached;assert(!npe_world_item(NULL,&world,45,&cached)&&cached.price==10);
        before.money=world.money;before.condition_words[45]=world.condition_words[45];
        before.item_record_id=world.item_record_id;memcpy(before.item_record_bytes,world.item_record_bytes,142);
        memcpy(before.item_record_raw,world.item_record_raw,41);assert(!memcmp(&before,&world,sizeof(world)));}
    unsigned shops=0;for(unsigned c=11;c<11+file.offset_table_records[11];++c){NpeShopCatalog catalog;uint8_t ids[254];unsigned count;
        assert(!npe_shop_catalog(&file,c,&catalog)&&!npe_shop_items(&file,c,ids,&count));
        for(unsigned i=0;i<count;++i){assert(ids[i]&&ids[i]<255&&(!i||ids[i]>ids[i-1]));NpeItem listed;
            assert(!npe_native_item(&file,ids[i],&listed));}++shops;}
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    NpeWorld world={0};world.money=100;
    for(unsigned selection=0;selection<2;++selection){uint8_t actual[NPE_WIDTH*NPE_HEIGHT];
        assert(!npe_render_shop_buy(&file,&font,&world,13,0,selection,actual));
        char name[128];snprintf(name,sizeof(name),"tmp/native-exit-dialogue-1-commit-%u.idx",16+selection);
        size_t n;uint8_t *native=load(name,&n);assert(n==sizeof(actual));unsigned diff=0;
        for(unsigned i=0;i<sizeof(actual);++i)diff+=actual[i]!=native[i];free(native);
        fprintf(stderr,"shop-buy selection%u C=%u\n",selection,diff);
        if(diff){FILE *f=fopen("tmp/shop-buy-actual.idx","wb");assert(f&&fwrite(actual,1,sizeof(actual),f)==sizeof(actual)&&!fclose(f));}
        assert(!diff);}
    /* An unread owned record beyond the seven-row window must not abort
       shop entry or drawing. Only the diagnostic copy's offset is changed. */
    uint8_t *diagnostic=malloc(file.size);assert(diagnostic);memcpy(diagnostic,file.bytes,file.size);
    NpeFile bounded=file;bounded.bytes=diagnostic;
    size_t late=file.sections[2].offset+173u*4u;
    uint32_t target=(uint32_t)(file.size-20u-file.sections[2].offset);
    for(unsigned i=0;i<4;++i)diagnostic[late+i]=(uint8_t)(target>>(24u-8u*i));
    assert(npe_native_item(&bounded,174,&item)<0);
    world=(NpeWorld){0};world.shop_kind=world.shop_ready=2;world.event_context_id=10;world.money=1000;
    const unsigned stock[]={45,46,47,48,49,53,54,174};
    for(unsigned i=0;i<sizeof(stock)/sizeof(*stock);++i)world.condition_words[stock[i]]=2;
    uint8_t actual[NPE_WIDTH*NPE_HEIGHT];
    assert(!npe_world_shop_begin(&bounded,&world)&&world.shop_selected_item==45&&world.shop_last_item==54);
    assert(!npe_render_shop_list(&bounded,&font,&world,actual));
    size_t n;uint8_t *native=load("tmp/native-shop-item-cache-sell-window-0.idx",&n);
    assert(n==sizeof(actual)&&!memcmp(actual,native,n));free(native);free(diagnostic);
    fprintf(stderr,"Unselected unreadable owned record preserves the seven-row shop frame\n");
    fprintf(stderr,"8 original shop transactions and %u current-NPE catalogs passed\n",shops);
    npe_font_free(&font);npe_free(&file);return 0;}
