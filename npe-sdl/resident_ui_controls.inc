/* Shared control bytes used by dialogue continuation and shop services.
   Word values are exposed in guest byte order, never native struct order. */
typedef struct {uint32_t address;unsigned size;size_t offset;} ResidentUiControl;
#define UI_WORD(a,f) {UINT32_C(a),2,offsetof(NpeWorld,f)}
#define UI_LONG(a,f) {UINT32_C(a),4,offsetof(NpeWorld,f)}
#define UI_BYTE(a,f) {UINT32_C(a),1,offsetof(NpeWorld,f)}
static const ResidentUiControl resident_ui_controls[]={
    UI_WORD(0x52791e,text_rect[0]),UI_WORD(0x527920,text_rect[1]),
    UI_WORD(0x527922,text_rect[2]),UI_WORD(0x527924,text_rect[3]),
    UI_LONG(0x52797a,text_file_position),UI_WORD(0x527a1a,text_offset),
    UI_BYTE(0x527a1c,text_more),UI_BYTE(0x527ace,text_style),
    UI_WORD(0x527934,game_mode),UI_WORD(0x527936,story_event_id),UI_WORD(0x527938,event_context_id),
    UI_BYTE(0x527a0a,mode2_followup_mode),UI_BYTE(0x527a0c,shop_choice),UI_BYTE(0x527a0e,shop_kind),
    UI_BYTE(0x527a10,shop_scan_first),UI_BYTE(0x527a12,shop_scan_last),UI_BYTE(0x527a14,shop_inventory_only),
    UI_BYTE(0x527a46,shop_row),UI_BYTE(0x527a48,shop_quantity),UI_BYTE(0x527a4a,shop_ui),
    UI_WORD(0x527a4c,shop_selected_item),UI_BYTE(0x527a68,mode2_context),
    UI_WORD(0x527a52,npc_guard_index),UI_WORD(0x527ab8,npc_guard_word_index),
    UI_BYTE(0x527a94,npc_guard_a),UI_BYTE(0x527a92,npc_guard_b),
    UI_WORD(0x527a82,mode2_followup_event_id),UI_WORD(0x527ab6,shop_last_item),
    UI_WORD(0x527aba,item_description_length),UI_WORD(0x527abc,shop_description_end),
    UI_WORD(0x527abe,shop_description_start),UI_BYTE(0x527ac2,shop_transaction_sell),
    UI_WORD(0x527ac6,shop_first_item)
};
#undef UI_LONG
#undef UI_WORD
#undef UI_BYTE
static const ResidentUiControl *resident_ui_control(uint32_t address){
    if(address<UINT32_C(0x52791e)||address>UINT32_C(0x527ace))return NULL;
    for(unsigned i=0;i<sizeof(resident_ui_controls)/sizeof(*resident_ui_controls);++i){
        const ResidentUiControl *c=resident_ui_controls+i;
        if(address>=c->address && address-c->address<c->size)return c;
    }return NULL;
}
static uint32_t resident_ui_control_end(uint32_t address){
    const ResidentUiControl *c=resident_ui_control(address);if(!c)return 0;
    uint32_t end=c->address+c->size;
    while((c=resident_ui_control(end))!=NULL)end=c->address+c->size;
    return end;
}
static int resident_ui_control_read(const NpeWorld *w,uint32_t address,uint8_t *bytes,unsigned count){
    uint32_t end=resident_ui_control_end(address);if(!end || count>end-address)return -1;
    for(unsigned i=0;i<count;++i){const ResidentUiControl *c=resident_ui_control(address+i);
        const uint8_t *p=(const uint8_t *)w+c->offset;
        if(c->size==1)bytes[i]=*p;
        else if(c->size==2){uint16_t word;memcpy(&word,p,2);bytes[i]=(uint8_t)((address+i==c->address)?word>>8:word);}
        else{uint32_t value;memcpy(&value,p,4);bytes[i]=(uint8_t)(value>>(8u*(3u-(address+i-c->address))));}
    }return 0;
}
static int resident_ui_control_write(NpeWorld *w,uint32_t address,const uint8_t *bytes,unsigned count){
    uint32_t end=resident_ui_control_end(address);if(!end || count>end-address)return -1;
    for(unsigned i=0;i<count;++i){const ResidentUiControl *c=resident_ui_control(address+i);
        uint8_t *p=(uint8_t *)w+c->offset;
        if(c->size==1)*p=bytes[i];
        else if(c->size==2){uint16_t word;memcpy(&word,p,2);
            word=(uint16_t)((address+i==c->address)?(word&255u)|((unsigned)bytes[i]<<8):(word&0xff00u)|bytes[i]);
            memcpy(p,&word,2);}
        else{uint32_t value;unsigned shift=8u*(3u-(address+i-c->address));memcpy(&value,p,4);
            value=(value&~(UINT32_C(255)<<shift))|((uint32_t)bytes[i]<<shift);memcpy(p,&value,4);}
    }return 0;
}
