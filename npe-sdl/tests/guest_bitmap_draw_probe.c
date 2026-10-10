#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "guest_bitmap_draw_native.h"

static uint8_t memory[131072],original_memory[131072];
static uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],expected[NPE_WIDTH*NPE_HEIGHT];
static uint8_t packed[NPE_WIDTH*NPE_HEIGHT/2],expected_packed[NPE_WIDTH*NPE_HEIGHT/2];
static uint16_t read_word(void *context,uint32_t address){
    (void)context;assert(address>=0x510000&&address+1<0x530000&&!(address&1));
    unsigned p=address-0x510000;return (uint16_t)((unsigned)memory[p]<<8|memory[p+1]);
}
static void load(const char *path,uint8_t *bytes,size_t size){
    FILE *file=fopen(path,"rb");assert(file);
    assert(fread(bytes,1,size,file)==size&&fgetc(file)==EOF);fclose(file);
}
static unsigned nibble(char c){
    assert((c>='0'&&c<='9')||(c>='a'&&c<='f'));
    return (unsigned)(c<='9'?c-'0':c-'a'+10);
}
int main(void){
    const NpeResidentHeap heap={NULL,read_word,NULL,NULL};
    for(unsigned c=0;c<sizeof(guest_bitmap_draw_cases)/sizeof(*guest_bitmap_draw_cases);++c){
        const GuestBitmapDrawCase *test=&guest_bitmap_draw_cases[c];
        memset(memory,0,sizeof(memory));
        memory[0xf0]=memory[0x80f0]=test->mask;
        unsigned start=test->address-0x510000,length=(unsigned)strlen(test->source_hex)/2;
        assert(start&&start+length<sizeof(memory));
        if(start&1)memory[start-1]=0x9a;
        for(unsigned i=0;i<length;++i)memory[start+i]=(uint8_t)(nibble(test->source_hex[2*i])*16+nibble(test->source_hex[2*i+1]));
        if((start+length)&1)memory[start+length]=0xbc;
        memcpy(original_memory,memory,sizeof(memory));
        for(unsigned i=0;i<sizeof(pixels);++i)pixels[i]=(uint8_t)((test->background>>(12-4*(i%4)))&15u);
        assert(!npe_draw_guest_bitmap(&heap,test->address,(int16_t)test->x,(int16_t)test->y,test->kind,pixels));
        assert(!memcmp(memory,original_memory,sizeof(memory)));
        load(test->pixels,expected,sizeof(expected));
        for(unsigned i=0;i<sizeof(pixels);++i)if(pixels[i]!=expected[i]){
            fprintf(stderr,"%s at(%u,%u): C%u original%u\n",test->name,i%NPE_WIDTH,i/NPE_WIDTH,pixels[i],expected[i]);return 1;
        }
        for(unsigned y=0;y<NPE_HEIGHT;++y)for(unsigned x=0;x<NPE_WIDTH;x+=2)
            packed[(NPE_HEIGHT-1-y)*(NPE_WIDTH/2)+x/2]=(uint8_t)(pixels[y*NPE_WIDTH+x]*16+pixels[y*NPE_WIDTH+x+1]);
        load(test->packed,expected_packed,sizeof(expected_packed));assert(!memcmp(packed,expected_packed,sizeof(packed)));
        fprintf(stderr,"Original guest bitmap draw %s: complete28800 pixels/14400 packed bytes match\n",test->name);
    }
    assert(npe_draw_guest_bitmap(NULL,0,0,0,0,pixels)==-1);
    assert(npe_draw_guest_bitmap(&heap,0,0,0,0,NULL)==-1);
    fprintf(stderr,"Guest bitmap draw:35 original complete display buffer cases verified\n");return 0;
}
