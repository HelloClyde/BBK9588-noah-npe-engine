#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#ifdef NPE_BATTLE_TYPE_WORDS
#include "battle_item_type_words_native.h"
#define NATIVE_PREFIX "tmp/native-battle-item-type-words-"
#define ACTUAL_PREFIX "tmp/battle-type-words-c-item-"
#elif defined(NPE_SELECTOR_TARGET)
#include "battle_item_selector_target_native.h"
#define NATIVE_PREFIX "tmp/native-battle-item-selector-target-"
#define ACTUAL_PREFIX "tmp/battle-selector-target-c-item-"
#elif defined(NPE_SELECTOR_RESOURCES)
#include "battle_item_resources_native.h"
#define NATIVE_PREFIX "tmp/native-battle-item-resources-"
#define ACTUAL_PREFIX "tmp/battle-selector-resources-c-item-"
#else
#include "battle_item_record_native.h"
#define NATIVE_PREFIX "tmp/native-battle-item-record-"
#endif
#ifdef main
#undef main
#endif
static unsigned case_index,commits,phase_index,total;
static int active;
static NpeWorld *live;
#ifdef NPE_SELECTOR_RESOURCES
static const NpeFile *resource_file;
static unsigned snapshots;
#endif
static Uint32 fake_clock;
static Uint32 ticks(void){fake_clock+=2001;return fake_clock;}
static int poll(SDL_Event *e){(void)e;return 0;}
static int input(SDL_Event *);
static void present(SDL_Renderer *);
#define SDL_GetTicks ticks
#define SDL_PollEvent poll
#define SDL_WaitEvent input
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_PollEvent
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#include "battle_input_keys.h"
static void state(const BattleItemState *s){
#ifdef NPE_SELECTOR_RESOURCES
    assert(live->battle_menu_phase==s->menu && live->battle_resource_flags==s->resourceFlags);
    uint8_t flags[2];assert(!npe_rpg_runtime_read(resource_file,0x527aa4,flags,2));
    assert(((unsigned)flags[0]*256+flags[1])==s->resourceFlags);
    uint8_t sample[146];sample[0]=live->battle_menu_phase;sample[1]=live->battle_actor;
    memcpy(sample+2,flags,2);memcpy(sample+4,live->item_record_bytes,142);
    char path[180];snprintf(path,sizeof(path),ACTUAL_PREFIX "%s-state-%u.bin",battle_item_cases[case_index].name,snapshots++);
    FILE *out=fopen(path,"wb");assert(out && fwrite(sample,1,sizeof(sample),out)==sizeof(sample) && !fclose(out));
#ifdef NPE_SELECTOR_TARGET
    uint8_t target;assert(!npe_rpg_runtime_read(resource_file,0x527ac0,&target,1));
    if(target!=s->selectorTarget)fprintf(stderr,"Selector target: C%u original%u phase%u actor%u\n",target,s->selectorTarget,s->menu,s->actor);
    assert(target==s->selectorTarget && live->battle_display_target==s->selectorTarget);
#ifdef NPE_BATTLE_TYPE_WORDS
    const uint32_t type_addresses[]={0x527aa0,0x527aa6,0x527aaa};
    const unsigned expected[]={s->itemType,s->magicType,s->typeCopy};uint8_t types[6];
    for(unsigned k=0;k<3;++k){assert(!npe_rpg_runtime_read(resource_file,type_addresses[k],types+2*k,2));
        unsigned actual=(unsigned)types[2*k]*256+types[2*k+1];
        if(actual!=expected[k])fprintf(stderr,"type word%u C%u original%u menu%u actor%u\n",k,actual,expected[k],s->menu,s->actor);
        assert(actual==expected[k]);}
    snprintf(path,sizeof(path),ACTUAL_PREFIX "%s-types-%u.bin",battle_item_cases[case_index].name,snapshots-1);
    out=fopen(path,"wb");assert(out && fwrite(types,1,6,out)==6 && !fclose(out));
#endif
    snprintf(path,sizeof(path),ACTUAL_PREFIX "%s-target-%u.bin",battle_item_cases[case_index].name,snapshots-1);
    out=fopen(path,"wb");assert(out && fwrite(&target,1,1,out)==1 && !fclose(out));
#endif
#endif
    const BattleItemCase *c=&battle_item_cases[case_index];
    assert(live->game_mode==s->mode && live->battle_actor==s->actor);
    assert(live->battle_native_targets[0]==s->command[1]);
    assert(live->item_record_id==s->loaded && !memcmp(live->item_record_bytes,s->bytes,142));
    assert(live->item_description_length==s->length && live->shop_description_start==s->start && live->shop_description_end==s->end);
    assert(live->shop_selected_item==s->selected && live->shop_row==s->row && live->inventory_column==s->column);
    assert(npe_world_inventory_word(live,255)==s->alias);
    for(unsigned i=0;i<c->count;++i)assert(live->condition_words[c->ids[i]]==s->stock[i]);
    if(s->command[0]==2){
        unsigned actor=0,offset=s->command[5]==0 && s->menu!=32?4:1;
        assert(live->battle_commands[actor].kind==(c->throwing?3:2));
        assert(live->battle_commands[actor].resource_id==s->command[2]);
        assert(live->battle_commands[actor].target+offset==s->command[1]);
        assert(live->battle_item_types[actor]==s->command[3] && live->battle_item_groups[actor]==s->command[4] && live->battle_item_sides[actor]==s->command[5]);
    }
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const BattleItemCase *c=&battle_item_cases[case_index];assert(commits<c->commits);
    const BattleItemFrame *f=&c->frame[commits];char path[180];
    if(f->index==65535)snprintf(path,sizeof(path),NATIVE_PREFIX "%s-before.idx",c->name);
    else snprintf(path,sizeof(path),NATIVE_PREFIX "%s-%u.idx",c->name,f->index);
    uint8_t native[28800];uint32_t rgba[28800];FILE *in=fopen(path,"rb");assert(in && fread(native,1,sizeof(native),in)==sizeof(native) && !fclose(in));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<28800;++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Battle item %s commit%u C=%u SDL=%u\n",c->name,commits,d,sd);
    if(d){FILE *o=fopen("tmp/battle-item-record-actual.idx","wb");assert(o && fwrite(last_pixel_frame,1,sizeof(native),o)==sizeof(native) && !fclose(o));}
    assert(!d && !sd);state(&f->state);++commits;++total;
#ifdef NPE_SELECTOR_RESOURCES
    snprintf(path,sizeof(path),ACTUAL_PREFIX "%s-frame-%u.idx",c->name,commits-1);
    FILE *out=fopen(path,"wb");assert(out && fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native) && !fclose(out));
