/* Source4C:0560..06B6 and38:DFAE..E0BC/E548..E6DE.
   Coordinates and voice descriptors are physical record bytes, independent
   of the current map scroll and the audio playback cursor. */
int npe_party_battle_positions_initialize(const NpeResidentHeap *h){
    if(!h||!h->read_word||!h->write_word)return -1;
    static const uint8_t party[]={168,51,136,75,168,51,136,75,168,51,200,28};
    static const uint8_t enemy[]={42,38,82,16,42,38,82,16,42,38,2,54};
    for(unsigned i=0;i<12;++i){
        enemy_guest_byte_write(h,UINT32_C(0x527700)+i,party[i]);
        enemy_guest_byte_write(h,UINT32_C(0x527714)+i,enemy[i]);
    }
    return 0;
}
int npe_party_battle_place(const NpeResidentHeap *h){
    if(!h||!h->read_word||!h->write_word)return -1;
    unsigned count=bitmap_guest_byte(h,UINT32_C(0x527afa));
    unsigned first=count*(count-1u)/2u;
    for(unsigned m=0;m<bitmap_guest_byte(h,UINT32_C(0x527afa));++m){
        uint32_t a=UINT32_C(0x526450)+148u*m;
        for(unsigned i=0;i<2;++i){
            uint16_t offset=(uint16_t)(2u*(first+m)+i);
            enemy_guest_byte_write(h,a+124+i,
                bitmap_guest_byte(h,UINT32_C(0x527700)+(uint32_t)bitmap_signed_word(offset)));
        }
    }
    return 0;
}
int npe_party_battle_audio(const NpeResidentHeap *h){
    if(!h||!h->read_word||!h->write_word)return -1;
    for(unsigned m=0;m<bitmap_guest_byte(h,UINT32_C(0x527afa));++m){
        uint32_t a=UINT32_C(0x526450)+148u*m;
        unsigned sex=bitmap_guest_byte(h,a+17);
        if(sex==1){
            heap_set_long(h,a+114,heap_long(h,UINT32_C(0x527946)));
            heap_set_long(h,a+118,heap_long(h,UINT32_C(0x527942)));
        }else if(sex==2){
            heap_set_long(h,a+114,heap_long(h,UINT32_C(0x527966)));
            heap_set_long(h,a+118,heap_long(h,UINT32_C(0x527962)));
        }
    }
    return 0;
}
