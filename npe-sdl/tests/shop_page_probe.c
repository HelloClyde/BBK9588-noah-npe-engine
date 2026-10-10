#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shop_page_native.h"
static unsigned difference(const NpeWorld *w,const ShopUiState *s,const ShopItemCache *cache){
    return (w->shop_selected_item!=s->selected)+(w->shop_first_item!=s->first)+
        (w->shop_last_item!=s->last)+(w->shop_row!=s->row)+(w->shop_scan_last!=s->scanLast)+
        (w->item_record_id!=cache->id)+(memcmp(w->item_record_bytes,cache->bytes,142)!=0);
}
static void put(NpeFile *f,uint32_t a,unsigned v,int word){
    uint8_t b[]={(uint8_t)(v>>8),(uint8_t)v};assert(!npe_rpg_runtime_write(f,a,word?b:b+1,word?2:1));
}
int main(int argc,char **argv){assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in);
    assert(!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0&&!fseek(in,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile f;char error[128];assert(!npe_parse(&f,bytes,(size_t)size,error,sizeof(error)));
    NpeWorld *w=malloc(sizeof(*w)),*idle=malloc(sizeof(*idle));assert(w&&idle);unsigned failures=0,checkpoints=0,idle_checks=0;
    for(unsigned k=0;k<sizeof(shop_ui_cases)/sizeof(*shop_ui_cases);++k){const ShopUiCase *c=shop_ui_cases+k;
        assert(!npe_world_new_game(&f,w)&&!npe_world_resources_open(&f,w));
        w->game_mode=4;w->event_context_id=13;w->shop_kind=w->shop_ready=1;
        w->item_record_id=(uint8_t)c->seed.id;memcpy(w->item_record_bytes,c->seed.bytes,142);
        assert(!npe_world_shop_begin(&f,w));
        if(c->clearCatalog){uint8_t zero[255]={0};assert(!npe_rpg_runtime_write(&f,0x526dba,zero,255));}
        for(unsigned i=0;i<c->patch_count;++i)put(&f,0x526dba+c->patch[i][0],c->patch[i][1],0);
        if(c->scanFirst<256)put(&f,0x527a10,c->scanFirst,0);
        if(c->scanLast<256)put(&f,0x527a12,c->scanLast,0);
        if(c->afterSelected<65536)put(&f,0x527a4c,c->afterSelected,1);
        if(c->afterFirst<65536)put(&f,0x527ac6,c->afterFirst,1);
        if(c->afterLast<65536)put(&f,0x527ab6,c->afterLast,1);
        if(c->afterRow<256)put(&f,0x527a46,c->afterRow,0);
        unsigned bad=0;int result=0;
        for(unsigned i=0;i<c->count;++i){const ShopUiPhase *p=c->phase+i;
            if(p->upper<256)put(&f,0x527a12,p->upper,0);
            if(p->key==0x82||p->key==0x83||result==2)result=npe_world_shop_move(&f,w,p->key==0x83?1:-1);
            bad+=(result<0);bad+=difference(w,&p->after,&p->cache);++checkpoints;
            if(!strncmp(c->name,"page-pending",12)&&i<2){
                bad+=result!=NPE_SHOP_MOVE_PENDING;
                *idle=*w;
                for(unsigned wait=0;wait<100;++wait){
                    bad+=npe_world_shop_resume(&f,w)!=NPE_SHOP_MOVE_PENDING;
                    bad+=memcmp(idle,w,sizeof(*w))!=0;++idle_checks;
                }
            }else bad+=result!=(!strcmp(c->name,"page-low-reject")?0:1);
        }
        fprintf(stderr,"%s: %u differences\n",c->name,bad);failures+=bad!=0;npe_rpg_runtime_close(&f);
    }
    printf("Original shop page:9 paths,%u checkpoints,%u failed paths; %u idle resumes\n",checkpoints,failures,idle_checks);
    free(idle);free(w);npe_free(&f);return failures?1:0;
}
