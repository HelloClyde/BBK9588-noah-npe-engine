#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {unsigned add,character,mode,next;} Cache;
typedef struct {const char *name;unsigned map;Cache boot[3],before[3],after[3];} CacheCase;
#include "party_cache_native.h"
static void check_event(const NpeMapPartyEvent *e,const Cache *c){
    if(e->add_member!=c->add||e->character_id!=c->character||e->next_mode!=c->mode||e->next_id!=c->next)
        fprintf(stderr,"Cache mismatch actual=%u/%u/%u/%u original=%u/%u/%u/%u\n",e->add_member,e->character_id,e->next_mode,e->next_id,c->add,c->character,c->mode,c->next);
    assert(e->add_member==c->add&&e->character_id==c->character&&e->next_mode==c->mode&&e->next_id==c->next);
}
static void check(const NpeWorld *w,const Cache *c){for(unsigned i=0;i<3;++i)check_event(&w->party_events[i],&c[i]);}
static void seed(NpeWorld *w,const Cache *c){
    for(unsigned i=0;i<3;++i)w->party_events[i]=(NpeMapPartyEvent){.add_member=(uint8_t)c[i].add,.character_id=(uint8_t)c[i].character,.next_mode=(uint8_t)c[i].mode,.next_id=(uint16_t)c[i].next};
}
int main(int argc,char **argv){
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0&&!fseek(in,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,in)==(size_t)size&&!fclose(in));NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    NpeWorld baseline;assert(!npe_world_new_game(&file,&baseline));
    for(unsigned i=0;i<sizeof(cache_cases)/sizeof(*cache_cases);++i){const CacheCase *c=&cache_cases[i];NpeWorld saved,w,cold;char path[128];
        fprintf(stderr,"Cache case %s boot\n",c->name);
        check(&baseline,c->boot);assert(!npe_world_init(&file,&saved,c->map,1,1));
        snprintf(path,sizeof(path),"tmp/party-cache-%s.npes",c->name);assert(!npe_world_save(&file,&saved,path));
        if(!i){
            const char *second_path="tmp/party-cache-wire-comparison.npes";NpeWorld changed=saved;seed(&changed,c->before);
            assert(!npe_world_save(&file,&changed,second_path));FILE *left=fopen(path,"rb"),*right=fopen(second_path,"rb");assert(left&&right);
            int a,b;do{a=fgetc(left);b=fgetc(right);assert(a==b);}while(a!=EOF);
            assert(!fclose(left)&&!fclose(right)&&!remove(second_path));
        }
        w=baseline;seed(&w,c->before);assert(!npe_world_restore(&file,&w,path)&&w.map_number==c->map);fprintf(stderr,"warm\n");check(&w,c->after);
        NpeMapPartyEvent event;assert(npe_world_party_event(&w,3,&event)==1);check_event(&event,&c->after[2]);
        /* With fewer than three records, the original service still uses
           its retained slot3. The three-record case replaces that slot. */
        NpeMap map;assert(!npe_map(&file,c->map-1,&map));int third=npe_map_party_event(&map,3,&event);assert(third>=0);
        if(!third){
            w.game_mode=10;w.story_event_id=3;assert(npe_world_apply_party_event(&file,&w)==1&&w.game_mode==1&&w.story_event_id==1);
            assert(w.party_ids[1]==2&&w.party_active[1]==1);check(&w,c->after);
        }
        assert(!npe_world_restore(&file,&w,path));check(&w,c->after);
        assert(!npe_world_load(&file,&cold,path));
        for(unsigned j=0;j<3;++j){int found=npe_map_party_event(&map,j+1,&event);assert(found>=0);
            if(found){Cache expected={event.add_member,event.character_id,event.next_mode,event.next_id};check_event(&cold.party_events[j],&expected);}
            else{const Cache zero={0};check_event(&cold.party_events[j],&zero);}}
        NpeWorld before=w;
        in=fopen(path,"wb");assert(in&&fwrite("NPESAV15",1,8,in)==8&&!fclose(in));
        assert(npe_world_restore(&file,&w,path)==-1&&!memcmp(&w,&before,sizeof(w)));assert(!remove(path));
        /* A malformed table must not partially overwrite live records. */
        NpeMap legacy=map;legacy.tail_size=592;
        assert(!npe_world_load_party_events(&w,&legacy)&&!memcmp(&w,&before,sizeof(w)));
        uint8_t invalid_tail[703]={0};invalid_tail[652+6*3+2]=4;
        NpeMap invalid=map;invalid.tail=invalid_tail;invalid.tail_size=sizeof(invalid_tail);
        assert(npe_world_load_party_events(&w,&invalid)==-1&&!memcmp(&w,&before,sizeof(w)));
        printf("Party cache %s: original warm values,reentry,cold decode and failed transaction passed\n",c->name);
    }
    /* Source-map event8/1 uses the common map loader on teleport. */
    NpeWorld moved=baseline;seed(&moved,cache_cases[0].before);moved.map_number=8;moved.game_mode=16;moved.story_event_id=1;
    NpeMap source;assert(!npe_map(&file,7,&source)&&!npe_world_load_teleport_events(&moved,&source));
    assert(npe_world_apply_teleport_event(&file,&moved)==1);
    NpeMap target;assert(!npe_map(&file,moved.map_number-1,&target));
    for(unsigned j=0;j<3;++j){NpeMapPartyEvent event;int found=npe_map_party_event(&target,j+1,&event);assert(found>=0);
        Cache expected=found?(Cache){event.add_member,event.character_id,event.next_mode,event.next_id}:cache_cases[0].before[j];check_event(&moved.party_events[j],&expected);}
    NpeWorld empty=baseline;empty.game_mode=10;empty.story_event_id=3;
    assert(npe_world_apply_party_event(&file,&empty)==1&&empty.game_mode==0&&empty.story_event_id==0);
    npe_free(&file);puts("Party event cache:10 original UI load cases,retained slots,teleport,cold empty service and transactional failures passed");return 0;
}
