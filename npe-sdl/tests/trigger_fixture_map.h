/* One diagnostic trigger row added to the current in-memory map copy. */
static void add_trigger_directory(NpeFile *file) {
    NpeMap map;const uint8_t *raw;size_t size;
    assert(!npe_map(file,0,&map)&&!npe_record(file,8,0,&raw,&size));
    if(map.tail[652+14*3+2])return;
    size_t offset=(size_t)(raw-file->bytes),tail=(size_t)(map.tail-raw);
    unsigned base=get_word(map.tail+652);
    size_t insert=tail+703+get_word(map.tail+652+14*3)-base,old=file->size;
    uint8_t *bytes=realloc(file->bytes,old+4+size+11);assert(bytes);file->bytes=bytes;
    uint8_t *copy=bytes+old+4;raw=bytes+offset;
    dword(bytes+old,4);memcpy(copy,raw,insert);memset(copy+insert,0,11);
    memcpy(copy+insert+11,raw+insert,size-insert);
    copy[tail+652+14*3+2]=1;
    for(unsigned i=15;i<17;++i){uint8_t *p=copy+tail+652+i*3;word(p,get_word(p)+11);}
    file->sections[8].offset=(uint32_t)old;file->sections[8].size=(uint32_t)(4+size+11);
    file->offset_table_records[8]=1;file->size=old+4+size+11;
}
