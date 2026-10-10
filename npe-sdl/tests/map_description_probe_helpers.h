#include "map_description_native.h"
static void map_description_frame(const char *name,int index,uint8_t *pixels){
    char path[200];if(index<0)snprintf(path,sizeof(path),"tmp/native-map-description-%s-before.idx",name);
    else snprintf(path,sizeof(path),"tmp/native-map-description-%s-%d.idx",name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,28800,in)==28800&&fgetc(in)==EOF&&!fclose(in));
}
static void map_description_seed(NpeWorld *w,const MapDescriptionState *s){
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->property_page=(uint8_t)s->page;w->property_member=(uint8_t)s->member;w->shop_selected_item=(uint16_t)s->selected;
    w->item_description_length=(uint16_t)s->length;w->shop_description_start=(uint16_t)s->start;w->shop_description_end=(uint16_t)s->end;
    w->item_record_id=(uint8_t)s->itemLoaded;memcpy(w->item_record_bytes,s->itemBytes,142);w->skill_record_id=(uint8_t)s->skillLoaded;memcpy(w->skill_record_bytes,s->skillBytes,144);
    const unsigned *p=s->party;w->hp[0]=(uint16_t)p[0];w->max_hp[0]=(uint16_t)p[1];w->qi[0]=(uint16_t)p[2];w->max_qi[0]=(uint16_t)p[3];w->attack[0]=(uint16_t)p[4];w->defense[0]=(uint16_t)p[5];w->level[0]=(uint8_t)p[6];w->experience[0]=(uint16_t)p[7];w->experience_to_next[0]=(uint16_t)p[8];
    for(unsigned i=0;i<3;++i)w->other_stats[0][i]=(uint16_t)p[9+i];
    for(unsigned i=0;i<4;++i)w->status_guard[0][i]=(uint16_t)p[12+i];
    w->party_active[0]=1;w->party_active[1]=w->party_active[2]=0;memset(w->condition_words,0,sizeof(w->condition_words));w->condition_words[45]=2;memset(w->skill_slots,0,sizeof(w->skill_slots));w->skill_slots[0][0]=6;w->music_enabled=0;w->lcd_background=0;
}
static unsigned map_description_differences(const NpeWorld *w,const MapDescriptionState *s){
    if(w->game_mode!=s->mode||w->story_event_id!=s->event||w->property_page!=s->page||w->shop_selected_item!=s->selected)
        fprintf(stderr,"controls actual mode%u event%u page%u selected%u; native %u %u %u %u\n",w->game_mode,w->story_event_id,w->property_page,w->shop_selected_item,s->mode,s->event,s->page,s->selected);
    const unsigned p[]={w->hp[0],w->max_hp[0],w->qi[0],w->max_qi[0],w->attack[0],w->defense[0],w->level[0],w->experience[0],w->experience_to_next[0],w->other_stats[0][0],w->other_stats[0][1],w->other_stats[0][2],w->status_guard[0][0],w->status_guard[0][1],w->status_guard[0][2],w->status_guard[0][3]};
    return (w->game_mode!=s->mode)+(w->story_event_id!=s->event)+(w->property_page!=s->page)+(w->property_member!=s->member)+(w->shop_selected_item!=s->selected)+(w->item_description_length!=s->length)+(w->shop_description_start!=s->start)+(w->shop_description_end!=s->end)+(w->item_record_id!=s->itemLoaded)+(w->skill_record_id!=s->skillLoaded)+(memcmp(w->item_record_bytes,s->itemBytes,142)!=0)+(memcmp(w->skill_record_bytes,s->skillBytes,144)!=0)+(memcmp(p,s->party,sizeof(p))!=0);
}
static void map_description_patch(NpeWorld *w,const MapDescriptionPhase *p){
    if(p->patch[0]<65536)w->item_description_length=(uint16_t)p->patch[0];
    if(p->patch[1]<65536)w->shop_description_start=(uint16_t)p->patch[1];
    if(p->patch[2]<65536)w->shop_description_end=(uint16_t)p->patch[2];
    assert(!map_description_differences(w,&p->before));
}
