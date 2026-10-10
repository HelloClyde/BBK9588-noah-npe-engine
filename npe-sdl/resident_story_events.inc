/* Source29:EAA2..EBD2 uses next word,progress word,mode byte,padding. */
static uint8_t story_event_byte(const NpeWorld *w,unsigned offset){
    const NpeMapStoryEvent *e=&w->story_events[offset/6];
    switch(offset%6){
    case 0:return (uint8_t)(e->next_id>>8);
    case 1:return (uint8_t)e->next_id;
    case 2:return (uint8_t)(e->story_progress>>8);
    case 3:return (uint8_t)e->story_progress;
    case 4:return e->mode;
    default:return w->story_event_padding[offset/6];
    }
}
static void story_event_byte_write(NpeWorld *w,unsigned offset,uint8_t byte){
    NpeMapStoryEvent *e=&w->story_events[offset/6];
    switch(offset%6){
    case 0:e->next_id=(uint16_t)((e->next_id&255u)|((unsigned)byte<<8));break;
    case 1:e->next_id=(uint16_t)((e->next_id&0xff00u)|byte);break;
    case 2:e->story_progress=(uint16_t)((e->story_progress&255u)|((unsigned)byte<<8));break;
    case 3:e->story_progress=(uint16_t)((e->story_progress&0xff00u)|byte);break;
    case 4:e->mode=byte;break;
    default:w->story_event_padding[offset/6]=byte;break;
    }
}
