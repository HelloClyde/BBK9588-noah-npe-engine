#include "shop_text_native.h"
static void shop_text_frame(const char *name,int index,uint8_t *pixels){
    char path[200];
    if(index<0)snprintf(path,sizeof(path),"tmp/native-shop-text-settled-%s-before.idx",name);
    else snprintf(path,sizeof(path),"tmp/native-shop-text-settled-%s-%d.idx",name,index);
    FILE *in=fopen(path,"rb");assert(in);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void shop_text_seed(NpeWorld *w,const ShopTextState *s){
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;
    w->shop_ui=(uint8_t)s->ui;w->shop_selected_item=(uint16_t)s->selected;
    w->shop_quantity=(uint8_t)s->quantity;w->shop_choice=(uint8_t)s->choice;
    w->item_description_length=(uint16_t)s->length;w->shop_description_start=(uint16_t)s->start;w->shop_description_end=(uint16_t)s->end;
    w->item_record_id=(uint8_t)s->loaded;memcpy(w->item_record_bytes,s->bytes,142);
    w->shop_transaction_sell=(uint8_t)s->sell;w->shop_first_item=(uint16_t)s->first;w->shop_last_item=(uint16_t)s->last;w->shop_row=(uint8_t)s->row;
    w->shop_list_ready=1;w->money=1000;memset(w->condition_words,0,sizeof(w->condition_words));
}
static unsigned shop_text_difference(const NpeWorld *w,const ShopTextState *s){
    return (w->game_mode!=s->mode)+(w->story_event_id!=s->event)+(w->event_context_id!=s->context)+
      (w->shop_ui!=s->ui)+(w->shop_selected_item!=s->selected)+(w->shop_quantity!=s->quantity)+(w->shop_choice!=s->choice)+
      (w->item_description_length!=s->length)+(w->shop_description_start!=s->start)+(w->shop_description_end!=s->end)+
      (w->item_record_id!=s->loaded)+(memcmp(w->item_record_bytes,s->bytes,142)!=0)+
      (w->shop_transaction_sell!=s->sell)+(w->shop_first_item!=s->first)+(w->shop_last_item!=s->last)+(w->shop_row!=s->row);
}
static void shop_text_patch(NpeWorld *w,const ShopTextPhase *p){
    if(p->patch[0]<65536)w->item_description_length=(uint16_t)p->patch[0];
    if(p->patch[1]<65536)w->shop_description_start=(uint16_t)p->patch[1];
    if(p->patch[2]<65536)w->shop_description_end=(uint16_t)p->patch[2];
    if(p->patch[3]<256)w->shop_choice=(uint8_t)p->patch[3];
    assert(!shop_text_difference(w,&p->before));
}
