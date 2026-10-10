/* Source41:AF1E..AFA8,4D:058A..06A2 and4F:649C..6506.
   Slot/random preparation belongs to the caller. Lengths retain32 bits. */
static void background_sound_publish(const NpeResidentHeap *h,NpeRpgSoundRequest *r){
    *r=(NpeRpgSoundRequest){0x526eba,heap_long(h,0x52799a),heap_long(h,0x527996),NPE_SOUND_CALLBACK_BATTLE};
}
int npe_scene_sound_request(const NpeResidentHeap *h,unsigned slot,NpeRpgSoundRequest *r){
    if(!h || !h->read_word || !r)return -1;
    if(h->read_word(h->context,0x527a50)!=1)return 0;
    slot=(uint8_t)slot;
    *r=(NpeRpgSoundRequest){0x526eba,heap_long(h,0x5278d4+4u*slot),heap_long(h,0x5278c4+4u*slot),0};
    return 1;
}
int npe_battle_background_start(const NpeResidentHeap *h,unsigned roll,NpeRpgSoundRequest *r){
    if(!h || !h->read_word || !h->write_word || !r)return -1;
    if(!h->read_word(h->context,0x527a50))return 0;
    unsigned slot=((uint16_t)roll%60u)/10u;
    heap_set_long(h,0x52799a,heap_long(h,0x527872+4u*slot));
    heap_set_long(h,0x527996,heap_long(h,0x527856+4u*slot));
    background_sound_publish(h,r);return 1;
}
int npe_battle_background_replay(const NpeResidentHeap *h,NpeRpgSoundRequest *r){
    if(!h || !h->read_word || !r)return -1;
    if(!h->read_word(h->context,0x527a50))return 0;
    background_sound_publish(h,r);return 1;
}