#endif
}
static int input(SDL_Event *e){
    const BattleItemCase *c=&battle_item_cases[case_index];
    if(phase_index){assert(commits==c->phase[phase_index-1].end);state(&c->phase[phase_index-1].after);}
    if(phase_index==c->phases)return 0;
    unsigned key=c->phase[phase_index++].key;probe_battle_input(e,key,SDLK_RETURN);
    return 1;
}
int main(int argc,char **argv){assert(argc==2);assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    uint8_t *bytes;size_t size;char error[128];NpeFile file;NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *win=SDL_CreateWindow("battle item cache",0,0,240,120,0);assert(win);
    SDL_Renderer *r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(r);
    SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(t);
    for(case_index=0;case_index<sizeof(battle_item_cases)/sizeof(*battle_item_cases);++case_index){
        const BattleItemCase *c=&battle_item_cases[case_index];const BattleItemState *s=&c->seed;NpeWorld w;
        assert(!npe_world_init(&file,&w,9,29,13));w.game_mode=3;w.story_event_id=1;
        for(unsigned i=0;i<3;++i){w.party_active[i]=1;w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];}
        assert(npe_world_prepare_scripted_battle(&file,&w)==1);live=&w;
        for(unsigned i=0;i<3;++i){w.hp[i]=50;w.max_hp[i]=200;w.qi[i]=100;w.max_qi[i]=120;w.level[i]=99;
            w.battle_enemy_ids[i]=51;w.battle_enemy_hps[i]=50;w.battle_enemy_max_hps[i]=200;
            NpeEnemy enemy;assert(!npe_enemy(&file,50,&enemy));
            w.battle_record_residue[3+i][46]=(uint8_t)(enemy.graphic>>8);
            w.battle_record_residue[3+i][47]=(uint8_t)enemy.graphic;
        }
        memset(w.condition_words,0,sizeof(w.condition_words));for(unsigned i=0;i<c->count;++i)w.condition_words[c->ids[i]]=1;
        assert(!npe_world_set_inventory_word(&w,255,(uint16_t)s->alias));
        w.item_record_id=(uint8_t)s->loaded;memcpy(w.item_record_bytes,s->bytes,142);
        w.item_description_length=(uint16_t)s->length;w.shop_description_start=(uint16_t)s->start;w.shop_description_end=(uint16_t)s->end;
#ifdef NPE_SELECTOR_RESOURCES
        w.battle_resource_flags=(uint16_t)s->resourceFlags;resource_file=&file;snapshots=0;
#ifdef NPE_BATTLE_TYPE_WORDS
        w.battle_selection_item_type=(uint16_t)s->itemType;w.battle_selection_magic_type=(uint16_t)s->magicType;
        w.battle_selection_type_copy=(uint16_t)s->typeCopy;
        const uint8_t next_types[]={c->nextTypes[1],c->nextTypes[0],c->nextTypes[2]};
        assert(!npe_world_record_write(&w,1,1,128,next_types,3));
#endif
#endif
        w.music_enabled=0;assert(!npe_world_resources_open(&file,&w));
        active=1;commits=phase_index=0;SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        show_battle_items(r,t,&file,&font,&w,c->throwing?2:1);active=0;
        assert(phase_index==c->phases && commits==c->commits);state(&c->phase[c->phases-1].after);
        assert(!npe_rpg_runtime_failures(&file));
        npe_rpg_runtime_close(&file);
    }
    npe_font_free(&font);SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);SDL_Quit();npe_free(&file);
    printf("Battle item record:%zu native paths/%u actual SDL LCD commits,142 bytes and reservation fields matched\n",sizeof(battle_item_cases)/sizeof(*battle_item_cases),total);return 0;
}
