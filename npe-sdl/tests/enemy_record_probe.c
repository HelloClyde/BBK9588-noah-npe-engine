#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct{
    const char *name,*initial,*expected;
    unsigned party,runtime,reads,seeks;
    uint32_t seek[6];unsigned request[57],supply[57];uint32_t destination[57];uint8_t chunks[57][12];
}EnemyRecordCase;
#include "enemy_record_native.h"
typedef struct{uint8_t ram[131072];const EnemyRecordCase *test;unsigned reads,seeks;}Context;
static uint16_t word(void *context,uint32_t a){
    Context *c=context;assert(a>=0x510000&&a<0x52ffff);a-=0x510000;
    return (uint16_t)((unsigned)c->ram[a]*256+c->ram[a+1]);
}
static void write_word(void *context,uint32_t a,uint16_t v){
    Context *c=context;assert(a>=0x510000&&a<0x52ffff);a-=0x510000;
    c->ram[a]=(uint8_t)(v>>8);c->ram[a+1]=(uint8_t)v;
}
static void seek(void *context,uint32_t pos){
    Context *c=context;assert(c->seeks<c->test->seeks&&pos==c->test->seek[c->seeks]);++c->seeks;
}
static unsigned read_index(Context *c,uint16_t n){
    unsigned i=c->reads++;assert(i<c->test->reads&&n==c->test->request[i]);return i;
}
static void read_local(void *context,uint8_t *dst,uint16_t n){
    Context *c=context;unsigned i=read_index(c,n);assert(c->test->destination[i]<0x10000);
    memcpy(dst,c->test->chunks[i],c->test->supply[i]);
}
static void read_guest(void *context,uint32_t dst,uint16_t n){
    Context *c=context;unsigned i=read_index(c,n);
    if(dst!=c->test->destination[i])fprintf(stderr,"%s read%u destination C=%06x original=%06x\n",c->test->name,i,dst,c->test->destination[i]);
    assert(dst==c->test->destination[i]);
    assert(dst>=0x510000&&dst+n<=0x530000);memcpy(c->ram+dst-0x510000,c->test->chunks[i],c->test->supply[i]);
}
static void load(const char *path,uint8_t *dst,size_t size){
    FILE *fp=fopen(path,"rb");assert(fp&&fread(dst,1,size,fp)==size&&fgetc(fp)==EOF&&!fclose(fp));
}
static void compare(const EnemyRecordCase *s,const uint8_t *actual,const uint8_t *expected,const char *route){
    unsigned differences=0;
    for(unsigned i=0;i<131072;++i)if(actual[i]!=expected[i]){
        if(differences++<8)fprintf(stderr,"%s %s %06x:C=%02x original=%02x\n",s->name,route,0x510000+i,actual[i],expected[i]);
    }
    assert(!differences);
}
int main(int argc,char **argv){
    assert(argc==2);FILE *fp=fopen(argv[1],"rb");assert(fp&&!fseek(fp,0,SEEK_END));
    long size=ftell(fp);assert(size>0&&!fseek(fp,0,SEEK_SET));uint8_t *bytes=malloc((size_t)size);
    assert(bytes&&fread(bytes,1,(size_t)size,fp)==(size_t)size&&!fclose(fp));
    NpeFile f;char error[128];assert(!npe_parse(&f,bytes,(size_t)size,error,sizeof(error)));
    Context *c=calloc(1,sizeof(*c));uint8_t *expected=malloc(131072),*actual=malloc(131072);assert(c&&expected&&actual);
    unsigned runtime_cases=0;
    for(unsigned i=0;i<sizeof(enemy_record_cases)/sizeof(enemy_record_cases[0]);++i){
        const EnemyRecordCase *s=&enemy_record_cases[i];c->test=s;c->reads=c->seeks=0;
        load(s->initial,c->ram,131072);load(s->expected,expected,131072);
        NpeBattleBitmapService service={{c,word,write_word,NULL},c,seek,read_local,read_guest,NULL};
        assert(!npe_enemy_record_service(&service));assert(c->reads==s->reads&&c->seeks==s->seeks);
        compare(s,c->ram,expected,"service");
        if(s->runtime){
            assert(!npe_rpg_runtime_open(&f,NULL));load(s->initial,actual,131072);
            assert(!npe_rpg_runtime_write(&f,0x510000,actual,131072));
            assert(!npe_rpg_runtime_load_enemies(&f,s->party));
            assert(!npe_rpg_runtime_read(&f,0x510000,actual,131072));compare(s,actual,expected,"runtime");
            assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);++runtime_cases;
        }
        fprintf(stderr,"%s:full128KiB RAM and %u reads/%u seeks match original\n",s->name,s->reads,s->seeks);
    }
    printf("%u original enemy loaders; %u real-file runtime paths; full128KiB RAM/IO passed\n",
           (unsigned)(sizeof(enemy_record_cases)/sizeof(enemy_record_cases[0])),runtime_cases);
    free(c);free(expected);free(actual);npe_free(&f);return 0;
}
