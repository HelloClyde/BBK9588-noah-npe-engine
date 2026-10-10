/* Original2B:0000..0246. The directory guard and seek displacement use
   independently wrapped signed16 arithmetic. IO counts use the low word. */
static int64_t indexed_signed_long(uint32_t value){
    return value<UINT32_C(0x80000000)?(int64_t)value:(int64_t)value-INT64_C(0x100000000);
}
static uint32_t indexed_local_long(const uint8_t *bytes){
    return (uint32_t)bytes[0]<<24|(uint32_t)bytes[1]<<16|(uint32_t)bytes[2]<<8|bytes[3];
}
int npe_indexed_bitmap_service(const NpeBattleBitmapService *service,int16_t index,
                               int16_t x,int16_t y,int16_t kind){
    if(!service||!service->heap.read_word||!service->heap.write_word||
       !service->seek||!service->read_local||!service->read_guest||!service->draw)return -1;
    const NpeResidentHeap *heap=&service->heap;
    uint16_t shifted=(uint16_t)((uint16_t)index*8u);
    int32_t end=bitmap_signed_word((uint16_t)(shifted+8u));
    if(index<0||kind<0||kind>2||
       end>indexed_signed_long(heap_long(heap,UINT32_C(0x5279c6))))return -1;
    uint8_t offset_bytes[4]={0},length_bytes[4]={0};
    service->seek(service->context,heap_long(heap,UINT32_C(0x5279ca))+(uint32_t)bitmap_signed_word(shifted));
    service->read_local(service->context,offset_bytes,4);
    service->read_local(service->context,length_bytes,4);
    uint32_t relative=indexed_local_long(offset_bytes),length=indexed_local_long(length_bytes);
    if(relative>=UINT32_C(0x80000000)||!length)return -3;
    uint16_t handle=npe_resident_heap_allocate(heap,(uint16_t)(length+1u));
    uint32_t address=npe_resident_heap_lock(heap,handle);
    service->seek(service->context,heap_long(heap,UINT32_C(0x5279ce))+relative);
    service->read_guest(service->context,address,(uint16_t)length);
    service->draw(service->context,address,x,y,(uint8_t)kind);
    /* Unlike the battle service, ownership remains in the local frame. */
    npe_resident_heap_unlock(heap,handle);npe_resident_heap_release(heap,handle);
    return 0;
}
