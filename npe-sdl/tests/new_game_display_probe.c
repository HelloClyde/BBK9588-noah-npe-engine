#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void frame(const char *path,uint8_t *pixels) {
    FILE *f=fopen(path,"rb");assert(f);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT&&fgetc(f)==EOF&&!fclose(f));
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long len=ftell(f);assert(len>0&&!fseek(f,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)len);assert(bytes&&fread(bytes,1,(size_t)len,f)==(size_t)len&&!fclose(f));
    NpeFile file;NpeWorld world;char error[128];
    assert(!npe_parse(&file,bytes,(size_t)len,error,sizeof(error))&&!npe_world_new_game(&file,&world));
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[sizeof(pixels)];
    assert(world.direction==1&&!npe_render_world(&file,&world,pixels));
    f=fopen("tmp/c-new-game-current.idx","wb");assert(f);
    assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels)&&!fclose(f));
    frame("tmp/native-empty-title-map-commit-0.idx",native);
    unsigned differences=0;for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=native[i];
    fprintf(stderr,"New game initial frame: %u differences\n",differences);assert(!differences);
    for(unsigned route=0;route<2;++route)for(unsigned direction=0;direction<=3;direction+=3)
        for(unsigned commit=0;commit<3;++commit){
            char path[160];snprintf(path,sizeof(path),"tmp/native-new-game-%s-prior-%u-commit-%u.idx",route?"empty":"new",direction,commit);
            frame(path,native);assert(!memcmp(pixels,native,sizeof(pixels)));
        }
    /* Diagnostic copies verify the loader follows its NPE direction field.
       Restore it before freeing; never modify the source file on disk. */
    NpeCharacter hero;assert(!npe_character(&file,0,&hero));
    size_t offset=(size_t)(hero.raw-file.bytes)+78;uint8_t original=file.bytes[offset];
    for(unsigned direction=0;direction<4;++direction){
        file.bytes[offset]=(uint8_t)direction;
        assert(!npe_world_new_game(&file,&world)&&world.direction==direction);
    }
    file.bytes[offset]=original;
    npe_free(&file);puts("Original empty-title initial map matched");return 0;
}
