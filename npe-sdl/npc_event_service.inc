/* Original35:4360..4E60 and40:6798..6C3A. File and memory are live
   services; directory indices are not validated by host record counts. */
static uint16_t npc_local_word(const uint8_t *p){return (uint16_t)((unsigned)p[0]<<8|p[1]);}
static uint32_t npc_local_long(const uint8_t *p){return (uint32_t)npc_local_word(p)<<16|npc_local_word(p+2);}
int npe_npc_condition_service(const NpeResidentHeap *h,uint8_t kind,uint16_t value){
    uint32_t a;
    switch(kind){
    case 1:return h->read_word(h->context,UINT32_C(0x527a66))>=value;
    case 2:
        a=UINT32_C(0x5267c8)+(uint32_t)value*2u;
        if(h->read_word(h->context,a))return 1;
        for(unsigned member=0;member<3;++member)for(unsigned slot=0;slot<6;++slot)
            if(bitmap_guest_byte(h,UINT32_C(0x5269d8)+148u*member+slot)==value)return 1;
        return 0;
    case 4:return bitmap_guest_byte(h,UINT32_C(0x5269de))>=value;
    case 5:return bitmap_guest_byte(h,UINT32_C(0x527186)+value)!=0;
    case 6:
        if(!bitmap_guest_byte(h,UINT32_C(0x527a94))){
            h->write_word(h->context,UINT32_C(0x527a52),value);
            text_guest_byte_write(h,UINT32_C(0x527a94),1);return 1;
        }
        a=UINT32_C(0x526fba)+h->read_word(h->context,UINT32_C(0x527a52));
        {int result=bitmap_guest_byte(h,a)>=value;text_guest_byte_write(h,UINT32_C(0x527a94),0);return result;}
    case 7:
        if(!bitmap_guest_byte(h,UINT32_C(0x527a92))){
            h->write_word(h->context,UINT32_C(0x527ab8),value);
            text_guest_byte_write(h,UINT32_C(0x527a92),1);return 1;
        }
        a=UINT32_C(0x5267c8)+(uint32_t)h->read_word(h->context,UINT32_C(0x527ab8))*2u;
        {int result=h->read_word(h->context,a)>=value;text_guest_byte_write(h,UINT32_C(0x527a92),0);return result;}
    case 8:{uint32_t money=heap_long(h,UINT32_C(0x52799e));return money<UINT32_C(0x80000000)&&money>=value;}
    case 9:return bitmap_guest_byte(h,UINT32_C(0x52742c)+(uint16_t)(value-1u))!=0;
    default:return 1; /*0,3 and all other byte kinds branch to6C2C. */
    }
}
int npe_npc_event_service(const NpeNpcEventService *s,uint16_t id,uint8_t local[44],NpeNpcEvent *event){
    if(!s||!local||!event||!s->heap.read_word||!s->heap.write_word||!s->seek||!s->read_local||!s->read_guest)return -1;
    const NpeResidentHeap *h=&s->heap;
    uint32_t directory=heap_long(h,UINT32_C(0x5279f2));
    uint16_t delta=(uint16_t)((uint16_t)(id*5u)-5u);
    s->seek(s->context,directory+(uint32_t)bitmap_signed_word(delta));
    uint32_t position=directory+(uint32_t)bitmap_signed_word(delta);
    local[1]=(uint8_t)(position>>24);local[2]=(uint8_t)(position>>16);local[3]=(uint8_t)(position>>8);local[4]=(uint8_t)position;
    s->read_local(s->context,local,1);s->read_local(s->context,local+1,4);
    uint32_t state_address=UINT32_C(0x526fba)+(uint32_t)bitmap_signed_word(id);
    uint8_t index=bitmap_guest_byte(h,state_address);
    if((int)index>(int)local[0]-1){index=(uint8_t)(local[0]-1u);text_guest_byte_write(h,state_address,index);}
    uint32_t offset=npc_local_long(local+1);
    s->seek(s->context,directory+offset+(uint32_t)bitmap_signed_word((uint16_t)(38u*index)));
    uint8_t *record=local+5;
    s->read_local(s->context,record,1);s->read_local(s->context,record+1,1);
    for(unsigned i=0;i<6;++i){s->read_local(s->context,record+2+3*i,1);s->read_local(s->context,record+3+3*i,2);}
    s->read_guest(s->context,UINT32_C(0x527a0a),1);s->read_guest(s->context,UINT32_C(0x527a82),2);
    s->read_guest(s->context,UINT32_C(0x527a08),1);s->read_guest(s->context,UINT32_C(0x527a80),2);
    for(unsigned i=0;i<3;++i)s->read_local(s->context,record+26+4*i,4);
    uint8_t succeeded=local[43];
    if(record[0]==0)succeeded=1;
    else if(record[0]<=6){
        succeeded=1;
        for(unsigned i=0;i<record[0];++i){
            succeeded=(uint8_t)npe_npc_condition_service(h,record[2+3*i],npc_local_word(record+3+3*i));
            if(!succeeded)break;
        }
        if(succeeded && (bitmap_guest_byte(h,UINT32_C(0x527a94))==1 || bitmap_guest_byte(h,UINT32_C(0x527a92))==1)){
            succeeded=0;text_guest_byte_write(h,UINT32_C(0x527a94),0);text_guest_byte_write(h,UINT32_C(0x527a92),0);
        }
    }
    local[43]=succeeded;
    unsigned choice=0;
    if(succeeded==1)text_guest_byte_write(h,state_address,(uint8_t)(bitmap_guest_byte(h,state_address)+1u));
    else{
        text_guest_byte_write(h,UINT32_C(0x527a0a),bitmap_guest_byte(h,UINT32_C(0x527a08)));
        h->write_word(h->context,UINT32_C(0x527a82),h->read_word(h->context,UINT32_C(0x527a80)));
        choice=record[1]==1?2:1;
    }
    uint32_t pointer=npc_local_long(record+26+4*choice);
    heap_set_long(h,UINT32_C(0x52797a),pointer);
    heap_set_long(h,UINT32_C(0x52797a),heap_long(h,UINT32_C(0x52797a))+heap_long(h,UINT32_C(0x5279f2)));
    *event=(NpeNpcEvent){index,local[0],record[0],succeeded,pointer,
        bitmap_guest_byte(h,UINT32_C(0x527a0a)),h->read_word(h->context,UINT32_C(0x527a82))};
    return 0;
}
