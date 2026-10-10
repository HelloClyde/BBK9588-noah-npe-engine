/* Original0C:D238..D584. The file cursor already points at group0.
   Record counts are local bytes; the resident slots contain no count. */
int npe_map_tile_record_service(const NpeBattleBitmapService *service,uint8_t count){
    if(!service || !service->heap.read_word || !service->heap.write_word ||
       !service->read_local || !service->read_guest)return -1;
    const NpeResidentHeap *heap=&service->heap;
    for(unsigned i=0;i<3;++i){
        uint32_t base=UINT32_C(0x52724e)+64u*i;
        if(i<count){
            uint8_t pairs=0;
            service->read_guest(service->context,base,2);
            service->read_local(service->context,&pairs,1);
            for(unsigned j=0;j<pairs;++j){
                uint32_t index_address=base+2+2u*j;
                uint32_t tile_address=base+32+2u*j;
                service->read_guest(service->context,index_address,2);
                service->read_guest(service->context,tile_address,2);
                uint16_t progress=heap->read_word(heap->context,UINT32_C(0x527a66));
                if(progress>=heap->read_word(heap->context,base)){
                    uint16_t index=heap->read_word(heap->context,index_address);
                    int32_t signed_index=index<0x8000u?(int32_t)index:(int32_t)index-65536;
                    uint32_t destination=heap_long(heap,UINT32_C(0x52797e))+
                                         (uint32_t)(2*signed_index);
                    uint16_t tile=heap->read_word(heap->context,tile_address);
                    heap->write_word(heap->context,destination,tile);
                }
            }
            /* Clear only the high flag byte; retain the adjacent byte. */
            uint16_t flags=heap->read_word(heap->context,base+62);
            heap->write_word(heap->context,base+62,(uint16_t)(flags&255u));
        }else{
            heap->write_word(heap->context,base,0);
            for(unsigned j=0;j<15;++j){
                heap->write_word(heap->context,base+2+2u*j,0);
                heap->write_word(heap->context,base+32+2u*j,0);
            }
        }
    }
    return 0;
}
