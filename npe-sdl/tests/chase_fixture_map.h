/* Diagnostic map copy; never writes the original NPE. */
static unsigned get_word(const uint8_t *p) { return (unsigned)p[0]*256+p[1]; }
static void dword(uint8_t *p,unsigned v) { word(p,v>>16);word(p+2,v); }
static void add_chase_directory(NpeFile *file) {
    NpeMap map;const uint8_t *raw;size_t record_size;
    assert(!npe_map(file,0,&map)&&!npe_record(file,8,0,&raw,&record_size));
    assert(map.tail[652+10*3+2]==0);
    size_t raw_offset=(size_t)(raw-file->bytes),tail_offset=(size_t)(map.tail-raw);
    unsigned directory_base=get_word(map.tail+652);
    size_t insert=tail_offset+703+get_word(map.tail+652+10*3)-directory_base;
    size_t old_size=file->size,new_size=old_size+4+record_size+20;
    uint8_t *bytes=realloc(file->bytes,new_size);assert(bytes);file->bytes=bytes;
    uint8_t *copy=bytes+old_size+4;raw=bytes+raw_offset;
    dword(bytes+old_size,4);memcpy(copy,raw,insert);memset(copy+insert,0,20);
    memcpy(copy+insert+20,raw+insert,record_size-insert);
    copy[tail_offset+652+10*3+2]=2;
    for(unsigned i=11;i<17;++i){uint8_t *p=copy+tail_offset+652+i*3;word(p,get_word(p)+20);}
    /* Diagnostic in-memory map section: keep original resources; insert two
       ordinary current-NPE chase rows in an isolated copy of the map record. */
    file->sections[8].offset=(uint32_t)old_size;
    file->sections[8].size=(uint32_t)(4+record_size+20);
    file->offset_table_records[8]=1;file->size=new_size;
}
