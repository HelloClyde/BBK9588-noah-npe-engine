#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#if defined(NPE_BATTLE_BITMAP_EXTENSION)
#define NPE_BATTLE_RESIDENT_BYTES 1
#define NPE_BATTLE_LEARN_GROUP 1
#include "battle_bitmap_extension_native.h"
#define POSTWIN_PREFIX "tmp/native-battle-bitmap-extension-"
#define POSTWIN_ACTUAL "tmp/battle-bitmap-extension-actual.idx"
#elif defined(NPE_BATTLE_RESIDENT_BYTES)
#define NPE_BATTLE_LEARN_GROUP 1
#include "battle_resident_bytes_native.h"
#define POSTWIN_PREFIX "tmp/native-battle-resident-bytes-"
#define POSTWIN_ACTUAL "tmp/battle-resident-bytes-actual.idx"
#elif defined(NPE_BATTLE_LEARN_GROUP)
#include "battle_learning_group_native.h"
#define POSTWIN_PREFIX "tmp/native-battle-learning-group-"
#define POSTWIN_ACTUAL "tmp/battle-learning-group-actual.idx"
#elif defined(NPE_BATTLE_ACTIVE_LEARN)
#include "battle_active_learn_native.h"
#define POSTWIN_PREFIX "tmp/native-battle-active-learn-"
#define POSTWIN_ACTUAL "tmp/battle-active-learn-actual.idx"
#else
#include "battle_postwin_record_native.h"
#define POSTWIN_PREFIX "tmp/native-battle-postwin-record-"
#define POSTWIN_ACTUAL "tmp/battle-postwin-record-actual.idx"
#endif
#ifdef main
#undef main
#endif
static unsigned case_index,frames,total;
static unsigned drop_panels,learn_panels;
static int recording;
static void present(SDL_Renderer *);
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!recording)return;
    const PostwinRecordCase *c=&postwin_record_cases[case_index];assert(frames<c->frames);
    char path[180];snprintf(path,sizeof(path),POSTWIN_PREFIX "%s-%u.idx",c->name,frames);
    uint8_t expected[28800];uint32_t rgba[28800];FILE *in=fopen(path,"rb");
    assert(in && fread(expected,1,sizeof(expected),in)==sizeof(expected) && !fclose(in));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned differences=0,sd=0;
    for(unsigned i=0;i<28800;++i){unsigned v=expected[i];differences+=v!=last_pixel_frame[i];
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Postwin record %s frame%u C=%u SDL=%u\n",c->name,frames,differences,sd);
    if(differences){FILE *out=fopen(POSTWIN_ACTUAL,"wb");assert(out && fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected) && !fclose(out));}
    assert(!differences && !sd);++frames;++total;
}
static void state(const NpeWorld *w,const PostwinRecordCase *c){
    assert(w->game_mode==5 && w->story_event_id==c->event && !w->battle_pending_mask);
    fprintf(stderr,"%s item cache %u/%u skill cache %u/%u\n",c->name,w->item_record_id,c->after_id,w->skill_record_id,c->skill_after_id);
    assert(w->item_record_id==c->after_id && !memcmp(w->item_record_bytes,c->after,142));
    assert(w->skill_record_id==c->skill_after_id && !memcmp(w->skill_record_bytes,c->skill_after,144));
#if defined(NPE_BATTLE_ACTIVE_LEARN) || defined(NPE_BATTLE_LEARN_GROUP)
    assert(!memcmp(w->party_active,c->map_flags,3));
    assert(!memcmp(w->battle_party_record_active,c->battle_flags,3));
#endif
#ifdef NPE_BATTLE_LEARN_GROUP
    assert(!memcmp(w->party_attack_groups,c->map_groups,3));
    assert(!memcmp(w->battle_attack_groups,c->battle_groups,3));
#endif
#ifdef NPE_BATTLE_RESIDENT_BYTES
    static const unsigned offsets[]={45,46,47,48,49,146,147};
    for(unsigned m=0;m<3;++m){uint8_t image[148];assert(!npe_world_battle_record_image(w,m,image));
        for(unsigned j=0;j<sizeof(offsets)/sizeof(*offsets);++j){unsigned o=offsets[j];
            assert(w->party_record_residue[m][o]==c->map_bytes[m][j]);
            assert(image[o]==c->battle_bytes[m][j]);}}
#endif
    static const unsigned ids[]={48,50,159,160};
    for(unsigned i=0;i<4;++i){
        fprintf(stderr,"%s item%u stock %u/%u\n",c->name,ids[i],w->condition_words[ids[i]],c->stock[i]);
        assert(w->condition_words[ids[i]]==c->stock[i]);
    }
    for(unsigned i=0;i<6;++i){unsigned e=i<3?0:i-3,p[13];
        p[0]=i<3?w->hp[i]:w->battle_enemy_hps[e];p[1]=i<3?w->max_hp[i]:w->battle_enemy_max_hps[e];
        p[2]=i<3?w->qi[i]:w->battle_enemy_qis[e];p[3]=i<3?w->max_qi[i]:w->battle_enemy_max_qis[e];
        p[4]=i<3?w->attack[i]+w->battle_skill_attack_bonus[i]:w->battle_enemy_attacks[e];
        p[5]=i<3?w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]:w->battle_enemy_defenses[e];
        p[6]=i<3?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        for(unsigned j=0;j<4;++j)p[7+j]=(uint16_t)(i<3?w->battle_party_status[i][j]:w->battle_enemy_status[e][j]);
        p[11]=i<3?w->experience[i]:w->battle_enemy_experience_rewards[e];
        p[12]=i<3?w->experience_to_next[i]:0;
        for(unsigned j=0;j<13;++j)if(p[j]!=c->records[i][j])fprintf(stderr,"%s record%u field%u %u/%u\n",c->name,i,j,p[j],c->records[i][j]);
        assert(!memcmp(p,c->records[i],sizeof(p)));
    }
}
#ifdef NPE_BATTLE_RESIDENT_BYTES
static void record_phase(void *context,const NpeWorld *w,unsigned action,unsigned target,unsigned phase){
    (void)context;(void)target;(void)phase;
    const PostwinRecordCase *c=&postwin_record_cases[case_index];
    if(action==NPE_BATTLE_ANIMATION_LEVEL&&c->defer_pose){
        NpeWorld *mutable=(NpeWorld *)w;
        mutable->battle_characters[0].pose_ids[0]=(uint16_t)c->pose0;
        mutable->battle_characters[0].pose_ids[1]=(uint16_t)c->pose1;
    }
}
#endif
static void observe(void *context,const NpeWorld *w,unsigned action,unsigned target,unsigned phase){
    uint8_t item[142],skill[144];memcpy(item,w->item_record_bytes,142);memcpy(skill,w->skill_record_bytes,144);
    const PostwinRecordCase *c=&postwin_record_cases[case_index];
    if(action==NPE_BATTLE_ANIMATION_DROP){
        assert(w->item_record_id==c->drop_id && !memcmp(item,c->drop_record,142));
        static const unsigned ids[]={48,50,159,160};
        for(unsigned i=0;i<4;++i)assert(w->condition_words[ids[i]]==c->stock[i]);
        ++drop_panels;
    }
    if(action==NPE_BATTLE_ANIMATION_LEARN){
        assert(w->skill_record_id==c->learn_id && !memcmp(skill,c->learn_record,144));
        ++learn_panels;
    }
    play_battle_animation(context,w,action,target,phase);
#ifdef NPE_BATTLE_RESIDENT_BYTES
    record_phase(NULL,w,action,target,phase);
#endif
    assert(!memcmp(item,w->item_record_bytes,142) && !memcmp(skill,w->skill_record_bytes,144));
}
static void prepare(const NpeFile *file,NpeWorld *w,const PostwinRecordCase *c){
    assert(!npe_world_init(file,w,9,29,13));w->game_mode=3;w->story_event_id=1;
    for(unsigned i=0;i<3;++i){w->party_active[i]=1;w->party_ids[i]=1;w->party_characters[i]=w->party_characters[0];memcpy(w->party_names[i],w->party_names[0],12);memcpy(w->battle_names[i],w->party_names[0],12);w->battle_characters[i]=w->party_characters[0];
#ifdef NPE_BATTLE_RESIDENT_BYTES
        memcpy(w->party_record_residue[i],w->party_record_residue[0],148);
#endif
    }
    /* The original fixture writes monster7626/7627 to51 before entry.
       Loaded enemy pose records now drive drawing, so changing only the
       later host lookup IDs cannot reproduce that original setup. */
    assert(!npe_world_resident_write(w,UINT32_C(0x527626),(const uint8_t[]){51,51},2));
    assert(npe_world_prepare_scripted_battle(file,w)==1);
    NpeCharacter character;assert(!npe_character(file,0,&character));
    for(unsigned i=0;i<3;++i){
        w->hp[i]=50;w->max_hp[i]=200;w->qi[i]=100;w->max_qi[i]=120;w->level[i]=(uint8_t)(c->learn && !i?1:99);
        w->experience[i]=(uint16_t)(c->learn && !i?9:0);w->experience_to_next[i]=10;
        memcpy(w->growth[i],character.growth,7);memcpy(w->skill_slots[i],c->skills,18);
        memset(w->equipped_items[i],0,6);memset(w->status_guard[i],0,8);
        w->attack[i]=90;w->defense[i]=w->battle_party_defenses[i]=0;
        w->other_stats[i][0]=1;w->other_stats[i][1]=(uint16_t)(c->reverse?(i==0?997:i==1?999:998):999-i);w->other_stats[i][2]=1;
        memset(w->battle_party_status[i],0,sizeof(w->battle_party_status[i]));
        w->battle_enemy_ids[i]=51;w->battle_enemy_hps[i]=(uint16_t)(i==1?c->enemy_hp:0);w->battle_enemy_max_hps[i]=2000;
        w->battle_enemy_qis[i]=100;w->battle_enemy_max_qis[i]=120;w->battle_enemy_spell_stats[i]=1;
        w->battle_enemy_attacks[i]=w->battle_enemy_base_attacks[i]=1;w->battle_enemy_defenses[i]=w->battle_enemy_base_defenses[i]=0;
        w->battle_enemy_speeds[i]=w->battle_enemy_lucks[i]=1;w->battle_enemy_damage_floor_stats[i]=99;
        w->battle_enemy_gold_rewards[i]=0;w->battle_enemy_experience_rewards[i]=(uint16_t)(i==1?1:0);
        memset(w->battle_enemy_status[i],0,8);w->battle_enemy_status[i][2]=-36;memset(w->battle_enemy_skill_ids[i],0,3);
        memset(w->battle_enemy_drop_ids[i],0,2);
    }
    memcpy(w->battle_enemy_drop_ids,c->drops,sizeof(c->drops));
    for(unsigned i=0;i<255;++i)w->condition_words[i]=0;
    w->condition_words[159]=(uint16_t)(c->second==159?2:1);if(c->second && c->second!=159)w->condition_words[c->second]=1;
    assert(npe_world_battle_throw_item(file,w,159,1)==NPE_BATTLE_CONTINUE && w->battle_actor==1);
    assert((c->second?npe_world_battle_throw_item(file,w,c->second,1):npe_world_battle_defend(w))==NPE_BATTLE_CONTINUE && w->battle_actor==2);
    w->item_record_id=(uint8_t)c->cached;memcpy(w->item_record_bytes,c->before,142);
    w->skill_record_id=(uint8_t)c->skill_before_id;memcpy(w->skill_record_bytes,c->skill_before,144);
#if defined(NPE_BATTLE_ACTIVE_LEARN) || defined(NPE_BATTLE_LEARN_GROUP)
    w->battle_party_record_active[0]=(uint8_t)c->active_byte;
#endif
#ifdef NPE_BATTLE_LEARN_GROUP
    w->battle_attack_groups[0]=(uint8_t)c->group_byte;
#endif
#ifdef NPE_BATTLE_RESIDENT_BYTES
    w->battle_record_residue[0][45]=(uint8_t)c->gap_byte;
    w->battle_record_residue[0][146]=(uint8_t)c->tail_before[0];
    w->battle_record_residue[0][147]=(uint8_t)c->tail_before[1];
    if(!c->defer_pose){w->battle_characters[0].pose_ids[0]=(uint16_t)c->pose0;
        w->battle_characters[0].pose_ids[1]=(uint16_t)c->pose1;}
#endif
    w->music_enabled=0;
}
#if defined(NPE_BATTLE_ACTIVE_LEARN) || defined(NPE_BATTLE_LEARN_GROUP)
#ifdef NPE_BATTLE_RESIDENT_BYTES
static void save_v21_prefix(const NpeFile *file,const NpeWorld *saved,const char *path){
    uint8_t wire[4096];FILE *in=fopen(path,"rb");assert(in);
    size_t size=fread(wire,1,sizeof(wire),in);assert(!fclose(in)&&size>480&&!memcmp(wire,"NPESAV22",8));
    assert(!memcmp(wire+size-NPE_CHARACTER_RECORD_SAVE_BYTES,saved->party_record_residue,NPE_CHARACTER_RECORD_SAVE_BYTES));
    size-=NPE_CHARACTER_RECORD_SAVE_BYTES;memcpy(wire,"NPESAV21",8);
    uint64_t sum=UINT64_C(14695981039346656037);
    for(size_t i=36;i<size;++i)sum=(sum^wire[i])*UINT64_C(1099511628211);
    for(unsigned i=0;i<4;++i)wire[24+i]=(uint8_t)((size-36)>>(8*i));
    for(unsigned i=0;i<8;++i)wire[28+i]=(uint8_t)(sum>>(8*i));
    char legacy[160];snprintf(legacy,sizeof(legacy),POSTWIN_PREFIX "v21-save-%u.sav",case_index);
    FILE *out=fopen(legacy,"wb");assert(out&&fwrite(wire,1,size,out)==size&&!fclose(out));
    NpeWorld loaded;assert(!npe_world_load(file,&loaded,legacy));
    assert(!memcmp(loaded.party_names,saved->party_names,36));
    for(unsigned m=0;m<3;++m){uint8_t defaults[148]={0};
        assert(!npe_character_record_seed(file,saved->party_ids[m]-1,defaults));
        assert(!memcmp(loaded.party_record_residue[m],defaults,148));}
    assert(!remove(legacy));
}
#endif
static void save_flags(const NpeFile *file,const NpeWorld *world){
    NpeWorld saved=*world,loaded;
    /* Keep this host-save fixture's IDs unique; the battle diagnostic
       deliberately cloned all three original character records. */
    memcpy(saved.party_ids,(uint8_t[]){1,2,3},3);
    /* Own saves are made on the map, after the victory story has returned. */
    saved.game_mode=1;saved.story_event_id=0;
    char path[128];snprintf(path,sizeof(path),POSTWIN_PREFIX "save-%u.sav",case_index);
    assert(!npe_world_save(file,&saved,path));
#ifdef NPE_BATTLE_RESIDENT_BYTES
    save_v21_prefix(file,&saved,path);
#endif
    assert(!npe_world_load(file,&loaded,path));
    assert(!memcmp(loaded.party_record_residue,saved.party_record_residue,sizeof(saved.party_record_residue)));
    assert(!memcmp(loaded.party_active,saved.party_active,3));
    memset(loaded.battle_party_record_active,255,3);
    assert(!npe_world_restore(file,&loaded,path));
    assert(!memcmp(loaded.party_record_residue,saved.party_record_residue,sizeof(saved.party_record_residue)));
    assert(!memcmp(loaded.party_active,saved.party_active,3));
    loaded.game_mode=3;loaded.story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,&loaded)==1);
    assert(!memcmp(loaded.battle_party_record_active,saved.party_active,3));
    for(unsigned m=0;m<3;++m)assert(loaded.battle_record_residue[m][45]==saved.party_record_residue[m][45]&&
        loaded.battle_record_residue[m][147]==saved.party_record_residue[m][147]);
