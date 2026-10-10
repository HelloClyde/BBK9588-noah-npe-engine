/* Original0D:7C34,7ECE,8232. Keep retained LCD pixels and read the source
   through its current guest address; there is no NPE/header-length fallback. */
static uint8_t guest_bitmap_byte(const NpeResidentHeap *memory,uint32_t address){
    address&=UINT32_C(0xffffff);
    uint16_t word=memory->read_word(memory->context,address&~UINT32_C(1));
    return (uint8_t)((address&1u)?word:word>>8);
}
static uint8_t guest_lcd_pair(const uint8_t *pixels,unsigned position){
    return (uint8_t)(pixels[position]*16u+pixels[position+1]);
}
static void guest_lcd_pair_write(uint8_t *pixels,unsigned position,uint8_t value){
    pixels[position]=(uint8_t)(value>>4);pixels[position+1]=(uint8_t)(value&15u);
}
/* Preserve the original odd-column byte operations, including mistakes in
   the PRG: kind1 jumps into kind2 after its first row (81FE),816E uses the
   retained low byte of R1,84D4 reads a banked RAM mask,84F8 stores RB7. */
static void guest_bitmap_odd_row(const NpeResidentHeap *memory,uint32_t source,
                                unsigned left,unsigned top,unsigned width,
                                uint8_t kind,uint16_t *r1,uint8_t *pixels){
    unsigned position=top*NPE_WIDTH+left-1;
    uint8_t destination=guest_lcd_pair(pixels,position),value;
    if(kind==1){
        if(!(destination&15u)){
            *r1=(uint16_t)(((uint16_t)guest_bitmap_byte(memory,source)<<8)|(*r1&255u));
            *r1>>=4;
            guest_lcd_pair_write(pixels,position,(uint8_t)((destination&240u)|(*r1>>8)));
        }
    }else{
        *r1=(uint16_t)(((uint16_t)guest_bitmap_byte(memory,source)<<8)|(*r1&255u));
        *r1>>=4;value=(uint8_t)(*r1>>8);
        if(value)guest_lcd_pair_write(pixels,position,(uint8_t)((destination&240u)|value));
    }
    position+=2;
    for(unsigned pair=0;pair<(width-1)/2;++pair){
        destination=guest_lcd_pair(pixels,position);
        if(kind==1){
            if(!(destination&240u)){
                value=(uint8_t)(guest_bitmap_byte(memory,source++)<<4);
                if(!(destination&15u)){
                    *r1=(uint16_t)(((uint16_t)guest_bitmap_byte(memory,source)<<8)|(*r1&255u));
                    *r1>>=4;
                    value|=(uint8_t)(*r1>>8);
                }else value|=(uint8_t)*r1; /* Original8180; no source read. */
                guest_lcd_pair_write(pixels,position,value);
            }else{
                ++source;
                if(!(destination&15u)){
                    *r1=(uint16_t)(((uint16_t)guest_bitmap_byte(memory,source)<<8)|(*r1&255u));
                    *r1>>=4;
                    guest_lcd_pair_write(pixels,position,(uint8_t)((destination&240u)|(*r1>>8)));
                }
            }
        }else{
            uint8_t high=(uint8_t)(guest_bitmap_byte(memory,source++)<<4);
            *r1=(uint16_t)(((uint16_t)guest_bitmap_byte(memory,source)<<8)|(*r1&255u));
            *r1>>=4;uint8_t low=(uint8_t)(*r1>>8);
            if(high){
                value=(uint8_t)(high|(low?low:destination&15u));
                guest_lcd_pair_write(pixels,position,value);
            }else if(low){
                uint32_t mask_address=(source&UINT32_C(0xff8000))+UINT32_C(0xf0);
                value=(uint8_t)((destination&guest_bitmap_byte(memory,mask_address))|low);
                guest_lcd_pair_write(pixels,position,value);
            }
        }
        position+=2;
    }
    if(!(width&1u)){
        destination=guest_lcd_pair(pixels,position);
        if(kind==1){
            if(!(destination&240u)){
                *r1=(uint16_t)((*r1&UINT16_C(0xff00))|guest_bitmap_byte(memory,source));
                *r1=(uint16_t)(*r1<<4);
                guest_lcd_pair_write(pixels,position,(uint8_t)((destination&15u)|(uint8_t)*r1));
            }
        }else{
            value=(uint8_t)(guest_bitmap_byte(memory,source)<<4);
            if(value)guest_lcd_pair_write(pixels,position,value);
        }
    }
}
int npe_draw_guest_bitmap(const NpeResidentHeap *memory,uint32_t address,
                          int16_t x,int16_t y,uint8_t kind,
                          uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!memory||!memory->read_word||!pixels)return -1;
    uint8_t header[10];
    for(unsigned i=0;i<10;++i)header[i]=guest_bitmap_byte(memory,address+i);
    unsigned width=header[6],height=header[8];
    unsigned left=(uint16_t)x,top=(uint16_t)y;
    if(left>=NPE_WIDTH||top>=NPE_HEIGHT)return 0;
    unsigned visible_width=width,visible_height=height;
    if(visible_width>NPE_WIDTH-left)visible_width=NPE_WIDTH-left;
    if(visible_height>NPE_HEIGHT-top)visible_height=NPE_HEIGHT-top;
    unsigned stride=((width+3u)/4u)*2u;
    uint16_t r1=(uint16_t)height;
    for(unsigned row=0;row<visible_height;++row){
        uint32_t source=address+10u+row*stride;
        if((left&1u)&&kind&&visible_width){
            guest_bitmap_odd_row(memory,source,left,top+row,visible_width,
                                 (uint8_t)(kind==1&&row==0?1:2),&r1,pixels);
            continue;
        }
        for(unsigned column=0;column<visible_width;++column){
            uint8_t byte=guest_bitmap_byte(memory,source+column/2u);
            uint8_t value=(uint8_t)((column&1u)?byte&15u:byte>>4);
            unsigned destination=(top+row)*NPE_WIDTH+left+column;
            if(!kind||(kind==1?!pixels[destination]:value!=0))pixels[destination]=value;
        }
    }
    return 0;
}
