/* Source4E:A3F4..A496, CPU31:A3F4. Its caller supplies the music gate.
   This service is unconditional, including the settings-on pre-store call. */
int npe_map_sound_request(const NpeResidentHeap *h,NpeMapSoundRequest *request){
    if(!h || !h->read_word || !request)return -1;
    unsigned slot=bitmap_guest_byte(h,UINT32_C(0x527a04));
    *request=(NpeMapSoundRequest){UINT32_C(0x526eba),
        heap_long(h,UINT32_C(0x5278fe)+4u*slot),
        heap_long(h,UINT32_C(0x5278f2)+4u*slot),UINT32_C(0x31a3f4)};
    return 1;
}
