/* Pure C port of unchanged source3E:F85E..FD7E.
   The filename is the current-NPE resident name, not a game-specific asset.
   Selection writes7A98 even when nonzero music word is not exactly1. */
int npe_battle_sound_request(const NpeResidentHeap *h,NpeBattleSoundRequest *request){
    if(!h||!h->read_word||!h->write_word||!request)return -1;
    if(!h->read_word(h->context,UINT32_C(0x527a50)))return 0;
    enemy_guest_byte_write(h,UINT32_C(0x527a98),0);
    unsigned slot=bitmap_guest_byte(h,UINT32_C(0x527acc));
    unsigned party=bitmap_guest_byte(h,UINT32_C(0x527afa));
    uint32_t record=UINT32_C(0x526450)+(uint32_t)((int32_t)slot-1)*148u;
    uint32_t offset=0,length=0;
    int selected=0;
    if(slot<=party){
        unsigned kind=bitmap_guest_byte(h,record+127);
        if(!kind){
            offset=heap_long(h,record+114);length=heap_long(h,record+118);selected=1;
        }else if(kind==1||kind==2||kind==3){
            uint32_t address;
            if(kind==3)address=UINT32_C(0x52794a);
            else if(bitmap_guest_byte(h,record+126)<=party)address=UINT32_C(0x527952);
            else address=kind==1?UINT32_C(0x52794a):UINT32_C(0x52796a);
            offset=heap_long(h,address+4);length=heap_long(h,address);selected=1;
        }
    }else{
        unsigned target=bitmap_guest_byte(h,record+126);
        if(target<=party){
            uint32_t a=UINT32_C(0x526450)+(uint32_t)((int32_t)target-1)*148u;
            if(bitmap_guest_byte(h,a+127)==5){
                offset=heap_long(h,UINT32_C(0x527976));
                length=heap_long(h,UINT32_C(0x527972));selected=1;
            }
        }
    }
    if(!selected)return 0;
    enemy_guest_byte_write(h,UINT32_C(0x527a98),1);
    if(h->read_word(h->context,UINT32_C(0x527a50))!=1)return 0;
    uint32_t low=length&UINT32_C(0xffff);
    *request=(NpeBattleSoundRequest){UINT32_C(0x526eba),offset,
        low<0x8000?(int32_t)low:(int32_t)low-65536};
    return 1;
}
