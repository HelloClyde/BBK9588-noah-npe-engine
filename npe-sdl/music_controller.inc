/* Source37:CF5A..CFA2/CFDE..D028,37:1324..1368 and36:4CE4..4CFC.
   Drawing is owned by the caller. Observer events:1 replay,2 stop. */
static void music_controller_byte(const NpeResidentHeap *h,uint32_t a,uint8_t value){
    uint32_t base=a&~UINT32_C(1);uint16_t word=h->read_word(h->context,base);
    word=(uint16_t)((a&1u)?(word&0xff00u)|value:(word&255u)|((unsigned)value<<8));
    h->write_word(h->context,base,word);
}
int npe_music_controller_apply(const NpeResidentHeap *h,unsigned operation,
                               NpeMusicControllerObserver observer,void *context){
    if(!h || !h->read_word || !h->write_word || operation>3)return -1;
    if(operation==0){
        music_controller_byte(h,0x527a54,6);
        music_controller_byte(h,0x527a8a,h->read_word(h->context,0x527a50)==1?2:1);
    }else if(operation==1){
        unsigned enabled=(h->read_word(h->context,0x527a8a)>>8)==1;
        if(observer)observer(context,enabled?1:2);
        h->write_word(h->context,0x527a50,(uint16_t)enabled);
        h->write_word(h->context,0x527934,1);
        h->write_word(h->context,0x527ab4,1);
    }else if(operation==2){
        if(!h->read_word(h->context,0x527ab4))return 0;
        if(h->read_word(h->context,0x527a50)==1 && observer)observer(context,1);
        h->write_word(h->context,0x527ab4,0);
    }else{
        unsigned choice=h->read_word(h->context,0x527a8a)>>8;
        music_controller_byte(h,0x527a8a,(uint8_t)(choice%2u+1u));
    }
    return 1;
}
