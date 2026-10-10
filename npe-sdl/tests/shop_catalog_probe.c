#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shop_catalog_native.h"
#include "shop_catalog_lifetime_native.h"
static void patch(const NpeFile *f,const ShopUiCase *c){
    for(unsigned i=0;i<c->patch_count;++i){uint8_t b=(uint8_t)c->patch[i][1];assert(!npe_rpg_runtime_write(f,0x526dba+c->patch[i][0],&b,1));}
    if(c->afterContext<65536){uint8_t b[]={(uint8_t)(c->afterContext>>8),(uint8_t)c->afterContext};assert(!npe_rpg_runtime_write(f,0x527938,b,2));}
    const unsigned value[]={c->scanFirst,c->scanLast,c->inventoryOnly};
    for(unsigned i=0;i<3;++i)if(value[i]<256){uint8_t b=(uint8_t)value[i];assert(!npe_rpg_runtime_write(f,0x527a10+2*i,&b,1));}
}
static unsigned different(const NpeFile *f,NpeWorld *w,const ShopUiState *s,const ShopItemCache *cache){
    uint8_t bytes[255],scan[3];unsigned result=0;
    if(npe_world_resident_read(w,0x526dba,bytes,255)||memcmp(bytes,s->catalog,255))++result;
    for(unsigned i=0;i<3;++i)if(npe_world_resident_read(w,0x527a10+2*i,scan+i,1))scan[i]=0;
    result+=(scan[0]!=s->scanFirst||scan[1]!=s->scanLast||scan[2]!=s->inventoryOnly);
    result+=(w->shop_selected_item!=s->selected||w->shop_first_item!=s->first||w->shop_last_item!=s->last||w->shop_row!=s->row);
    result+=(w->item_record_id!=cache->id||memcmp(w->item_record_bytes,cache->bytes,142)!=0);
    assert(!npe_rpg_runtime_failures(f));return result;
}
static void lifetime(NpeFile *f,NpeWorld *w,const char *path){
    NpeWorld *cold=malloc(sizeof(*cold));assert(cold);
    assert(!npe_world_new_game(f,w)&&!npe_world_save(f,w,path)&&!npe_world_resources_open(f,w));
    for(unsigned k=0;k<sizeof(catalog_lifetime)/sizeof(*catalog_lifetime);++k){const ShopCatalogLifetime *c=catalog_lifetime+k;
        assert(!npe_rpg_runtime_write(f,0x526dba,c->catalog,255));
        const unsigned value[]={c->first,c->last,c->inventory};
        for(unsigned i=0;i<3;++i){uint8_t b=(uint8_t)value[i];assert(!npe_rpg_runtime_write(f,0x527a10+2*i,&b,1));}
        assert(!(c->restart?npe_world_restart_game(f,w):npe_world_restore(f,w,path)));
        uint8_t table[255],b;assert(!npe_world_resident_read(w,0x526dba,table,255)&&!memcmp(table,c->catalog,255));
        assert(!npe_rpg_runtime_read(f,0x526dba,table,255)&&!memcmp(table,c->catalog,255));
        for(unsigned i=0;i<3;++i)assert(!npe_rpg_runtime_read(f,0x527a10+2*i,&b,1)&&b==value[i]);
        assert(!npe_world_load(f,cold,path)&&!cold->shop_catalog_ready);
        assert(!cold->shop_scan_first&&!cold->shop_scan_last&&!cold->shop_inventory_only);
        for(unsigned i=0;i<255;++i)assert(!cold->shop_catalog_present[i]);
    }
    /* Exact end, unowned neighbour and rejection without a partial change. */
    uint8_t neighbour=173,last=255,actual;
    assert(!npe_rpg_runtime_write(f,0x526eb9,&neighbour,1));
    assert(!npe_world_resident_write(w,0x526eb8,&last,1));
    assert(!npe_rpg_runtime_read(f,0x526eb8,&actual,1)&&actual==last);
    assert(!npe_rpg_runtime_read(f,0x526eb9,&actual,1)&&actual==neighbour);
    uint8_t invalid[]={19,23};assert(npe_world_resident_write(w,0x526eb8,invalid,2)==-1);
    assert(!npe_world_resident_read(w,0x526eb8,&actual,1)&&actual==last);
    assert(!npe_rpg_runtime_failures(f));free(cold);npe_rpg_runtime_close(f);
    puts("Catalog lifetime:5 original warm loads,6 constructors,11 own cold decodes and byte boundaries passed");
}
int main(int argc,char **argv){assert(argc==2||argc==3);FILE *in=fopen(argv[1],"rb");assert(in);
    assert(!fseek(in,0,SEEK_END));long length=ftell(in);assert(length>0&&!fseek(in,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)length);assert(bytes&&fread(bytes,1,(size_t)length,in)==(size_t)length&&!fclose(in));
    NpeFile f;char error[128];assert(!npe_parse(&f,bytes,(size_t)length,error,sizeof(error)));
    NpeWorld *w=malloc(sizeof(*w));assert(w);unsigned failures=0;
    for(unsigned k=0;k<sizeof(shop_ui_cases)/sizeof(*shop_ui_cases);++k){const ShopUiCase *c=shop_ui_cases+k;
        assert(!npe_world_new_game(&f,w)&&!npe_world_resources_open(&f,w));
        w->game_mode=4;w->event_context_id=c->selling?10:13;w->shop_kind=w->shop_ready=c->selling?2:1;
        w->shop_first_item=(uint16_t)c->initial_first;w->shop_last_item=(uint16_t)c->initial_last;
        w->item_record_id=(uint8_t)c->seed.id;memcpy(w->item_record_bytes,c->seed.bytes,142);
        for(unsigned i=0;i<c->stock_count;++i)w->condition_words[c->stock[i][0]]=(uint16_t)c->stock[i][1];
        assert(!npe_rpg_runtime_write(&f,0x526dba,c->seed_catalog,255));
        assert(!npe_world_shop_begin(&f,w));
        unsigned bad=different(&f,w,&c->frame[0].state,&c->frame[0].cache);patch(&f,c);
        for(unsigned i=0;i<c->count;++i){const ShopUiPhase *p=c->phase+i;assert(p->key==0x82||p->key==0x83);
            int result=npe_world_shop_move(&f,w,p->key==0x83?1:-1);
            bad+=(result<0);if(result>=0)bad+=different(&f,w,&p->after,&p->cache);
        }
        fprintf(stderr,"%s: %u differences\n",c->name,bad);failures+=bad!=0;
        npe_rpg_runtime_close(&f);
    }
    printf("Resident shop catalog:8 original paths,%u failed paths\n",failures);
    if(!failures&&argc==3)lifetime(&f,w,argv[2]);
    free(w);npe_free(&f);return failures?1:0;
}
