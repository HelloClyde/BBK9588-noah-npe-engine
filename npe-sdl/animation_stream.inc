/* Source41:AD2A..AEF8 and AFB6..B0C8 (CODE24). Later frame descriptors
   are not reread: the first length controls the complete byte stream. */
static int animation_stream_open(const NpeBattleBitmapService *service,int16_t group,
                                 NpeAnimationStream *stream,uint32_t directory_slot,uint32_t payload_slot){
    if(!service||!stream||stream->active||group<=0||!service->heap.read_word||
       !service->heap.write_word||!service->seek||
       !service->read_local||!service->read_guest||!service->draw)return -1;
    uint8_t relative_bytes[4]={0},count=0,offset_bytes[4]={0},length_bytes[4]={0};
    const NpeResidentHeap *heap=&service->heap;
    int32_t displacement=bitmap_signed_word((uint16_t)((uint16_t)group*5u-5u));
    service->seek(service->context,heap_long(heap,directory_slot)+(uint32_t)displacement);
    service->read_local(service->context,relative_bytes,4);
    service->read_local(service->context,&count,1);
    uint32_t relative=indexed_local_long(relative_bytes);
    if(relative>=UINT32_C(0x80000000)||!count)return -3;
    service->seek(service->context,heap_long(heap,directory_slot)+relative);
    service->read_local(service->context,offset_bytes,4);
    service->read_local(service->context,length_bytes,4);
    uint32_t offset=indexed_local_long(offset_bytes),length=indexed_local_long(length_bytes);
    if(offset>=UINT32_C(0x80000000)||!length)return -1;
    stream->handle=npe_resident_heap_allocate(heap,(uint16_t)(length+1u));
    stream->address=npe_resident_heap_lock(heap,stream->handle);
    stream->length=length;stream->group=(uint16_t)group;
    stream->frames=count;stream->position=0;stream->active=1;
    service->seek(service->context,heap_long(heap,payload_slot)+offset);
    return 0;
}
int npe_scene_animation_stream_open(const NpeBattleBitmapService *service,int16_t group,
                                     NpeAnimationStream *stream){
    return animation_stream_open(service,group,stream,UINT32_C(0x5279f6),UINT32_C(0x5279fa));
}
int npe_role_animation_stream_open(const NpeBattleBitmapService *service,int16_t group,
                                    NpeAnimationStream *stream){
    return animation_stream_open(service,group,stream,UINT32_C(0x5279de),UINT32_C(0x5279e2));
}
int npe_animation_stream_draw(const NpeBattleBitmapService *service,NpeAnimationStream *stream,
                               int16_t x,int16_t y,uint8_t kind){
    if(!service||!stream||!stream->active||
       (stream->frames!=255 && stream->position>=stream->frames))return -1;
    service->read_guest(service->context,stream->address,(uint16_t)stream->length);
    service->draw(service->context,stream->address,x,y,kind);
    ++stream->position;return 0;
}
void npe_animation_stream_close(const NpeResidentHeap *heap,NpeAnimationStream *stream){
    if(!heap||!stream||!stream->active)return;
    npe_resident_heap_unlock(heap,stream->handle);
    npe_resident_heap_release(heap,stream->handle);
    stream->active=0;
}
