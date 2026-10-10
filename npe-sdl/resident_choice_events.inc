int npe_world_load_choice_events(NpeWorld *world,const NpeMap *map){
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    uint8_t loaded[5][6];memcpy(loaded,world->choice_event_records,sizeof(loaded));
    for(unsigned i=0;i<5;++i){NpeMapChoiceEvent event;int found=npe_map_choice_event(map,i+1,&event);
        if(found<0)return -1;
        if(!found)break;
        uint8_t *r=loaded[i];r[0]=event.mode[0];r[1]=event.mode[1];
        for(unsigned j=0;j<2;++j){r[2+j*2]=(uint8_t)(event.next_id[j]>>8);r[3+j*2]=(uint8_t)event.next_id[j];}
    }
    memcpy(world->choice_event_records,loaded,sizeof(loaded));return 0;
}
int npe_world_choice_event(const NpeFile *file,const NpeWorld *world,
                           unsigned id,NpeMapChoiceEvent *event){
    if(!file||!world||!event)return -1;
    /*45:D3BA/D408 subtract in word width, then sign extend the full word.
      No five-slot or low-byte guard precedes either native branch. */
    uint16_t index=(uint16_t)(id-1u);
    int32_t signed_index=index<32768u?(int32_t)index:(int32_t)index-65536;
    uint32_t address=(uint32_t)(INT32_C(0x52781c)+signed_index*6);
    uint8_t r[6];
    for(unsigned i=0;i<sizeof(r);++i){
        if(!npe_world_resident_read(world,address+i,r+i,1))continue;
        if(npe_rpg_runtime_owner(file)!=world||npe_rpg_runtime_read(file,address+i,r+i,1))return -2;
    }
    memset(event,0,sizeof(*event));event->id=(uint16_t)id;
    event->mode[0]=r[0];event->mode[1]=r[1];
    event->next_id[0]=(uint16_t)(((unsigned)r[2]<<8)|r[3]);
    event->next_id[1]=(uint16_t)(((unsigned)r[4]<<8)|r[5]);return 1;
}
