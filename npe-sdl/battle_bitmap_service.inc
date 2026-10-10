/* Pure C control flow of45:8ED8..91CC. IO and drawing are explicit
   engine boundaries; neither a host bitmap nor a fixed RAM target is used. */
static uint8_t bitmap_guest_byte(const NpeResidentHeap *heap,uint32_t address){
    uint16_t word=heap->read_word(heap->context,address&~UINT32_C(1));
    return (uint8_t)((address&1u)?word:word>>8);
}
static int32_t bitmap_signed_word(uint16_t word){
    return word<0x8000u?(int32_t)word:(int32_t)word-0x10000;
}
int npe_battle_bitmap_service(const NpeBattleBitmapService *service,
                             uint16_t number,int16_t x,int16_t y,uint8_t kind){
    if(!service || !service->heap.read_word || !service->heap.write_word || !service->draw)return -1;
    const NpeResidentHeap *heap=&service->heap;
    uint32_t address;
    if(number<=29){
        uint32_t packed=heap_long(heap,UINT32_C(0x5279a6));
        uint16_t displacement=(uint16_t)(3u*(uint16_t)(number-1u));
        uint32_t entry=packed+(uint32_t)bitmap_signed_word(displacement);
        uint32_t offset=bitmap_guest_byte(heap,entry);
        offset|=(uint32_t)bitmap_guest_byte(heap,entry+1)<<8;
        offset|=(uint32_t)bitmap_guest_byte(heap,entry+2)<<16;
        address=packed+offset;
    }else{
        if(!service->seek || !service->read_local || !service->read_guest)return -1;
        uint8_t directory[5]={0};
        uint16_t displacement=(uint16_t)(6u*(uint16_t)(number-29u));
        service->seek(service->context,heap_long(heap,UINT32_C(0x5279ca))+displacement);
        /* Original overlapping FFEC/FFEE locals: each read requests3.
           Partial/zero reads retain the initialized/previous local bytes. */
        service->read_local(service->context,directory,3);
        service->read_local(service->context,directory+2,3);
        uint16_t relative=(uint16_t)((unsigned)directory[0]<<8|directory[1]);
        uint16_t count=(uint16_t)((unsigned)directory[2]<<8|directory[3]);
        uint16_t handle=npe_resident_heap_allocate(heap,(uint16_t)(count+1u));
        heap->write_word(heap->context,UINT32_C(0x527a88),handle);
        /* The original caller resolves handle0 even after allocation fails. */
        address=npe_resident_heap_lock(heap,heap->read_word(heap->context,UINT32_C(0x527a88)));
        service->seek(service->context,heap_long(heap,UINT32_C(0x5279ce))+(uint32_t)bitmap_signed_word(relative));
        service->read_guest(service->context,address,count);
    }
    service->draw(service->context,address,x,y,kind<2?kind:2);
    if(number>29){
        /* Drawing/reading may overwrite the global. Reread7A88 for each
           service just as45:919C..91B6 does, rather than keeping a local ID. */
        npe_resident_heap_unlock(heap,heap->read_word(heap->context,UINT32_C(0x527a88)));
        npe_resident_heap_release(heap,heap->read_word(heap->context,UINT32_C(0x527a88)));
    }
    return 0;
}
