/* Group5 and group6 are adjacent native tables; only effective service fields
   have C members. Explicit projections preserve all seven opaque bytes. */
static uint8_t party_teleport_byte(const NpeWorld *w,unsigned offset){
    uint16_t value;
    if(offset<20){
        unsigned s=offset/10,o=offset%10;const NpeMapTeleportEvent *e=&w->teleport_events[s];
        if(o==2||o==3)return w->teleport_padding[s][o-2];
        if(o==4)return e->x;
        if(o==5)return e->y;
        if(o==6)return e->marker;
        if(o==7)return e->next_mode;
        value=o<2?e->destination_map:e->next_id;
        return (uint8_t)((o&1u)?value:value>>8);
    }
    unsigned s=(offset-20)/6,o=(offset-20)%6;const NpeMapPartyEvent *e=&w->party_events[s];
    if(o==0)return e->add_member;
    if(o==1)return e->character_id;
    if(o==2)return e->next_mode;
    if(o==3)return w->party_event_padding[s];
    return (uint8_t)((o&1u)?e->next_id:e->next_id>>8);
}
static void party_teleport_byte_write(NpeWorld *w,unsigned offset,uint8_t value){
    uint16_t *word;unsigned o;
    if(offset<20){
        unsigned s=offset/10;o=offset%10;NpeMapTeleportEvent *e=&w->teleport_events[s];
        if(o==2||o==3){w->teleport_padding[s][o-2]=value;return;}
        if(o==4){e->x=value;return;}
        if(o==5){e->y=value;return;}
        if(o==6){e->marker=value;return;}
        if(o==7){e->next_mode=value;return;}
        word=o<2?&e->destination_map:&e->next_id;
    }else{
        unsigned s=(offset-20)/6;o=(offset-20)%6;NpeMapPartyEvent *e=&w->party_events[s];
        if(o==0){e->add_member=value;return;}
        if(o==1){e->character_id=value;return;}
        if(o==2){e->next_mode=value;return;}
        if(o==3){w->party_event_padding[s]=value;return;}
        word=&e->next_id;
    }
    *word=(uint16_t)((o&1u)?(*word&0xff00u)|value:(*word&255u)|((unsigned)value<<8));
}
