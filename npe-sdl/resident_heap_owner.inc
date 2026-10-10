/* Original31:4974..4A30 and launcher1B:23CE..2496. All addresses remain
   live RAM owners; no captured heap headers or payload addresses are seeded. */
int npe_resident_heap_allocate_owned(const NpeResidentHeapOwnerService *service,
                                    uint32_t handle_address,uint32_t request,
                                    uint32_t pointer_address){
    if(!service||!service->heap.read_word||!service->heap.write_word)return -1;
    const NpeResidentHeap *heap=&service->heap;
    uint16_t handle=npe_resident_heap_allocate(heap,(uint16_t)request);
    heap->write_word(heap->context,handle_address,handle);
    if(!heap->read_word(heap->context,handle_address)){
        if(service->owner_failed)service->owner_failed(service->context);
        return 0;
    }
    uint32_t pointer=npe_resident_heap_lock(heap,heap->read_word(heap->context,handle_address));
    heap_set_long(heap,pointer_address,pointer);
    return 1;
}
int npe_resident_heap_launcher_buffers(const NpeResidentHeapOwnerService *service){
    static const uint32_t handles[]={0x52574c,0x52574e,0x525760,0x525626};
    static const uint32_t pointers[]={0x525186,0x52518a,0x52518e,0x525192};
    for(unsigned i=0;i<4;++i){
        int result=npe_resident_heap_allocate_owned(service,handles[i],256,pointers[i]);
        if(result!=1)return result;
    }
    return 1;
}
void npe_resident_heap_cleanup(const NpeResidentHeap *heap,uint16_t handle){
    if(!handle)return;
    npe_resident_heap_unlock(heap,handle);
    npe_resident_heap_release(heap,handle);
}
int npe_resident_heap_launcher_cleanup(const NpeResidentHeap *heap){
    if(!heap||!heap->read_word||!heap->write_word)return -1;
    static const uint32_t handles[]={0x52574c,0x52574e,0x525760,0x525626};
    for(unsigned i=0;i<4;++i)npe_resident_heap_cleanup(heap,heap->read_word(heap->context,handles[i]));
    return 0;
}
int npe_rpg_map_buffer_allocate(const NpeResidentHeap *heap){
    if(!heap||!heap->read_word||!heap->write_word)return -1;
    heap->write_word(heap->context,0x527a6a,npe_resident_heap_allocate(heap,7000));
    uint32_t pointer=npe_resident_heap_lock(heap,heap->read_word(heap->context,0x527a6a));
    heap_set_long(heap,0x52797e,pointer);
    return 0;
}
int npe_rpg_battle_packed_load(const NpeRpgResourceService *service){
    if(!service||!service->heap.read_word||!service->seek||!service->read_guest)return -1;
    const NpeResidentHeap *heap=&service->heap;
    uint16_t count=(uint16_t)heap_long(heap,0x5279be);
    service->seek(service->context,heap_long(heap,0x5279c2));
    service->read_guest(service->context,heap_long(heap,0x5279a6),count);
    return 0;
}
int npe_rpg_battle_resources_allocate(const NpeRpgResourceService *service){
    if(!service||!service->heap.read_word||!service->heap.write_word||
       !service->seek||!service->read_guest)return -1;
    const NpeResidentHeap *heap=&service->heap;
    heap->write_word(heap->context,0x527a88,npe_resident_heap_allocate(heap,32768));
    uint32_t pointer=npe_resident_heap_lock(heap,heap->read_word(heap->context,0x527a88));
    heap_set_long(heap,0x5279a6,pointer);
    uint16_t flags=heap->read_word(heap->context,0x527aa4);
    heap->write_word(heap->context,0x527aa4,(uint16_t)(0x0100u|(flags&255u)));
    return npe_rpg_battle_packed_load(service);
}
int npe_rpg_battle_resources_release(const NpeResidentHeap *heap){
    if(!heap||!heap->read_word||!heap->write_word)return -1;
    /*42:8E72..8E9A unconditionally calls both services, rereading7A88.
      It retains the released handle and packed pointer; only7AA4 clears. */
    npe_resident_heap_unlock(heap,heap->read_word(heap->context,0x527a88));
    npe_resident_heap_release(heap,heap->read_word(heap->context,0x527a88));
    heap->write_word(heap->context,0x527aa4,(uint16_t)(heap->read_word(heap->context,0x527aa4)&255u));
    return 0;
}
int npe_rpg_finish_game_service(const NpeResidentHeap *heap){
    if(!heap||!heap->read_word||!heap->write_word)return -1;
    /*4A:F59A..F5D2 releases only the map, rereading its handle. The
      battle flag, published pointers, event word and audio remain live. */
    npe_resident_heap_unlock(heap,heap->read_word(heap->context,0x527a6a));
    npe_resident_heap_release(heap,heap->read_word(heap->context,0x527a6a));
    heap->write_word(heap->context,0x527934,7);
    heap->write_word(heap->context,0x527ab4,1);
    return 0;
}
int npe_rpg_resources_release(const NpeResidentHeap *heap){
    if(!heap||!heap->read_word||!heap->write_word)return -1;
    /*3C:D50E..D564 rereads the map handle for each service. The battle
      high byte is compared with1, rather than treated as a boolean. */
    npe_resident_heap_unlock(heap,heap->read_word(heap->context,0x527a6a));
    npe_resident_heap_release(heap,heap->read_word(heap->context,0x527a6a));
    if((heap->read_word(heap->context,0x527aa4)>>8)==1)
        return npe_rpg_battle_resources_release(heap);
    return 0;
}
