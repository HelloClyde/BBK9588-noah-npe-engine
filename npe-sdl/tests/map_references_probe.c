/* File-backed map destinations through the production NPE parser.
   This is a conservative structural graph: all exit branches and teleport
   records are included, even when their game conditions cannot be met.
   Retained/modified RAM tables and natural gameplay reachability are outside
   this audit. No source bytes or game state are changed. */
#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned from,to,slot,branch; const char *kind; } Edge;

int main(int argc,char **argv) {
    assert(argc==2);
    FILE *input=fopen(argv[1],"rb");assert(input&&!fseek(input,0,SEEK_END));
    long length=ftell(input);assert(length>0&&!fseek(input,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)length);assert(bytes);
    assert(fread(bytes,1,(size_t)length,input)==(size_t)length&&!fclose(input));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)length,error,sizeof(error)));
    unsigned total=file.offset_table_records[8],count=0,errors=0;
    assert(total&&total<=65535);
    Edge *edges=calloc((size_t)total*26,sizeof(*edges));
    unsigned *parent=calloc(total+1,sizeof(*parent));
    unsigned *queue=calloc(total,sizeof(*queue));assert(edges&&parent&&queue);
    printf("{\"maps\":%u,\"edges\":[",total);
    for(unsigned number=1;number<=total;++number) {
        NpeMap map;if(npe_map(&file,number-1,&map)){++errors;continue;}
        for(unsigned slot=0;slot<12;++slot) {
            NpeMapExit event;if(npe_map_exit(&map,slot,&event)){++errors;break;}
            for(unsigned branch=0;branch<2;++branch) {
                unsigned to=branch?event.second_map:event.destination_map;
                if(!to||to>total)continue;
                edges[count++]=(Edge){number,to,slot+1,branch,"exit"};
            }
        }
        for(unsigned slot=1;slot<=2;++slot) {
            NpeMapTeleportEvent event;int result=npe_map_teleport_event(&map,slot,&event);
            if(result<0){++errors;break;}if(!result)break;
            if(event.destination_map&&event.destination_map<=total)
                edges[count++]=(Edge){number,event.destination_map,slot,0,"teleport"};
        }
    }
    for(unsigned i=0;i<count;++i) {
        Edge e=edges[i];printf("%s{\"from\":%u,\"to\":%u,\"kind\":\"%s\",\"slot\":%u,\"branch\":%u}",
            i?",":"",e.from,e.to,e.kind,e.slot,e.branch);
    }
    unsigned used=1,next=0;queue[0]=1;parent[1]=1;
    while(next<used) {
        unsigned from=queue[next++];
        for(unsigned i=0;i<count;++i)if(edges[i].from==from&&!parent[edges[i].to]) {
            unsigned to=edges[i].to;parent[to]=from;queue[used++]=to;
        }
    }
    printf("],\"structural_reachable\":[");
    for(unsigned i=0;i<used;++i)printf("%s%u",i?",":"",queue[i]);
    printf("],\"parents\":[");
    for(unsigned i=1;i<=total;++i)printf("%s%u",i>1?",":"",parent[i]);
    printf("],\"parse_errors\":%u,\"natural_reachability_proven\":false}\n",errors);
    free(queue);free(parent);free(edges);npe_free(&file);return errors?1:0;
}
