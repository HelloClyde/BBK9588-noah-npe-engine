#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#include "battle_item_resident_native.h"
#ifdef main
#undef main
#endif
static unsigned case_index,frames,total,phase_index,states_checked,conversions;
static int recording;
static void present(SDL_Renderer *);
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
/* The original input selects defense from resident menu31. */
static int choose_defense(NpeWorld *w){
    w->battle_menu_phase=31;return npe_world_battle_defend(w);
}
static void stage(const NpeWorld *w,const ResidentStage *s,int include_kind){
    assert(w->condition_words[46]==s->stock[0] && w->condition_words[159]==s->stock[1]);
    for(unsigned i=0;i<3;++i){unsigned p[]={w->battle_native_kinds[i],w->battle_item_ids[i],w->battle_item_types[i],w->battle_item_type_copies[i],w->battle_item_groups[i],w->battle_item_sides[i],w->battle_skill_ids[i],w->battle_native_targets[i]};
        for(unsigned j=include_kind?0:1;j<8;++j){
            if(p[j]!=s->commands[i][j])fprintf(stderr,"%s actor%u field%u %u/%u\n",resident_cases[case_index].name,i,j,p[j],s->commands[i][j]);
            assert(p[j]==s->commands[i][j]);
        }
    }
    ++states_checked;
}
static void mark(const NpeWorld *w){
    const ResidentCase *c=&resident_cases[case_index];assert(phase_index<c->phase_count);
    stage(w,&c->phases[phase_index++],1);
}
static void item(const NpeFile *f,NpeWorld *w,unsigned id,int cancel){
    assert(!(id==159?npe_world_battle_reserve_throw_item(f,w,id):npe_world_battle_reserve_use_item(f,w,id)));mark(w);
    if(cancel){assert(npe_world_battle_cancel_item_selection(w)==1);mark(w);assert(npe_world_battle_cancel_target(w)==1);mark(w);}
    else assert((id==159?npe_world_battle_throw_item(f,w,id,0):npe_world_battle_use_item(f,w,id,0))==NPE_BATTLE_CONTINUE);
}
static void prepare(const NpeFile *f,NpeWorld *w){
    const ResidentCase *c=&resident_cases[case_index];assert(!npe_world_init(f,w,9,29,13));w->game_mode=3;w->story_event_id=1;
    for(unsigned i=0;i<3;++i){w->party_active[i]=1;w->party_ids[i]=1;w->party_characters[i]=w->party_characters[0];memcpy(w->party_names[i],w->party_names[0],12);memcpy(w->battle_names[i],w->party_names[0],12);w->battle_characters[i]=w->party_characters[0];}
    assert(npe_world_prepare_scripted_battle(f,w)==1);
    for(unsigned i=0;i<3;++i){
        w->hp[i]=50;w->max_hp[i]=200;w->qi[i]=100;w->max_qi[i]=120;w->level[i]=99;
        w->attack[i]=90;w->defense[i]=w->battle_party_defenses[i]=0;w->experience[i]=0;w->experience_to_next[i]=10;
        w->other_stats[i][0]=1;w->other_stats[i][1]=(uint16_t)(999-i);w->other_stats[i][2]=1;
        memset(w->battle_party_status[i],0,8);memset(w->equipped_items[i],0,6);memset(w->skill_slots[i],0,18);w->skill_slots[i][0]=6;
        w->battle_enemy_ids[i]=51;w->battle_enemy_hps[i]=1000;w->battle_enemy_max_hps[i]=2000;
        /* Match the original fixture's three physical enemy image words. */
        NpeEnemy enemy;assert(!npe_enemy(f,50,&enemy));
        w->battle_record_residue[3+i][46]=(uint8_t)(enemy.graphic>>8);
        w->battle_record_residue[3+i][47]=(uint8_t)enemy.graphic;
        w->battle_enemy_qis[i]=100;w->battle_enemy_max_qis[i]=120;w->battle_enemy_spell_stats[i]=1;
        w->battle_enemy_attacks[i]=w->battle_enemy_base_attacks[i]=1;w->battle_enemy_defenses[i]=w->battle_enemy_base_defenses[i]=0;
        w->battle_enemy_speeds[i]=w->battle_enemy_lucks[i]=1;w->battle_enemy_damage_floor_stats[i]=99;
        w->battle_enemy_gold_rewards[i]=w->battle_enemy_experience_rewards[i]=0;memset(w->battle_enemy_drop_ids[i],0,2);
        memset(w->battle_enemy_status[i],0,8);w->battle_enemy_status[i][2]=-36;memset(w->battle_enemy_skill_ids[i],0,3);
    }
    for(unsigned i=0;i<255;++i)w->condition_words[i]=0;
    w->condition_words[46]=w->condition_words[159]=1;
    phase_index=0;stage(w,&c->initial,1);
    if(case_index==0 || case_index==3){item(f,w,case_index==3?159:46,1);assert(npe_world_battle_attack(w,0)==NPE_BATTLE_CONTINUE);assert(choose_defense(w)==NPE_BATTLE_CONTINUE);}
    else if(case_index==1 || case_index==2){assert(npe_world_battle_attack(w,0)==NPE_BATTLE_CONTINUE);item(f,w,46,1);assert((case_index==2?npe_world_battle_cast_magic(f,w,6,1):choose_defense(w))==NPE_BATTLE_CONTINUE);}
    else if(case_index==4){item(f,w,46,0);item(f,w,159,1);assert(npe_world_battle_previous_actor(w)==1);mark(w);assert(npe_world_battle_attack(w,0)==NPE_BATTLE_CONTINUE);assert(choose_defense(w)==NPE_BATTLE_CONTINUE);}
    else if(case_index==6){item(f,w,46,1);assert(npe_world_battle_select_attack(w,0)==1);mark(w);assert(npe_world_battle_cancel_target(w)==1);mark(w);assert(choose_defense(w)==NPE_BATTLE_CONTINUE);assert(npe_world_battle_attack(w,0)==NPE_BATTLE_CONTINUE);}
    else {item(f,w,159,0);assert(choose_defense(w)==NPE_BATTLE_CONTINUE);}
    assert(phase_index==c->phase_count && w->battle_actor==2);
    if(case_index!=5){w->battle_enemy_hps[0]=w->battle_enemy_hps[2]=0;w->battle_enemy_hps[1]=1;}
    stage(w,&c->before,1);w->music_enabled=0;npe_world_seed(w,30);
    /* Controller list scrolling has already loaded several resources in
      the original. This backend probe starts from that captured cache. */
    w->item_record_id=(uint8_t)c->before_id;memcpy(w->item_record_bytes,c->before_cache,142);
}
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!recording)return;
    const ResidentCase *c=&resident_cases[case_index];assert(frames<c->frames);
    char path[180];snprintf(path,sizeof(path),"tmp/native-battle-item-resident-%s-%u.idx",c->name,frames);
    uint8_t expected[28800];uint32_t rgba[28800];FILE *in=fopen(path,"rb");
    assert(in && fread(expected,1,sizeof(expected),in)==sizeof(expected) && !fclose(in));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned differences=0,sd=0;for(unsigned i=0;i<28800;++i){unsigned v=expected[i];differences+=v!=last_pixel_frame[i];sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Resident %s frame%u C=%u SDL=%u\n",c->name,frames,differences,sd);
    if(differences){FILE *out=fopen("tmp/battle-item-resident-actual.idx","wb");assert(out && fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected) && !fclose(out));}
    assert(!differences && !sd);++frames;++total;
}
static void observe(void *context,const NpeWorld *w,unsigned action,unsigned target,unsigned phase){
    if(case_index==5 && action==NPE_BATTLE_ANIMATION_ITEM){
        unsigned type=phase==NPE_BATTLE_ANIMATION_BEGIN?31:1;
        assert(w->battle_item_ids[0]==159 && w->battle_item_types[0]==type && w->battle_item_type_copies[0]==type);
        if(phase==NPE_BATTLE_ANIMATION_DAMAGE)++conversions;
    }
    play_battle_animation(context,w,action,target,phase);
}
static void finish(const NpeWorld *w){
    const ResidentCase *c=&resident_cases[case_index];assert(w->game_mode==c->after_mode && !w->battle_pending_mask);
    stage(w,&c->after,1);assert(w->item_record_id==c->after_id && !memcmp(w->item_record_bytes,c->cache,142));
    for(unsigned i=0;i<6;++i){unsigned e=i<3?0:i-3,p[11];
        p[0]=i<3?w->hp[i]:w->battle_enemy_hps[e];p[1]=i<3?w->max_hp[i]:w->battle_enemy_max_hps[e];p[2]=i<3?w->qi[i]:w->battle_enemy_qis[e];p[3]=i<3?w->max_qi[i]:w->battle_enemy_max_qis[e];p[4]=i<3?w->attack[i]+w->battle_skill_attack_bonus[i]:w->battle_enemy_attacks[e];p[5]=i<3?w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]:w->battle_enemy_defenses[e];p[6]=i<3?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        for(unsigned j=0;j<4;++j)p[7+j]=(uint16_t)(i<3?w->battle_party_status[i][j]:w->battle_enemy_status[e][j]);
        assert(!memcmp(p,c->records[i],sizeof(p)));
    }
}
int main(int argc,char **argv){
    assert(argc==2 && !SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;NpeFont font={0};uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *win=SDL_CreateWindow("native resident commands",0,0,240,120,0);assert(win);SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(case_index=0;case_index<sizeof(resident_cases)/sizeof(*resident_cases);++case_index){
        NpeWorld w;prepare(&file,&w);BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
        NpeWorld entry=w;for(unsigned i=0;i<3;++i)entry.battle_enemy_hps[i]=1000;
        assert(!npe_render_battle_actors_scene(&file,&entry,display.actors));display.cached=1;display.epoch=w.battle_display_epoch;w.battle_scene_refreshed=1;
#ifdef NPE_RPG_RUNTIME_BIND
        assert(!npe_world_resources_open(&file,&w));
#endif
        w.battle_animation_observer=observe;w.battle_animation_context=&display;frames=0;recording=1;
        assert(choose_defense(&w)==(case_index==5?NPE_BATTLE_CONTINUE:NPE_BATTLE_WON));
        if(case_index==5){w.battle_animation_observer=play_battle_animation;assert(draw_battle_animation_commands(renderer,texture,&file,&w,&font,2)==1);}
        else {NpeStoryRecord story;uint8_t pixels[28800];size_t end=0;assert(!npe_story_record(&file,w.story_event_id-1,&story) && !npe_render_native_story_page(&file,&font,&story,0,&end,pixels) && end==story.text_size);draw_pixels(renderer,texture,pixels);}
        recording=0;assert(frames==resident_cases[case_index].frames);finish(&w);
#ifdef NPE_RPG_RUNTIME_BIND
        npe_rpg_runtime_close(&file);
#endif
        NpeWorld headless;prepare(&file,&headless);assert(choose_defense(&headless)==(case_index==5?NPE_BATTLE_CONTINUE:NPE_BATTLE_WON));finish(&headless);
    }
    assert(conversions==1);npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);SDL_Quit();
    printf("Resident item:7 original paths/%u actual C/SDL LCD commits,%u phase states,independent IDs/type copies,142 bytes,66 fields and headless states passed\n",total,states_checked);return 0;
}
