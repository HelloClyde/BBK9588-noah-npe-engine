static void enemy_guest_byte_write(const NpeResidentHeap *h,uint32_t address,uint8_t value){
    uint32_t a=address&~UINT32_C(1);uint16_t w=h->read_word(h->context,a);
    h->write_word(h->context,a,(uint16_t)((address&1u)?(w&0xff00u)|value:(w&255u)|((unsigned)value<<8)));
}
/* Original31:C76E..D76C /32:8A72..995E (CODE14:C76E /15:8A72): stream fields into resident
   records. The signed directory displacement wraps before extending. */
int npe_enemy_record_service(const NpeBattleBitmapService *s){
    if(!s||!s->heap.read_word||!s->heap.write_word||!s->seek||
       !s->read_local||!s->read_guest)return -1;
    const NpeResidentHeap *h=&s->heap;
    int scripted=bitmap_guest_byte(h,UINT32_C(0x527a9a))==1;
    uint8_t offset[4]={0},level=0,skill=0;
    unsigned party=bitmap_guest_byte(h,UINT32_C(0x527afa));
    for(unsigned slot=party;slot<(unsigned)bitmap_guest_byte(h,UINT32_C(0x527afa))+
                                      bitmap_guest_byte(h,UINT32_C(0x527af0));++slot){
        uint32_t a=UINT32_C(0x526450)+148u*slot;
        int32_t index=(int32_t)slot-bitmap_guest_byte(h,UINT32_C(0x527afa));
        uint16_t id=h->read_word(h->context,UINT32_C(0x52792e)+(uint32_t)(2*index));
        if(scripted&&bitmap_signed_word(id)<=0){
            enemy_guest_byte_write(h,a+122,0);enemy_guest_byte_write(h,a+123,0);
            h->write_word(h->context,a+100,0);h->write_word(h->context,a+76,0);
            continue;
        }
        if(!scripted&&!id)continue;
        uint32_t base=heap_long(h,UINT32_C(0x5279b6));
        uint32_t directory=base+(uint32_t)bitmap_signed_word((uint16_t)(id*4u-4u));
        for(unsigned i=0;i<4;++i)offset[i]=(uint8_t)(directory>>((3-i)*8));
        s->seek(s->context,directory);s->read_local(s->context,offset,4);
        uint32_t relative=(uint32_t)offset[0]<<24|(uint32_t)offset[1]<<16|(uint32_t)offset[2]<<8|offset[3];
        s->seek(s->context,base+relative);
        s->read_guest(s->context,a,12);s->read_guest(s->context,a+46,2);
        s->read_local(s->context,&level,1);enemy_guest_byte_write(h,a+24,level);
        const unsigned ordinary_fields[]={78,82,92,94,86,88,90};
        const unsigned scripted_fields[]={78,82,96,98,86,88,90};
        const unsigned ordinary_copies[]={80,84,96,98},scripted_copies[]={80,84,92,94};
        const unsigned *fields=scripted?scripted_fields:ordinary_fields;
        const unsigned *copies=scripted?scripted_copies:ordinary_copies;
        for(unsigned i=0;i<7;++i){s->read_guest(s->context,a+fields[i],2);
            if(i<4)h->write_word(h->context,a+copies[i],h->read_word(h->context,a+fields[i]));}
        /*The three skill bytes reuse one local byte for each read. */
        for(unsigned i=0;i<3;++i){s->read_local(s->context,&skill,1);enemy_guest_byte_write(h,a+25+i,skill);}
        s->read_guest(s->context,a+122,1);s->read_guest(s->context,a+123,1);
        s->read_guest(s->context,a+100,2);s->read_guest(s->context,a+76,2);
        s->read_guest(s->context,a+44,1);
        for(unsigned i=0;i<6;++i)h->write_word(h->context,a+62+2*i,0);
        if(!scripted)enemy_guest_byte_write(h,a+44,0);
    }
    party=bitmap_guest_byte(h,UINT32_C(0x527afa));
    unsigned num=bitmap_guest_byte(h,UINT32_C(0x527af0));
    unsigned placements=num==3?3:num==2?2:1;
    static const uint8_t x[]={82,42,2},y[]={16,38,54};
    for(unsigned i=0;i<placements;++i){uint32_t a=UINT32_C(0x526450)+148u*(party+i);
        enemy_guest_byte_write(h,a+43,0);enemy_guest_byte_write(h,a+124,placements==1?42:x[i]);
        enemy_guest_byte_write(h,a+125,placements==1?38:y[i]);}
    if(scripted)
        h->write_word(h->context,UINT32_C(0x526490)+148u*(party+1),10);
    return 0;
}
