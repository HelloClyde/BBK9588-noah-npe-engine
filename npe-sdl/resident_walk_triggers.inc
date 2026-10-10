/*7490..74F3 aliases object flag bytes100..199. Use the existing owner so
  item/object writes and event reads observe the same resident bytes. */
static uint16_t walk_trigger_word(const uint8_t *r,unsigned offset){
    return (uint16_t)(((unsigned)r[offset]<<8)|r[offset+1]);
}
int npe_world_load_walk_triggers(NpeWorld *world,const NpeMap *map){
    uint8_t loaded[100];
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    memcpy(loaded,world->object_done+100,sizeof(loaded));
    for(unsigned i=0;i<10;++i){
        NpeMapMoveTrigger event;int result=npe_map_move_event(map,i+1,&event);
        if(result<0)return -1;
        uint8_t *r=loaded+10*i;
        if(result){
            record_word(r,0,event.event_id);record_word(r,2,event.min_progress);
            record_word(r,4,event.max_progress);record_word(r,6,event.tile_index);
            r[8]=event.mode;
        }else{
            /*29:F04A..F084 clears only the absent slot's position word. */
            record_word(r,6,0);
        }
    }
    memcpy(world->object_done+100,loaded,sizeof(loaded));return 0;
}
int npe_world_move_event(const NpeWorld *world,unsigned id,NpeMapMoveTrigger *event){
    if(!world||!event)return -1;
    id&=255u;if(!id||id>10)return 0;
    const uint8_t *r=world->object_done+100+10*(id-1);
    *event=(NpeMapMoveTrigger){.event_id=walk_trigger_word(r,0),
        .min_progress=walk_trigger_word(r,2),.max_progress=walk_trigger_word(r,4),
        .tile_index=walk_trigger_word(r,6),.mode=r[8],.raw=NULL};
    return 1;
}
int npe_world_move_trigger(const NpeWorld *world,const NpeMap *map,
                           unsigned x,unsigned y,NpeMapMoveTrigger *event){
    if(!world||!map||!event||x>=map->width||y>=map->height)return -1;
    unsigned target=y*map->width+x;
    /*3A:0602..075C scans all ten slots and compares progress unsigned.
      Position0 is a real comparison; no file-count or event-ID gate. */
    for(unsigned i=0;i<10;++i){
        NpeMapMoveTrigger candidate;
        if(npe_world_move_event(world,i+1,&candidate)!=1)return -1;
        if(candidate.tile_index!=target||world->story_progress<candidate.min_progress||
           world->story_progress>candidate.max_progress)continue;
        *event=candidate;return 1;
    }
    return 0;
}
