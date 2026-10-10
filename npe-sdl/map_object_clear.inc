/* Original25:3082 type1 / source42:30A8..3286. Negative slots scan
   flags0..99 in order, then all eight records for each flag exactly1. */
static void map_object_clear_slot(const NpeResidentHeap *heap,unsigned slot){
    uint32_t record=UINT32_C(0x52759e)+8u*slot;
    uint16_t y=bitmap_guest_byte(heap,record+7);
    uint16_t width=bitmap_guest_byte(heap,UINT32_C(0x527a56));
    uint16_t x=bitmap_guest_byte(heap,record+6);
    uint16_t index=(uint16_t)(y*width+x);
    uint32_t address=heap_long(heap,UINT32_C(0x52797e))+
        (uint32_t)(2*bitmap_signed_word(index));
    if(address&1u){
        /* RAM images use BE words, while an unaligned native MOV stores
           the low physical byte first. Its image locations are address-1
           and address+2, not two adjacent image bytes. */
        enemy_guest_byte_write(heap,address-1,53);
        enemy_guest_byte_write(heap,address+2,0);
    }else heap->write_word(heap->context,address,53);
}
int npe_map_object_clear_service(const NpeResidentHeap *heap,uint16_t slot){
    if(!heap || !heap->read_word || !heap->write_word)return -1;
    if(bitmap_signed_word(slot)>=0){map_object_clear_slot(heap,slot);return 1;}
    int cleared=0;
    for(unsigned flag=0;flag<100;++flag){
        if(bitmap_guest_byte(heap,UINT32_C(0x52742c)+flag)!=1)continue;
        for(unsigned record=0;record<8;++record){
            uint16_t id=heap->read_word(heap->context,UINT32_C(0x52759e)+8u*record);
            if((uint16_t)(id-1u)!=flag)continue;
            map_object_clear_slot(heap,record);++cleared;
        }
    }
    return cleared;
}
