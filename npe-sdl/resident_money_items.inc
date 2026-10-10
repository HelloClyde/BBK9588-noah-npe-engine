static uint8_t money_item_event_byte(const NpeWorld *w,unsigned money,unsigned offset){
    unsigned s=offset/8,o=offset%8;uint16_t value;
    if(money){
        const NpeMapMoneyEvent *e=&w->money_events[s];
        if(o>=6)return w->money_event_padding[s][o-6];
        if(o==4)return e->add_money;
        if(o==5)return e->next_mode;
        value=o<2?e->amount:e->next_id;
    }else{
        const NpeMapItemEvent *e=&w->item_events[s];
        if(o==6)return e->add_item;
        if(o==7)return e->next_mode;
        value=o<2?e->quantity:o<4?e->item_index:e->next_id;
    }
    return (uint8_t)((o&1u)?value:value>>8);
}
static void money_item_event_byte_write(NpeWorld *w,unsigned money,unsigned offset,uint8_t byte){
    unsigned s=offset/8,o=offset%8;uint16_t *word;
    if(money){
        NpeMapMoneyEvent *e=&w->money_events[s];
        if(o>=6){w->money_event_padding[s][o-6]=byte;return;}
        if(o==4){e->add_money=byte;return;}
        if(o==5){e->next_mode=byte;return;}
        word=o<2?&e->amount:&e->next_id;
    }else{
        NpeMapItemEvent *e=&w->item_events[s];
        if(o==6){e->add_item=byte;return;}
        if(o==7){e->next_mode=byte;return;}
        word=o<2?&e->quantity:o<4?&e->item_index:&e->next_id;
    }
    *word=(uint16_t)((o&1u)?(*word&0xff00u)|byte:(*word&255u)|((unsigned)byte<<8));
}
