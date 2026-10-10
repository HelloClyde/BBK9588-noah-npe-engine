#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#include "character_metadata_native.h"
#ifdef main
#undef main
#endif
static unsigned case_index,frames,total;
static int recording;
static void present(SDL_Renderer *);
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
static const char *path="tmp/character-metadata-codec.npes";
static void equal(const NpeCharacterMetadata *a,const NpeCharacterMetadata *b,unsigned n){
    for(unsigned i=0;i<n;++i){assert(a[i].sex==b[i].sex);
        assert(!memcmp(a[i].pose_ids,b[i].pose_ids,sizeof(a[i].pose_ids)));
        assert(!memcmp(a[i].animation_ids,b[i].animation_ids,3));}
}
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!recording)return;
    char p[180];assert(frames<4);
#ifdef NPE_CHARACTER_NAMES
    snprintf(p,sizeof(p),"tmp/native-character-names-%s-commit-%u.idx",metadata_cases[case_index].name,frames);
#else
    snprintf(p,sizeof(p),"tmp/native-character-metadata-%s-commit-%u.idx",metadata_cases[case_index].name,frames);
#endif
    uint8_t expected[28800];uint32_t rgba[28800];FILE *f=fopen(p,"rb");
    assert(f&&fread(expected,1,28800,f)==28800&&!fclose(f));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned diff=0,sdl=0;
    for(unsigned i=0;i<28800;++i){unsigned v=expected[i];diff+=v!=last_pixel_frame[i];
        sdl+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Metadata %s entry%u C=%u SDL=%u\n",metadata_cases[case_index].name,frames,diff,sdl);
    if(diff){snprintf(p,sizeof(p),"tmp/character-metadata-%s-%u-actual.idx",metadata_cases[case_index].name,frames);
        f=fopen(p,"wb");assert(f&&fwrite(last_pixel_frame,1,28800,f)==28800&&!fclose(f));}
    assert(!diff&&!sdl);++frames;++total;
}
static size_t read_bytes(uint8_t b[4096]){
    FILE *f=fopen(path,"rb");assert(f);size_t n=fread(b,1,4096,f);assert(!fclose(f));return n;
}
static void write_bytes(uint8_t *b,size_t n){
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=36;i<n;++i)hash=(hash^b[i])*UINT64_C(1099511628211);
    for(unsigned i=0;i<4;++i)b[24+i]=(uint8_t)((n-36)>>(8*i));
    for(unsigned i=0;i<8;++i)b[28+i]=(uint8_t)(hash>>(8*i));
    FILE *f=fopen(path,"wb");assert(f&&fwrite(b,1,n,f)==n&&!fclose(f));
}
static void defaults(const NpeFile *f,const NpeWorld *w){
    for(unsigned i=0;i<3;++i){NpeCharacterMetadata m;
        assert(!npe_character_metadata(f,w->party_ids[i]-1,&m));equal(&w->party_characters[i],&m,1);}
}
#ifdef NPE_CHARACTER_NAMES
#include "character_name_checks.h"
#endif
static void save_checks(const NpeFile *f,NpeWorld *w){
    NpeWorld loaded;uint8_t bytes[4096],original[4096];
    assert(!npe_world_save(f,w,path));size_t n=read_bytes(bytes);memcpy(original,bytes,n);
    assert(!memcmp(bytes,"NPESAV22",8));
    unsigned pos=(unsigned)n-96-NPE_CHARACTER_RECORD_SAVE_BYTES;
    for(unsigned i=0;i<3;++i){const NpeCharacterMetadata *m=&w->party_characters[i];
        assert(bytes[pos++]==m->sex);
        for(unsigned j=0;j<8;++j){assert((bytes[pos]|(unsigned)bytes[pos+1]<<8)==m->pose_ids[j]);pos+=2;}
        assert(!memcmp(bytes+pos,m->animation_ids,3));pos+=3;}
    assert(pos==n-36-NPE_CHARACTER_RECORD_SAVE_BYTES && !npe_world_load(f,&loaded,path));equal(loaded.party_characters,w->party_characters,3);
    memset(loaded.battle_characters,255,sizeof(loaded.battle_characters));
    assert(!npe_world_save(f,&loaded,path)&&read_bytes(bytes)==n&&!memcmp(bytes,original,n));
    memset(loaded.party_characters,254,sizeof(loaded.party_characters));
    assert(!npe_world_restore(f,&loaded,path));equal(loaded.party_characters,w->party_characters,3);
    memcpy(bytes,original,n);memcpy(bytes,"NPESAV19",8);write_bytes(bytes,n-96-NPE_CHARACTER_RECORD_SAVE_BYTES);
    assert(!npe_world_load(f,&loaded,path));defaults(f,&loaded);
    memset(loaded.party_characters,253,sizeof(loaded.party_characters));
    assert(!npe_world_restore(f,&loaded,path));defaults(f,&loaded);
    NpeWorld before=loaded;memcpy(bytes,original,n);write_bytes(bytes,n-1);
    assert(npe_world_restore(f,&loaded,path)<0&&!memcmp(&before,&loaded,sizeof(loaded)));
    memcpy(bytes,original,n);bytes[n]=0;write_bytes(bytes,n+1);
    assert(npe_world_load(f,&loaded,path)<0&&!memcmp(&before,&loaded,sizeof(loaded)));
    assert(!npe_world_restart_game(f,&loaded));defaults(f,&loaded);assert(!remove(path));
}
static void nonzero_resource_fixture(const NpeFile *original){
    uint8_t *copy=malloc(original->size);assert(copy);memcpy(copy,original->bytes,original->size);
    NpeFile f;char error[128];assert(!npe_parse(&f,copy,original->size,error,sizeof(error)));
    for(unsigned i=0;i<3;++i){NpeCharacter c;assert(!npe_character(&f,i,&c));size_t o=(size_t)(c.raw-f.bytes);
        for(unsigned j=0;j<6;++j)copy[o+13+j]=(uint8_t)(10+6*i+j);
        copy[o+77]=239;copy[o+79]=(uint8_t[]){1,2,255}[i];}
    NpeWorld w,loaded;assert(!npe_world_init(&f,&w,9,29,13));
    for(unsigned i=0;i<3;++i){assert(w.party_attack_groups[i]==((uint8_t[]){1,2,255})[i]);
        for(unsigned j=0;j<6;++j)assert(w.equipped_items[i][j]==10+6*i+j);}
    memcpy(w.party_ids,(uint8_t[]){3,1,2},3);
    assert(!npe_world_save(&f,&w,path));uint8_t b[4096];size_t n=read_bytes(b);
    memcpy(b,"NPESAV19",8);memset(b+36+1376,0,5);write_bytes(b,n-96-NPE_CHARACTER_RECORD_SAVE_BYTES);
    assert(!npe_world_load(&f,&loaded,path));defaults(&f,&loaded);
    assert(!memcmp(loaded.party_attack_groups,(uint8_t[]){255,1,2},3));
    assert(!remove(path));npe_free(&f);
}
static void sex_restrictions(const NpeFile *file){
    unsigned ids[3]={0};
    for(unsigned i=1;i<255;++i){NpeItem item;if(!npe_item(file,i-1,&item)&&item.flags<3&&!ids[item.flags])ids[item.flags]=i;}
    assert(ids[1]&&ids[2]);
    for(unsigned flags=1;flags<=2;++flags)for(unsigned sex=0;sex<=3;++sex){
        NpeWorld w;assert(!npe_world_init(file,&w,9,29,13));w.party_characters[0].sex=(uint8_t)sex;
        w.condition_words[ids[flags]]=1;
        int result=npe_world_equip_item(file,&w,ids[flags],0);
        assert((result==-2)==((flags==1&&sex==2)||(flags==2&&sex==1)));
    }
}
int main(int argc,char **argv){
    assert(argc==2&&!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    NpeFile f;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *win=SDL_CreateWindow("resident characters",0,0,240,120,0);assert(win);
    SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(case_index=0;case_index<3;++case_index){const MetadataCase *c=&metadata_cases[case_index];NpeWorld w;
        assert(!npe_world_init(&f,&w,9,29,13));equal(w.party_characters,c->defaults,3);
        assert(!memcmp(w.equipped_items,c->gear,18));NpeWorld initial=w;
        for(unsigned i=0;i<3;++i){unsigned j=c->ids[i]-1;
#define COPY(field) memcpy(&w.field[i],&initial.field[j],sizeof(w.field[i]))
            COPY(party_names);COPY(party_ids);COPY(skill_slots);COPY(equipped_items);COPY(max_hp);COPY(qi);COPY(max_qi);
            COPY(attack);COPY(defense);COPY(level);COPY(growth);COPY(other_stats);
            COPY(experience);COPY(experience_to_next);COPY(damage_floor_stat);COPY(status_guard);
#undef COPY
            w.hp[i]=(uint16_t)(80+10*i);w.party_active[i]=c->flags[i];w.party_attack_groups[i]=(uint8_t[]){0,2,255}[i];}
        memcpy(w.party_characters,c->before,sizeof(w.party_characters));
#ifdef NPE_CHARACTER_NAMES
        const NameCase *nc=&name_cases[case_index];
        memcpy(w.party_names,nc->before,36);
#endif
        w.game_mode=3;w.story_event_id=1;w.story_progress=6;assert(npe_world_prepare_scripted_battle(&f,&w)==1);
        equal(w.battle_characters,c->entry,c->count);equal(w.party_characters,c->before,3);
#ifdef NPE_CHARACTER_NAMES
        assert(!memcmp(w.battle_names,nc->entry,12*c->count)&&!memcmp(w.party_names,nc->before,36));
#endif
        frames=0;recording=1;assert(!draw_battle_entry(renderer,texture,&f,&w,&font));recording=0;assert(frames==4);
        memcpy(w.battle_characters,c->mutated,c->count*sizeof(w.battle_characters[0]));equal(w.party_characters,c->before,3);
#ifdef NPE_CHARACTER_NAMES
        memcpy(w.battle_names,nc->mutated,12*c->count);alias_names(&f,&w,nc);
#endif
        for(unsigned i=0;i<c->count;++i)assert(npe_world_battle_pose(&w,i,0)==c->mutated[i].pose_ids[0]);
        w.story_event_id=0;npe_world_seed(&w,1);assert(npe_world_battle_flee(&w)==NPE_BATTLE_FLED);
        equal(w.party_characters,c->exit,3);save_checks(&f,&w);
#ifdef NPE_CHARACTER_NAMES
        assert(!memcmp(w.party_names,nc->exit,36));save_names(&f,&w);
#endif
        if(c->removed){NpeMapPartyEvent event={0};event.character_id=2;assert(npe_world_change_party(&w,&event)==3);
            equal(w.party_characters,c->swapped,3);save_checks(&f,&w);
#ifdef NPE_CHARACTER_NAMES
            assert(!memcmp(w.party_names,nc->swapped,36));save_names(&f,&w);
#endif
        }
        w.game_mode=3;w.story_event_id=1;assert(npe_world_prepare_scripted_battle(&f,&w)==1);
        equal(w.battle_characters,c->reentry,c->reentry_count);
#ifdef NPE_CHARACTER_NAMES
        assert(!memcmp(w.battle_names,nc->reentry,12*c->reentry_count));
#endif
    }
    sex_restrictions(&f);nonzero_resource_fixture(&f);
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));
    npe_font_free(&font);npe_free(&f);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);SDL_Quit();
#ifdef NPE_CHARACTER_NAMES
    printf("Resident names:3 original lifetimes,1 swap,1 native empty alias cancel,word255 wrap,4 save sequences,V20 names by ID,%u C/SDL entry commits passed\n",total);
#endif
    printf("Resident character metadata:3 original lifetimes,1 party swap,%u C/SDL entry commits,4 save sequences,V19 migration,nonzero resource fields,8 sex gates passed\n",total);return 0;
}
