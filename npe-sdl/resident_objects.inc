/* Source29:BFBA..C130 reads eight10-byte file records into8-byte slots;
   source bytes8/9 are discarded. All eight resident bytes are overwritten. */
int npe_world_load_object_records(NpeWorld *w,const NpeMap *map){
    if(!w||!map||map->tail_size<80)return -1;
    for(unsigned i=0;i<8;++i){
        const uint8_t *v=map->tail+i*10;uint8_t *r=w->object_records[i];
        memcpy(r,v,2);memcpy(r+2,v+4,2);memcpy(r+4,v+2,2);r[6]=v[6];r[7]=v[7];
    }
    w->object_records_ready=1;return 0;
}
int npe_world_object(const NpeWorld *w,const NpeMap *map,unsigned i,NpeMapObject *object){
    if(!w||!object||i>=8)return -1;
    if(!w->object_records_ready)return npe_map_object_info(map,i,object);
    const uint8_t *r=w->object_records[i];
    object->id=resident_record_word(r,0);object->item_index=resident_record_word(r,4);
    object->x=r[6];object->y=r[7];object->raw=NULL;return 0;
}
static int world_service_byte_read(const NpeWorld *w,uint32_t address,uint8_t *byte){
    if(!npe_world_resident_read(w,address,byte,1))return 0;
    const NpeFile *file=w->battle_file;
    if(file&&npe_rpg_runtime_owner(file)==w&&!npe_rpg_runtime_read(file,address,byte,1))return 0;
    return -2;
}
static uint32_t object_flag_address(unsigned id){
    uint16_t word=(uint16_t)(id-1u);
    int32_t offset=word<32768u?(int32_t)word:(int32_t)word-65536;
    return (uint32_t)(INT32_C(0x52742c)+offset);
}
int npe_world_object_flag(const NpeWorld *w,unsigned id){
    uint8_t byte;if(!w)return -1;
    if(world_service_byte_read(w,object_flag_address(id),&byte))return -2;
    return byte;
}
static int world_object_flag_set(NpeWorld *w,unsigned id){
    uint32_t address=object_flag_address(id);const uint8_t byte=1;
    if(!npe_world_resident_write(w,address,&byte,1))return 0;
    const NpeFile *file=w->battle_file;
    if(file&&npe_rpg_runtime_owner(file)==w&&!npe_rpg_runtime_write(file,address,&byte,1))return 0;
    return -2;
}
