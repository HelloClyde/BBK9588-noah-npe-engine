int npe_world_load_scene_events(NpeWorld *world,const NpeMap *map){
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    uint8_t loaded[5][16];memcpy(loaded,world->scene_event_records,sizeof(loaded));
    for(unsigned i=0;i<5;++i){NpeMapAnimationEvent event;int found=npe_map_animation_event(map,i+1,&event);
        if(found<0)return -1;
        if(!found)break;
        uint8_t *r=loaded[i];r[0]=(uint8_t)(event.next_id>>8);r[1]=(uint8_t)event.next_id;
        r[2]=(uint8_t)event.id;r[3]=event.marker;r[4]=event.draw_mode;r[5]=event.erase_mode;r[6]=event.next_mode;
        const uint16_t coordinates[]={event.x,event.y,event.x2,event.y2};
        for(unsigned j=0;j<4;++j){r[8+j*2]=(uint8_t)(coordinates[j]>>8);r[9+j*2]=(uint8_t)coordinates[j];}
    }
    memcpy(world->scene_event_records,loaded,sizeof(loaded));return 0;
}
int npe_world_scene_event(const NpeWorld *world,unsigned id,NpeMapAnimationEvent *event){
    if(!world||!event)return -1;
    id&=255u;if(!id||id>5)return 0;
    const uint8_t *r=world->scene_event_records[id-1];memset(event,0,sizeof(*event));
    event->next_id=(uint16_t)(((unsigned)r[0]<<8)|r[1]);event->id=r[2];event->marker=r[3];
    event->draw_mode=r[4];event->erase_mode=r[5];event->next_mode=r[6];
    event->x=(uint16_t)(((unsigned)r[8]<<8)|r[9]);event->y=(uint16_t)(((unsigned)r[10]<<8)|r[11]);
    event->x2=(uint16_t)(((unsigned)r[12]<<8)|r[13]);event->y2=(uint16_t)(((unsigned)r[14]<<8)|r[15]);
    return 1;
}
static uint8_t skill_adjust_event_byte(const NpeWorld *w,unsigned adjust,unsigned offset){
    if(adjust){const NpeMapPartyAdjustEvent *e=&w->party_adjust_event;
        if(offset==0)return e->character_id;
        if(offset==1)return e->next_mode;
        return (uint8_t)((offset&1u)?e->next_id:e->next_id>>8);
    }
    unsigned slot=offset/6,o=offset%6;const NpeMapSkillEvent *e=&w->skill_events[slot];
    if(o<2)return (uint8_t)((o&1u)?e->next_id:e->next_id>>8);
    if(o==2)return e->skill_id;
    if(o==3)return e->character_id;
    if(o==4)return e->next_mode;
    return w->skill_event_padding[slot];
}
static void skill_adjust_event_byte_write(NpeWorld *w,unsigned adjust,unsigned offset,uint8_t byte){
    uint16_t *word;
    if(adjust){NpeMapPartyAdjustEvent *e=&w->party_adjust_event;
        if(offset==0){e->character_id=byte;return;}
        if(offset==1){e->next_mode=byte;return;}
        word=&e->next_id;
    }else{
        unsigned slot=offset/6,o=offset%6;NpeMapSkillEvent *e=&w->skill_events[slot];
        if(o==2){e->skill_id=byte;return;}
        if(o==3){e->character_id=byte;return;}
        if(o==4){e->next_mode=byte;return;}
        if(o==5){w->skill_event_padding[slot]=byte;return;}
        word=&e->next_id;
    }
    *word=(uint16_t)((offset&1u)?(*word&0xff00u)|byte:(*word&255u)|((unsigned)byte<<8));
}
