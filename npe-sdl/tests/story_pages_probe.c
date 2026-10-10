#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv){
    assert(argc==2);
    FILE *stream=fopen(argv[1],"rb");assert(stream);
    assert(!fseek(stream,0,SEEK_END));long size=ftell(stream);assert(size>0);
    rewind(stream);uint8_t *bytes=malloc((size_t)size);assert(bytes);
    assert(fread(bytes,1,(size_t)size,stream)==(size_t)size&&!fclose(stream));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    const unsigned ids[]={4,9,23,37,63,164,181,198};unsigned total=0;
    for(unsigned c=0;c<sizeof(ids)/sizeof(*ids);++c){
        NpeStoryRecord story;assert(!npe_story_record(&file,ids[c]-1,&story));
        size_t start=0,next=0;unsigned page=0;
        do{
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[sizeof(pixels)];char path[128];
            assert(!npe_render_native_story_page(&file,&font,&story,start,&next,pixels)&&next>start);
            snprintf(path,sizeof(path),"tmp/native-story-%u-page-%u.idx",ids[c],page);
            stream=fopen(path,"rb");assert(stream);
            assert(fread(native,1,sizeof(native),stream)==sizeof(native)&&fgetc(stream)==EOF&&!fclose(stream));
            unsigned differences=0;for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=native[i];
            fprintf(stderr,"Original story %u page %u, text %zu..%zu: %u differences\n",ids[c],page,start,next,differences);
            if(differences){stream=fopen("tmp/story-page-c.idx","wb");assert(stream);assert(fwrite(pixels,1,sizeof(pixels),stream)==sizeof(pixels)&&!fclose(stream));}
            assert(!differences);start=next;++page;++total;
        }while(start<story.text_size);
        snprintf(error,sizeof(error),"tmp/native-story-%u-page-%u.idx",ids[c],page);
        stream=fopen(error,"rb");if(stream)fclose(stream);assert(!stream);
    }
    npe_font_free(&font);npe_free(&file);
    printf("8 original full-screen stories, %u pages passed\n",total);return 0;
}
