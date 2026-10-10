#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "status_items_fixture.h"
int main(int argc,char **argv){
    assert(argc==2);FILE *input=fopen(argv[1],"rb");assert(input);
    assert(!fseek(input,0,SEEK_END));long size=ftell(input);assert(size>0&&!fseek(input,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,input)==(size_t)size&&!fclose(input));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    for(unsigned scenario=0;scenario<sizeof(status_item_cases)/sizeof(*status_item_cases);++scenario){
        const StatusItemCase *c=&status_item_cases[scenario];NpeWorld w=status_item_fixture(&file,scenario);
        uint8_t saved[42];uint8_t *raw=status_item_patch(&file,c,saved);status_item_check(&w,scenario,0);
        for(unsigned round=0;round<c->rounds;++round){
            for(unsigned actor=0;actor<c->party;++actor){
                if((int16_t)w.hp[actor]<=0||w.battle_party_status[actor][1]<0||w.battle_party_status[actor][2]<0)continue;assert(w.battle_actor==actor);
                status_item_numeric_action(&file,&w,c,actor==0&&(round==0||c->refresh));
            }
            status_item_check(&w,scenario,round+1);
        }
        memcpy(raw,saved,42);fprintf(stderr,"Original status item %s passed\n",c->name);
    }
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    printf("Status items: %u original record phases passed\n",status_items_compared);npe_free(&file);return 0;
}
