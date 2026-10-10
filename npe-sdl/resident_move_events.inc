/* Fixed ten-byte move-event slots. C service fields and original RAM share
   storage through explicit big-endian byte projections; no host padding. */
static uint8_t move_event_byte(const NpeWorld *w,unsigned script,unsigned offset){
    unsigned s=offset/10,o=offset%10;uint16_t value;
    if(script){
        const NpeScriptWalkCache *e=&w->script_walk_events[s];
        if(o==0)return e->marker;
        if(o==1)return w->script_walk_padding[s][0];
        if(o==2)return e->next_mode;
        if(o==3)return w->script_walk_padding[s][1];
        value=o<6?e->next_id:o<8?e->x:e->y;
    }else{
        const NpeNpcMoveCache *e=&w->npc_move_events[s];
        if(o==2)return e->next_mode;
        if(o==3)return w->npc_move_padding[s];
        value=o<2?e->npc_id:o<6?e->next_id:o<8?e->x:e->y;
    }
    return (uint8_t)((o&1u)?value:value>>8);
}
static void move_event_byte_write(NpeWorld *w,unsigned script,unsigned offset,uint8_t value){
    unsigned s=offset/10,o=offset%10;uint16_t *word;
    if(script){
        NpeScriptWalkCache *e=&w->script_walk_events[s];
        if(o==0){e->marker=value;return;}
        if(o==1){w->script_walk_padding[s][0]=value;return;}
        if(o==2){e->next_mode=value;return;}
        if(o==3){w->script_walk_padding[s][1]=value;return;}
        word=o<6?&e->next_id:o<8?&e->x:&e->y;
    }else{
        NpeNpcMoveCache *e=&w->npc_move_events[s];
        if(o==2){e->next_mode=value;return;}
        if(o==3){w->npc_move_padding[s]=value;return;}
        word=o<2?&e->npc_id:o<6?&e->next_id:o<8?&e->x:&e->y;
    }
    *word=(uint16_t)((o&1u)?(*word&0xff00u)|value:(*word&255u)|((unsigned)value<<8));
}
