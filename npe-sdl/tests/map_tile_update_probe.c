#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {uint8_t ram[131072];unsigned writes;} Memory;
static uint8_t *load(const char *name,size_t *size){
    FILE *f=fopen(name,"rb");assert(f && !fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>=0 && !fseek(f,0,SEEK_SET));uint8_t *p=malloc((size_t)n+1);
    assert(p && fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*size=(size_t)n;return p;
}
static unsigned offset(uint32_t a){assert(a>=0x510000 && a<0x52ffff);return a-0x510000;}
static uint16_t read_word(void *context,uint32_t a){Memory *m=context;unsigned i=offset(a);return (uint16_t)((unsigned)m->ram[i]<<8|m->ram[i+1]);}
static void write_word(void *context,uint32_t a,uint16_t v){Memory *m=context;unsigned i=offset(a);m->ram[i]=(uint8_t)(v>>8);m->ram[i+1]=(uint8_t)v;++m->writes;}
static int resident_read(void *context,uint32_t a,uint8_t *p,unsigned n){
    return npe_world_resident_read(context,a,p,n);
}
static int resident_write(void *context,uint32_t a,const uint8_t *p,unsigned n){
    return npe_world_resident_write(context,a,p,n);
}
static int progress_read(void *context,uint32_t a,uint8_t *p,unsigned n){
    return a>=0x527a66 && a<0x527a68 && n<=0x527a68-a?resident_read(context,a,p,n):-1;
}
static int progress_write(void *context,uint32_t a,const uint8_t *p,unsigned n){
    return a>=0x527a66 && a<0x527a68 && n<=0x527a68-a?resident_write(context,a,p,n):-1;
}
static void compare_ram(const uint8_t *actual,const uint8_t *expected,const char *stage){
    unsigned differences=0;
    for(unsigned i=0;i<131072;++i)if(actual[i]!=expected[i]){
        if(differences<12)fprintf(stderr,"%s RAM mismatch %06x C=%02x original=%02x\n",stage,0x510000+i,actual[i],expected[i]);
        ++differences;
    }
    assert(!differences);
}
int main(int argc,char **argv){
    assert(argc>=4 && argc<=6);size_t n,b,file_size;uint8_t *before=load(argv[1],&n),*after=load(argv[2],&b);assert(n==131072 && n==b);
    Memory *m=calloc(1,sizeof(*m));assert(m);memcpy(m->ram,before,n);
    NpeResidentHeap heap={m,read_word,write_word,NULL};int updated=npe_map_tile_update_service(&heap,NULL,NULL);assert(updated>=0);
    for(unsigned i=0;i<n;++i)if(m->ram[i]!=after[i]){fprintf(stderr,"RAM mismatch %06x C=%02x original=%02x\n",0x510000+i,m->ram[i],after[i]);return 1;}
    uint8_t *bytes=load(argv[3],&file_size);NpeFile file;NpeWorld world;char error[128];
    assert(!npe_parse(&file,bytes,file_size,error,sizeof(error)));assert(!npe_world_init(&file,&world,1,7,10));
    unsigned bound=0;for(uint32_t a=0x510000;a<0x530000;++a){uint8_t v;if(!npe_world_resident_read(&world,a,&v,1))++bound;}assert(bound==NPE_WORLD_RESIDENT_BYTES);
    uint8_t values[2]={0x12,0x34};assert(!npe_world_resident_write(&world,0x527a66,values,2) && world.story_progress==0x1234);
    values[0]=0xab;assert(!npe_world_resident_write(&world,0x527a67,values,1) && world.story_progress==0x12ab);
    assert(npe_world_resident_write(&world,0x527a67,values,2)==-1 && world.story_progress==0x12ab);
    if(argc==6){
        unsigned party=(unsigned)strtoul(argv[5],NULL,10);assert(party>=1 && party<=3 && !strcmp(argv[4],"import-all"));
        for(unsigned i=0;i<3;++i)assert(!npe_world_map_record_image(&world,i,world.battle_map_records[i]));
        memcpy(world.battle_map_party_active,world.party_active,3);
        world.battle_record_party_count=(uint8_t)party;world.battle_party_flags_saved=1;
    }
    const NpeResidentBindings bindings={&world,argc>=5?resident_read:progress_read,argc>=5?resident_write:progress_write};
    assert(!npe_rpg_runtime_open_idle(&file,&bindings));assert(!npe_rpg_runtime_map_load(&file,&world,0));
    if(argc>=5 && strcmp(argv[4],"import-all")){
        /* Start from an actual constructed World. Keep its independent
           bindings; seed only flat RAM and the live progress word. The
           arbitrary full-import limitation is tested by import-all. */
        for(unsigned i=0;i<131072;++i){uint32_t a=UINT32_C(0x510000)+i;uint8_t v;
            if((a>=0x527a66 && a<0x527a68) || npe_world_resident_read(&world,a,&v,1))
                assert(!npe_rpg_runtime_write(&file,a,before+i,1));
        }
    }else assert(!npe_rpg_runtime_write(&file,0x510000,before,131072));
    assert(!npe_rpg_runtime_read(&file,0x510000,m->ram,131072));
    if(argc>=5 && strcmp(argv[4],"full") && strcmp(argv[4],"import-all")){
        FILE *seed=fopen(argv[4],"wb");assert(seed && fwrite(m->ram,1,n,seed)==n && !fclose(seed));
        printf("World-bound initial RAM exported; no update reference produced\n");
        npe_free(&file);free(before);free(after);free(m);return 0;
    }
    compare_ram(m->ram,before,"initial import");
    uint32_t cursor=npe_rpg_runtime_cursor(&file);unsigned reads=npe_rpg_runtime_reads(&file);
    assert(npe_world_update_tiles(&world)==updated);
    assert(!npe_rpg_runtime_read(&file,0x510000,m->ram,131072));compare_ram(m->ram,after,"World update");
    assert(world.story_progress==(uint16_t)((unsigned)after[0x17a66]<<8|after[0x17a67]));
    assert(cursor==npe_rpg_runtime_cursor(&file) && reads==npe_rpg_runtime_reads(&file));
    printf("full128KiB RAM matches original service/helper and World dispatch(%s); updates=%d;3748 bound bytes\n",argc>=5?"all bindings":"progress binding",updated);
    npe_free(&file);free(before);free(after);free(m);return 0;
}
