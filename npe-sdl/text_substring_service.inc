/* Original CPU28:031E (PRG45:031E..063A). All indices and sums are
   wrapping words, compared as signed16. Copy reads remain live on overlap. */
uint16_t npe_text_length_service(const NpeResidentHeap *h,uint32_t source,int cursor_kind){
    uint16_t length=0,index=0;
    while(bitmap_guest_byte(h,source+(cursor_kind==2?(uint32_t)index:(uint32_t)bitmap_signed_word(index)))){
        length=(uint16_t)(length+1u);
        index=cursor_kind==1?(uint8_t)(index+1u):(uint16_t)(index+1u);
    }
    return length;
}
uint16_t npe_text_substring_service(const NpeResidentHeap *h,uint32_t destination,
                                    uint32_t source,uint16_t start,uint16_t count){
    uint16_t length=npe_text_length_service(h,source,0),first,last,negative=0;
    uint16_t sum=(uint16_t)(start+count);
    if(bitmap_signed_word(count)>=0&&bitmap_signed_word(length)>bitmap_signed_word(start)){
        first=start;
        if(bitmap_signed_word(sum)>=bitmap_signed_word(length))last=(uint16_t)(length-1u);
        else{
            for(uint16_t i=start;bitmap_signed_word(i)<bitmap_signed_word(sum);i=(uint16_t)(i+1u))
                if(bitmap_guest_byte(h,source+(uint32_t)bitmap_signed_word(i))>=0x80u)++negative;
            last=(uint16_t)(sum-1u-(negative&1u));
        }
    }else if(bitmap_signed_word(count)<0&&bitmap_signed_word(length)>=bitmap_signed_word(start)){
        last=(uint16_t)(start-1u);
        if(bitmap_signed_word(sum)<=0)first=0;
        else{
            for(uint16_t i=sum;bitmap_signed_word(i)<bitmap_signed_word(start);i=(uint16_t)(i+1u))
                if(bitmap_guest_byte(h,source+(uint32_t)bitmap_signed_word(i))>=0x80u)++negative;
            first=(uint16_t)(sum+(negative&1u));
        }
    }else{ text_guest_byte_write(h,destination,0);return 0; }
    uint16_t i=first;
    for(;bitmap_signed_word(i)<=bitmap_signed_word(last);i=(uint16_t)(i+1u)){
        uint8_t value=bitmap_guest_byte(h,source+(uint32_t)bitmap_signed_word(i));
        text_guest_byte_write(h,destination+(uint32_t)bitmap_signed_word((uint16_t)(i-first)),value);
    }
    text_guest_byte_write(h,destination+(uint32_t)bitmap_signed_word((uint16_t)(i-first)),0);
    return (uint16_t)(last-first+1u);
}
