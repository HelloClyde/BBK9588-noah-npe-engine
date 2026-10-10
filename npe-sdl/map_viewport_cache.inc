/* Original4C:E15A..E27E / CPU2F:E15A. Always105 words; signed
   linear positions and live source pointer, without logical-grid bounds. */
int npe_map_viewport_cache_refresh(const NpeResidentHeap *heap){
    if(!heap || !heap->read_word || !heap->write_word)return -1;
    uint16_t row=bitmap_guest_byte(heap,UINT32_C(0x527a2c));
    uint16_t column=bitmap_guest_byte(heap,UINT32_C(0x527a2e));
    uint16_t position=(uint16_t)(column+row*bitmap_guest_byte(heap,UINT32_C(0x527a56)));
    for(unsigned i=0;i<105;++i){
        if(i && i%15==0){
            column=bitmap_guest_byte(heap,UINT32_C(0x527a2e));
            row=(uint16_t)(row+1u);
            position=(uint16_t)(column+row*bitmap_guest_byte(heap,UINT32_C(0x527a56)));
        }
        int32_t index=bitmap_signed_word(position);
        uint32_t source=heap_long(heap,UINT32_C(0x52797e))+(uint32_t)(2*index);
        uint16_t tile=heap->read_word(heap->context,source);
        heap->write_word(heap->context,UINT32_C(0x5270b4)+2u*i,tile);
        position=(uint16_t)(position+1u);
    }
    return 0;
}
