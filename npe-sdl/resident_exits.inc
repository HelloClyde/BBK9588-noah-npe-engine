/* Original source29:C420..C998 reads all twelve21-byte file records into24-byte
   RAM slots, retaining byte3 and zero-extending each event byte to a word. */
int npe_world_load_exit_records(NpeWorld *w,const NpeMap *map){
    if(!w||!map||map->tail_size<592)return -1;
    for(unsigned i=0;i<12;++i){
        const uint8_t *v=map->tail+340+i*21;uint8_t *r=w->exit_records[i];
        memcpy(r,v,3);memcpy(r+4,v+3,8);r[12]=0;r[13]=v[11];
        memcpy(r+14,v+12,8);r[22]=0;r[23]=v[20];
    }
    w->exit_records_ready=1;return 0;
}
int npe_world_exit(const NpeWorld *w,const NpeMap *map,unsigned i,NpeMapExit *exit){
    if(!w||!exit||i>=12)return -1;
    if(!w->exit_records_ready)return npe_map_exit(map,i,exit);
    const uint8_t *r=w->exit_records[i];
    exit->x=r[0];exit->y=r[1];exit->kind=r[2];
    exit->destination_map=resident_record_word(r,4);exit->min_progress=resident_record_word(r,6);
    exit->destination_x=r[8];exit->destination_y=r[9];exit->next_mode=r[11];
    exit->next_id=resident_record_word(r,12);
    exit->second_map=resident_record_word(r,14);exit->second_min_progress=resident_record_word(r,16);
    exit->second_x=r[18];exit->second_y=r[19];exit->second_mode=r[21];
    exit->second_id=resident_record_word(r,22);
    exit->raw=NULL; /* This is a resident projection, not a21-byte file view. */
    return 0;
}
