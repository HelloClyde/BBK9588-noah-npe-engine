/* Original0C:BF0C..BFB4. The signed comparison uses the low word
   of width*height. Each cell is cleared before the2-byte file read,
   with the live pointer read again after clearing. */
int npe_map_grid_stream_service(const NpeBattleBitmapService *service){
    if(!service || !service->heap.read_word || !service->heap.write_word ||
       !service->read_guest)return -1;
    const NpeResidentHeap *heap=&service->heap;
    for(uint16_t i=0;;i=(uint16_t)(i+1u)){
        uint16_t cells=(uint16_t)(bitmap_guest_byte(heap,UINT32_C(0x527a56))*
                                  bitmap_guest_byte(heap,UINT32_C(0x527a5a)));
        if(bitmap_signed_word(i)>=bitmap_signed_word(cells))break;
        uint32_t displacement=(uint32_t)(2*bitmap_signed_word(i));
        uint32_t pointer=heap_long(heap,UINT32_C(0x52797e));
        heap->write_word(heap->context,pointer+displacement,0);
        pointer=heap_long(heap,UINT32_C(0x52797e));
        service->read_guest(service->context,pointer+displacement,2);
    }
    return 0;
}