#ifdef NPE_BATTLE_LEARN_GROUP
    assert(!memcmp(loaded.party_attack_groups,saved.party_attack_groups,3));
    assert(!memcmp(loaded.battle_attack_groups,saved.party_attack_groups,3));
#endif
    assert(!remove(path));
}
#endif
int main(int argc,char **argv){
    assert(argc==2 && !SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;NpeFont font={0};uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *win=SDL_CreateWindow("native postwin cache",0,0,240,120,0);assert(win);
    SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(case_index=0;case_index<sizeof(postwin_record_cases)/sizeof(*postwin_record_cases);++case_index){
        const PostwinRecordCase *c=&postwin_record_cases[case_index];NpeWorld w;prepare(&file,&w,c);
        BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
        /* The original battle-entry scene was drawn before diagnostic HP
          changes. Preserve that cache, including the two now-dead poses. */
        NpeWorld entry=w;entry.battle_enemy_hps[0]=entry.battle_enemy_hps[2]=1;
        assert(!npe_render_battle_actors_scene(&file,&entry,display.actors));
        display.cached=1;display.epoch=w.battle_display_epoch;w.battle_scene_refreshed=1;
        w.battle_animation_observer=observe;w.battle_animation_context=&display;
        frames=0;recording=1;assert(npe_world_battle_defend(&w)==NPE_BATTLE_WON);
        NpeStoryRecord story;assert(!npe_story_record(&file,w.story_event_id-1,&story));
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];size_t end=0;
        assert(!npe_render_native_story_page(&file,&font,&story,0,&end,pixels) && end==story.text_size);
        draw_pixels(renderer,texture,pixels);recording=0;assert(frames==c->frames);state(&w,c);
#if defined(NPE_BATTLE_ACTIVE_LEARN) || defined(NPE_BATTLE_LEARN_GROUP)
        save_flags(&file,&w);
#endif
        NpeWorld headless;prepare(&file,&headless,c);
#ifdef NPE_BATTLE_RESIDENT_BYTES
        headless.battle_animation_observer=record_phase;
#endif
        assert(npe_world_battle_defend(&headless)==NPE_BATTLE_WON);state(&headless,c);
    }
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);SDL_Quit();
#if defined(NPE_BATTLE_BITMAP_EXTENSION)
    assert(!drop_panels && learn_panels==8);
#elif defined(NPE_BATTLE_RESIDENT_BYTES)
    assert(!drop_panels && learn_panels==28);
#elif defined(NPE_BATTLE_LEARN_GROUP)
    assert(!drop_panels && learn_panels==25);
#elif defined(NPE_BATTLE_ACTIVE_LEARN)
    assert(!drop_panels && learn_panels==1);
#else
    assert(drop_panels==6 && learn_panels==3);
#endif
    printf("Postwin shared record:%zu original paths/%u actual C/SDL LCD commits,142+144 bytes,78 record fields,%u drop/%u learn panels,stock and headless state passed\n",sizeof(postwin_record_cases)/sizeof(*postwin_record_cases),total,drop_panels,learn_panels);return 0;
}
