#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "character_group_lifecycle_native.h"

enum { GROUP_WIRE_OFFSET=36+1376 };
static const char *path="tmp/character-group-lifecycle.npes";
static void map_check(const NpeWorld *w,const GroupState *s){
    assert(!memcmp(w->party_ids,s->ids,3));
    assert(!memcmp(w->party_active,s->flags,3));
    assert(!memcmp(w->party_attack_groups,s->map,3));
}
static void battle_check(const NpeWorld *w,const GroupState *s){
    assert(w->game_mode==3 && !memcmp(w->party_ids,s->ids,3));
    assert(!memcmp(w->party_attack_groups,s->map,3));
    for(unsigned i=0;i<3;++i)assert(w->party_active[i]==(i<s->count));
    assert(!memcmp(w->battle_attack_groups,s->battle,s->count));
}
static size_t read_bytes(uint8_t bytes[4096]){
    FILE *f=fopen(path,"rb");assert(f);size_t size=fread(bytes,1,4096,f);assert(!fclose(f));return size;
}
static void write_bytes(uint8_t *b,size_t n){
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=36;i<n;++i){hash^=b[i];hash*=UINT64_C(1099511628211);}
    for(unsigned i=0;i<4;++i)b[24+i]=(uint8_t)((n-36)>>(8*i));
    for(unsigned i=0;i<8;++i)b[28+i]=(uint8_t)(hash>>(8*i));
    FILE *f=fopen(path,"wb");assert(f && fwrite(b,1,n,f)==n && !fclose(f));
}
static void defaults(const NpeFile *f,const NpeWorld *w){
    for(unsigned i=0;i<3;++i){NpeCharacter c;assert(!npe_character(f,w->party_ids[i]-1,&c));
        assert(w->party_attack_groups[i]==c.raw[79]);}
}
static void saved_groups(const NpeFile *f,NpeWorld *w){
    NpeWorld loaded,old;uint8_t bytes[4096],baseline[4096];
    assert(!npe_world_save(f,w,path));size_t size=read_bytes(bytes);memcpy(baseline,bytes,size);
    assert(size>GROUP_WIRE_OFFSET+5 && !memcmp(bytes,"NPESAV22",8));
    assert(bytes[GROUP_WIRE_OFFSET]=='G' && bytes[GROUP_WIRE_OFFSET+1]=='P');
    assert(!memcmp(bytes+GROUP_WIRE_OFFSET+2,w->party_attack_groups,3));
    assert(!npe_world_load(f,&loaded,path));
    assert(!memcmp(loaded.party_attack_groups,w->party_attack_groups,3));
    assert(!memcmp(loaded.party_ids,w->party_ids,3));
    old=loaded;memset(loaded.battle_attack_groups,99,3);
    assert(!npe_world_save(f,&loaded,path) && read_bytes(bytes)==size && !memcmp(baseline,bytes,size));
    assert(!npe_world_restore(f,&loaded,path) && !memcmp(loaded.party_attack_groups,w->party_attack_groups,3));
    /* Genuine old V19 reserved zeros: reconstruct by saved party ID. */
    memcpy(bytes,baseline,size);memset(bytes+GROUP_WIRE_OFFSET,0,5);memcpy(bytes,"NPESAV19",8);write_bytes(bytes,size-96-NPE_CHARACTER_RECORD_SAVE_BYTES);
    assert(!npe_world_load(f,&loaded,path));defaults(f,&loaded);
    memset(loaded.party_attack_groups,251,3);
    assert(!npe_world_restore(f,&loaded,path));defaults(f,&loaded);
    /* V18 may contain arbitrary old host-session bytes. A coincident GP
       marker in that old layout must not become character data. */
    memcpy(bytes,baseline,size);memcpy(bytes,"NPESAV18",8);write_bytes(bytes,size-128-NPE_CHARACTER_RECORD_SAVE_BYTES);
    assert(!npe_world_load(f,&loaded,path));defaults(f,&loaded);
    /* A bad file must leave the entire live world untouched. */
    loaded=old;NpeWorld before=loaded;FILE *bad=fopen(path,"wb");
    assert(bad && fwrite(baseline,1,40,bad)==40 && !fclose(bad));
    assert(npe_world_restore(f,&loaded,path)<0 && !memcmp(&before,&loaded,sizeof(loaded)));
    assert(!npe_world_restart_game(f,&loaded));defaults(f,&loaded);
    assert(!remove(path));
}
static void resource_defaults_migration(const NpeFile *original){
    /* A separate in-memory NPE fixture makes the resource groups distinct,
       so a reordered legacy save cannot pass by returning three zeros. */
    uint8_t *copy=malloc(original->size);assert(copy);memcpy(copy,original->bytes,original->size);
    NpeFile file;char error[128];assert(!npe_parse(&file,copy,original->size,error,sizeof(error)));
    for(unsigned i=0;i<3;++i){NpeCharacter c;assert(!npe_character(&file,i,&c));
        copy[(size_t)(c.raw-file.bytes)+79]=(uint8_t[]){1,2,255}[i];}
    NpeWorld w,loaded;assert(!npe_world_init(&file,&w,9,29,13));
    memcpy(w.party_ids,(uint8_t[]){3,1,2},3);memset(w.party_attack_groups,83,3);
    assert(!npe_world_save(&file,&w,path));uint8_t b[4096];size_t n=read_bytes(b);
    memset(b+GROUP_WIRE_OFFSET,0,5);memcpy(b,"NPESAV19",8);write_bytes(b,n-96-NPE_CHARACTER_RECORD_SAVE_BYTES);
    assert(!npe_world_load(&file,&loaded,path));defaults(&file,&loaded);
    assert(!memcmp(loaded.party_attack_groups,(uint8_t[]){255,1,2},3));
    assert(!remove(path));npe_free(&file);
}
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f && !fseek(f,0,SEEK_END));
    long length=ftell(f);assert(length>0 && !fseek(f,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)length);assert(bytes && fread(bytes,1,(size_t)length,f)==(size_t)length && !fclose(f));
    NpeFile file;char error[128];assert(!npe_parse(&file,bytes,(size_t)length,error,sizeof(error)));
    for(unsigned i=0;i<3;++i){
        const GroupLifecycleCase *c=&group_lifecycle_cases[i];NpeWorld w;
        assert(!npe_world_init(&file,&w,9,29,13));defaults(&file,&w);
        memcpy(w.party_ids,c->entry.ids,3);memcpy(w.party_active,c->entry.flags,3);
        memcpy(w.party_attack_groups,c->entry.map,3);
        w.game_mode=3;w.story_event_id=1;assert(npe_world_prepare_scripted_battle(&file,&w)==1);
        battle_check(&w,&c->entry);assert(npe_world_battle_select_attack(&w,0)==1);
        assert(w.battle_item_groups[0]==c->entry.battle[0]);
        for(unsigned m=0;m<c->entry.count;++m)w.battle_attack_groups[m]=(uint8_t[]){1,255,2}[m];
        w.story_event_id=0;npe_world_seed(&w,1);assert(npe_world_battle_flee(&w)==NPE_BATTLE_FLED);
        assert(w.game_mode==1 && !w.battle_party_flags_saved);map_check(&w,&c->exit);saved_groups(&file,&w);
        if(c->remove){NpeMapPartyEvent event={0};event.character_id=2;
            assert(npe_world_change_party(&w,&event)==3);map_check(&w,&c->removed);saved_groups(&file,&w);}
        w.game_mode=3;w.story_event_id=1;assert(npe_world_prepare_scripted_battle(&file,&w)==1);
        battle_check(&w,&c->reentry);assert(npe_world_battle_select_attack(&w,0)==1);
        assert(w.battle_item_groups[0]==c->reentry.battle[0]);
        printf("Character group lifecycle:%s entry/flee/map/swap/reentry/legacy saves passed\n",c->name);
    }
    resource_defaults_migration(&file);
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));npe_free(&file);
    puts("Character group lifecycle:3 original paths,1 original party swap,4 local save sequences,old V19/V18 migration,reordered resource defaults,warm restore/restart and failure transaction passed");
    return 0;
}
