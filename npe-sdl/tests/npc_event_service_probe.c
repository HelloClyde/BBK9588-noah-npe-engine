#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {uint8_t ram[262144];const uint8_t *file;size_t size;uint32_t cursor,seeks[2],dest[23];unsigned reads,seek_count,request[23],supply[23];} Memory;
static unsigned offset(uint32_t a){assert(a>=0x510000&&a<0x550000);return a-0x510000;}
static uint16_t word(void *p,uint32_t a){Memory *m=p;unsigned o=offset(a);assert(o+1<262144);return (uint16_t)((unsigned)m->ram[o]<<8|m->ram[o+1]);}
static void write_word(void *p,uint32_t a,uint16_t v){Memory *m=p;unsigned o=offset(a);assert(o+1<262144);m->ram[o]=(uint8_t)(v>>8);m->ram[o+1]=(uint8_t)v;}
static uint8_t *read_file(const char *path,size_t *size){FILE *in=fopen(path,"rb");assert(in&&!fseek(in,0,SEEK_END));long n=ftell(in);assert(n>0&&!fseek(in,0,SEEK_SET));uint8_t *b=malloc((size_t)n);assert(b&&fread(b,1,(size_t)n,in)==(size_t)n&&!fclose(in));*size=(size_t)n;return b;}
static void seek(void *p,uint32_t a){Memory *m=p;assert(m->seek_count<2);m->seeks[m->seek_count++]=a;if(a<=m->size)m->cursor=a;}
static void read_local(void *p,uint8_t *out,uint16_t n){Memory *m=p;assert(m->reads<23);unsigned supplied=m->size-m->cursor<n?(unsigned)(m->size-m->cursor):n;unsigned i=m->reads++;m->request[i]=n;m->supply[i]=supplied;memcpy(out,m->file+m->cursor,supplied);m->cursor+=supplied;}
static void read_guest(void *p,uint32_t a,uint16_t n){Memory *m=p;unsigned i=m->reads;read_local(m,m->ram+offset(a),n);m->dest[i]=a;}
static void compare(const uint8_t *actual,const uint8_t *expected,const char *stage){unsigned bad=0;for(unsigned i=0;i<262144;++i)if(actual[i]!=expected[i]){if(bad<12)fprintf(stderr,"%s %06x original%02x C%02x\n",stage,0x510000+i,expected[i],actual[i]);++bad;}assert(!bad);}
static int bound_read(void *p,uint32_t a,uint8_t *b,unsigned n){return npe_world_resident_read(p,a,b,n);}
static int bound_write(void *p,uint32_t a,const uint8_t *b,unsigned n){return npe_world_resident_write(p,a,b,n);}
static unsigned be16(const uint8_t *b){return (unsigned)b[0]<<8|b[1];}
static uint32_t be32(const uint8_t *b){return (uint32_t)be16(b)<<16|be16(b+2);}
int main(int argc,char **argv){
    assert(argc==2);char path[220];size_t size,npe_size,trace_size;
    snprintf(path,sizeof(path),"%s/before.bin",argv[1]);uint8_t *before=read_file(path,&size);assert(size==262144);
    snprintf(path,sizeof(path),"%s/after.bin",argv[1]);uint8_t *after=read_file(path,&size);assert(size==262144);
    snprintf(path,sizeof(path),"%s/trace.bin",argv[1]);uint8_t *trace=read_file(path,&trace_size);assert(trace_size==0x420);
    snprintf(path,sizeof(path),"%s.npe",argv[1]);uint8_t *data=read_file(path,&npe_size);
    Memory *m=calloc(1,sizeof(*m));assert(m);memcpy(m->ram,before,262144);m->file=data;m->size=npe_size;
    NpeResidentHeap heap={m,word,write_word,NULL};NpeNpcEventService service={heap,m,seek,read_local,read_guest};uint8_t local[44]={0};NpeNpcEvent event;
    uint16_t id=(uint16_t)word(m,UINT32_C(0x527936));assert(!npe_npc_event_service(&service,id,local,&event));compare(m->ram,after,"flat");
    assert(m->reads==be16(trace)&&m->seek_count==be16(trace+2));
    for(unsigned i=0;i<m->seek_count;++i)assert(m->seeks[i]==be32(trace+0x400+4*i));
    for(unsigned i=0;i<m->reads;++i){const uint8_t *t=trace+0x100+8*i;assert(m->request[i]==be16(t)&&m->supply[i]==be16(t+2));if(m->dest[i])assert(m->dest[i]==be32(t+4));}
    uint32_t final_cursor=m->cursor;NpeFile file;NpeWorld world;char error[128];assert(!npe_parse(&file,data,npe_size,error,sizeof(error))&&!npe_world_init(&file,&world,1,7,10));
    NpeResidentBindings bindings={&world,bound_read,bound_write};assert(!npe_rpg_runtime_open_idle(&file,&bindings));
    assert(!npe_rpg_runtime_write(&file,UINT32_C(0x510000),before,262144)&&!npe_rpg_runtime_read(&file,UINT32_C(0x510000),m->ram,262144));compare(m->ram,before,"bound import");
    unsigned reads=npe_rpg_runtime_reads(&file);NpeNpcEvent runtime_event;assert(!npe_rpg_runtime_npc_event(&file,id,&runtime_event));
    assert(!npe_rpg_runtime_read(&file,UINT32_C(0x510000),m->ram,262144));compare(m->ram,after,"World bindings");
    assert(npe_rpg_runtime_reads(&file)-reads==23&&npe_rpg_runtime_cursor(&file)==final_cursor);
    assert(event.case_index==runtime_event.case_index&&event.case_count==runtime_event.case_count&&event.action_type==runtime_event.action_type&&event.succeeded==runtime_event.succeeded&&event.dialogue_offset==runtime_event.dialogue_offset&&event.followup_mode==runtime_event.followup_mode&&event.followup_event_id==runtime_event.followup_event_id);
    printf("NPC%u type%u success%d: two complete256KiB original RAM comparisons,23 reads/2 seeks,live bindings passed\n",id,event.action_type,event.succeeded);
    npe_free(&file);free(before);free(after);free(trace);free(m);return 0;
}
