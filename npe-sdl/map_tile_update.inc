/* Original25:3082 type2 /42:328A..346E, including its real2F:E15A
   helper. The threshold/progress guard occurs once per slot. */
int npe_map_tile_update_service(const NpeResidentHeap *heap,NpeMapTileObserver observe,void *context){
    if(!heap || !heap->read_word || !heap->write_word)return -1;
    int updated=0;
    for(unsigned i=0;i<3;++i){
        uint32_t base=UINT32_C(0x52724e)+64u*i;
        uint16_t threshold=heap->read_word(heap->context,base);
        if(!threshold || heap->read_word(heap->context,UINT32_C(0x527a66))<threshold ||
           bitmap_guest_byte(heap,base+62))continue;
        for(unsigned j=0;j<15;++j){
            uint16_t index=heap->read_word(heap->context,base+2+2u*j);
            if(bitmap_signed_word(index)<=0)break;
            uint32_t address=heap_long(heap,UINT32_C(0x52797e))+(uint32_t)(2*bitmap_signed_word(index));
            uint16_t tile=heap->read_word(heap->context,base+32+2u*j);
            heap->write_word(heap->context,address,tile);
            if(observe)observe(context,index,tile);
        }
        enemy_guest_byte_write(heap,base+62,1);
        if(npe_map_viewport_cache_refresh(heap))return -1;
        ++updated;
    }
    return updated;
}
