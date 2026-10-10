#include "../npe.h"
#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "equipment_description_native.h"
static uint8_t *read_bytes(const char *path,size_t *size){
    FILE *in=fopen(path,"rb");assert(in&&!fseek(in,0,SEEK_END));long n=ftell(in);assert(n>0&&!fseek(in,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)n);assert(bytes&&fread(bytes,1,(size_t)n,in)==(size_t)n&&!fclose(in));*size=(size_t)n;return bytes;
}
static void frame(const MenuCase *c,unsigned phase,uint8_t *pixels){
    char path[180];snprintf(path,sizeof(path),"tmp/native-equipment-description-%s-%u-commit-0.idx",c->name,phase);
    size_t n;uint8_t *data=read_bytes(path,&n);assert(n==28800);memcpy(pixels,data,n);free(data);
}
static void check(const NpeWorld *w,const MenuState *s){
    fprintf(stderr,"control actual %u/%u/%u; original %u/%u/%u\n",w->item_description_length,w->shop_description_start,w->shop_description_end,s->descLength,s->descStart,s->descEnd);
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->property_member==s->member&&w->equipment_view_slot+1u==s->equipmentSlot);
    assert(w->equipment_description_item==s->gearSelected&&w->item_description_length==s->descLength&&w->shop_description_start==s->descStart&&w->shop_description_end==s->descEnd);
    assert(w->item_record_id==s->itemLoaded&&!memcmp(w->item_record_bytes,s->itemBytes,142));
    assert(w->skill_record_id==s->skillLoaded&&!memcmp(w->skill_record_bytes,s->skillBytes,144));
}
static void isolation(const NpeFile *file){
    NpeWorld w,cold;assert(!npe_world_new_game(file,&w)&&!npe_world_resources_open((NpeFile *)file,&w));
    assert(!npe_world_save(file,&w,"tmp/equipment-description-isolation-a.sav"));
    uint8_t *before=malloc(131072),*after=malloc(131072);assert(before&&after);
    assert(!npe_rpg_runtime_read(file,UINT32_C(0x510000),before,131072));
    memset(w.equipment_description_local,0xcc,sizeof(w.equipment_description_local));w.equipment_description_local_ready=1;
    assert(!npe_rpg_runtime_read(file,UINT32_C(0x510000),after,131072)&&!memcmp(before,after,131072));
    assert(!npe_world_save(file,&w,"tmp/equipment-description-isolation-b.sav"));
    size_t na,nb;uint8_t *a=read_bytes("tmp/equipment-description-isolation-a.sav",&na),*b=read_bytes("tmp/equipment-description-isolation-b.sav",&nb);
    assert(na==nb&&!memcmp(a,b,na));memset(&cold,0xcc,sizeof(cold));assert(!npe_world_load(file,&cold,"tmp/equipment-description-isolation-b.sav"));
    assert(!cold.equipment_description_local_ready);for(unsigned i=0;i<sizeof(cold.equipment_description_local);++i)assert(!cold.equipment_description_local[i]);
    printf("caller local isolation: complete128KiB guest RAM and %zu save bytes unchanged; cold local cleared\n",na);
    free(a);free(b);free(before);free(after);npe_rpg_runtime_close((NpeFile *)file);
    assert(!remove("tmp/equipment-description-isolation-a.sav")&&!remove("tmp/equipment-description-isolation-b.sav"));
}
int main(int argc,char **argv){
    assert(argc==2);size_t size;uint8_t *data=read_bytes(argv[1],&size);NpeFile file;char error[128];assert(!npe_parse(&file,data,size,error,sizeof(error)));
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));NpeWorld w;unsigned total=0,ignored=0;
    isolation(&file);
    for(unsigned c=0;c<sizeof(menu_cases)/sizeof(*menu_cases);++c){const MenuCase *current=menu_cases+c;const MenuState *seed=&current->phase[3].before;
        assert(!npe_world_new_game(&file,&w));w.game_mode=6;w.property_page=10;w.property_member=0;w.equipment_view_slot=0;w.lcd_background=0;
        memcpy(w.equipped_items,current->gear,sizeof(w.equipped_items));w.item_record_id=(uint8_t)seed->itemLoaded;memcpy(w.item_record_bytes,seed->itemBytes,142);
        w.skill_record_id=(uint8_t)seed->skillLoaded;memcpy(w.skill_record_bytes,seed->skillBytes,144);
        w.item_description_length=(uint16_t)seed->descLength;w.shop_description_start=(uint16_t)seed->descStart;w.shop_description_end=(uint16_t)seed->descEnd;
        assert(!npe_world_resources_open(&file,&w));uint8_t pixels[28800],native[28800],bitmap[32],after_bitmap[32];assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32));
        for(unsigned phase=3;phase<current->phases;++phase){const MenuPhase *p=current->phase+phase;
            if(p->patch[0]<65536)w.item_description_length=(uint16_t)p->patch[0];
            if(p->patch[1]<65536)w.shop_description_start=(uint16_t)p->patch[1];
            if(p->patch[2]<65536)w.shop_description_end=(uint16_t)p->patch[2];
            NpeWorld old=w;int result=1;
            if(phase==3)assert(!npe_render_equipment_view(&file,&font,&w,NULL,pixels));
            else if(p->mask==8){w.equipment_view_slot=(uint8_t)(p->after.equipmentSlot-1);assert(!npe_render_equipment_view(&file,&font,&w,&old,pixels));}
            else{
                uint8_t retained[28800];memcpy(retained,pixels,sizeof(retained));
                result=npe_world_equipment_description_page(&file,&w,p->mask==256?1:-1);assert(result>=0);
                if(result)assert(!npe_render_equipment_view(&file,&font,&w,&old,pixels));else assert(!memcmp(retained,pixels,sizeof(retained)));
                assert(w.equipment_description_revision==old.equipment_description_revision+(unsigned)result);
            }
            check(&w,&p->after);assert(result==(int)p->commits);
            if(result){frame(current,phase,native);unsigned bad=0;for(unsigned i=0;i<28800;++i)bad+=pixels[i]!=native[i];fprintf(stderr,"%s phase%u differences%u\n",current->name,phase,bad);assert(!bad);++total;}else ++ignored;
        }
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,after_bitmap,32)&&!memcmp(bitmap,after_bitmap,32));npe_rpg_runtime_close(&file);
    }
    printf("equipment descriptions: %u original frames/states, %u boundary stops, heap bitmap restored\n",total,ignored);npe_font_free(&font);npe_free(&file);return 0;
}
