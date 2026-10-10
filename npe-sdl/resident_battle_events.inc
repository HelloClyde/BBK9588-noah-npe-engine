int npe_world_load_random_enemy_records(NpeWorld *world,const NpeMap *map){
    if(!world||!map||map->tail_size<652)return -1;
    /*29:CC60..CD28 clears each destination byte and appends each
      nonzero file byte at the next compact index. No signed filtering. */
    uint8_t loaded[8]={0};unsigned count=0;
    for(unsigned i=0;i<8;++i)if(map->tail[644+i])loaded[count++]=map->tail[644+i];
    memcpy(world->random_enemy_records,loaded,8);
    /*29:F21E..F244 /46:D2B2..D2D4 use a signed first byte. */
    world->encounters_enabled=(uint16_t)(loaded[0]>0&&loaded[0]<128);return 0;
}
int npe_world_load_battle_events(NpeWorld *world,const NpeMap *map){
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    uint8_t loaded[4][8];memcpy(loaded,world->battle_event_records,sizeof(loaded));
    for(unsigned i=0;i<4;++i){NpeMapBattleEvent event;int found=npe_map_battle_event(map,i+1,&event);
        if(found<0)return -1;
        uint8_t *r=loaded[i];r[0]=0;
        if(!found)continue;
        r[0]=event.formation_key;r[1]=event.win_mode;r[2]=event.lose_mode;
        r[4]=(uint8_t)(event.win_event_id>>8);r[5]=(uint8_t)event.win_event_id;
        r[6]=(uint8_t)(event.lose_event_id>>8);r[7]=(uint8_t)event.lose_event_id;
    }
    memcpy(world->battle_event_records,loaded,sizeof(loaded));return 0;
}
int npe_world_battle_event(const NpeFile *file,const NpeWorld *world,
                           unsigned id,NpeMapBattleEvent *event){
    if(!file||!world||!event)return -1;
    /*39:AF5C..AF8A and33:BD0C..BD82 / C50E..C588 use the full
      event word, subtract one, sign extend, and multiply by eight. */
    uint16_t index=(uint16_t)(id-1u);
    int32_t signed_index=index<32768u?(int32_t)index:(int32_t)index-65536;
    uint32_t address=(uint32_t)(INT32_C(0x5277fc)+signed_index*8);
    uint8_t r[8];
    for(unsigned i=0;i<sizeof(r);++i){
        if(!npe_world_resident_read(world,address+i,r+i,1))continue;
        if(npe_rpg_runtime_owner(file)!=world||npe_rpg_runtime_read(file,address+i,r+i,1))return -2;
    }
    memset(event,0,sizeof(*event));event->formation_key=r[0];event->win_mode=r[1];event->lose_mode=r[2];
    event->win_event_id=(uint16_t)(((unsigned)r[4]<<8)|r[5]);
    event->lose_event_id=(uint16_t)(((unsigned)r[6]<<8)|r[7]);return 1;
}
