#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#include "character_attack_group_native.h"
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

static void prepare(const NpeFile *f,NpeWorld *w){
    assert(!npe_world_init(f,w,9,29,13));w->game_mode=3;w->story_event_id=1;
    for(unsigned i=0;i<3;++i){w->party_active[i]=1;w->party_ids[i]=1;w->party_characters[i]=w->party_characters[0];memcpy(w->party_names[i],w->party_names[0],12);memcpy(w->battle_names[i],w->party_names[0],12);}
    assert(npe_world_prepare_scripted_battle(f,w)==1);
    for(unsigned i=0;i<3;++i){
        NpeCharacter character;assert(!npe_character(f,w->party_ids[i]-1,&character));
        assert(w->battle_attack_groups[i]==character.raw[79]);
        w->hp[i]=50;w->max_hp[i]=200;w->qi[i]=100;w->max_qi[i]=120;w->level[i]=99;
        w->attack[i]=90;w->defense[i]=w->battle_party_defenses[i]=0;w->experience[i]=0;w->experience_to_next[i]=10;
        w->other_stats[i][0]=1;w->other_stats[i][1]=(uint16_t)(999-i);w->other_stats[i][2]=1;
        memset(w->battle_party_status[i],0,8);memset(w->equipped_items[i],0,6);
        memset(w->skill_slots[i],0,18);w->skill_slots[i][0]=6;w->skill_slots[i][1]=4;
        w->battle_enemy_ids[i]=51;w->battle_enemy_hps[i]=1000;w->battle_enemy_max_hps[i]=2000;
        w->battle_enemy_qis[i]=100;w->battle_enemy_max_qis[i]=120;w->battle_enemy_spell_stats[i]=1;
        w->battle_enemy_attacks[i]=w->battle_enemy_base_attacks[i]=1;
        w->battle_enemy_defenses[i]=(uint16_t)(case_index==3?(i==1?15:i==2?80:0):0);
        w->battle_enemy_base_defenses[i]=0; /* Native injection changes64B2, not the base word. */
        w->battle_enemy_speeds[i]=w->battle_enemy_lucks[i]=1;w->battle_enemy_damage_floor_stats[i]=99;
        w->battle_enemy_gold_rewards[i]=w->battle_enemy_experience_rewards[i]=0;
        memset(w->battle_enemy_drop_ids[i],0,2);memset(w->battle_enemy_status[i],0,8);
        w->battle_enemy_status[i][2]=-36;memset(w->battle_enemy_skill_ids[i],0,3);
    }
    memset(w->condition_words,0,sizeof(w->condition_words));w->condition_words[46]=w->condition_words[159]=1;
    w->battle_attack_groups[0]=(uint8_t)character_group_cases[case_index].group;
    /* Isolated fixture reproduces native variance0,1,2; cold RNG remains pending. */
    w->music_enabled=0;npe_world_seed(w,90);
}
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!recording)return;
    const CharacterGroupCase *c=&character_group_cases[case_index];assert(frames<c->frames);
    char path[160];snprintf(path,sizeof(path),"tmp/native-character-attack-group-%s-%u.idx",c->name,frames);
    uint8_t expected[28800];uint32_t rgba[28800];FILE *in=fopen(path,"rb");
    assert(in && fread(expected,1,sizeof(expected),in)==sizeof(expected) && !fclose(in));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned diff=0,sd=0;
    for(unsigned i=0;i<28800;++i){unsigned v=expected[i];diff+=v!=last_pixel_frame[i];
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Character %s frame%u C=%u SDL=%u\n",c->name,frames,diff,sd);
    if(diff){snprintf(path,sizeof(path),"tmp/character-attack-group-%s-%u-actual.idx",c->name,frames);
        FILE *out=fopen(path,"wb");assert(out && fwrite(last_pixel_frame,1,28800,out)==28800 && !fclose(out));}
    assert(!diff && !sd);++frames;++total;
}
static void check(const NpeWorld *w){
    const CharacterGroupCase *c=&character_group_cases[case_index];
    assert(w->game_mode==3 && !w->battle_pending_mask && w->battle_attack_groups[0]==c->group);
    assert(w->battle_item_groups[0]==(c->surround?0:c->group));
    assert(w->battle_native_targets[0]==4 && !w->battle_item_types[0] &&
        w->battle_item_type_copies[0]==1 && !w->battle_item_sides[0]);
    for(unsigned i=0;i<6;++i){unsigned e=i<3?0:i-3,p[11];
        p[0]=i<3?w->hp[i]:w->battle_enemy_hps[e];p[1]=i<3?w->max_hp[i]:w->battle_enemy_max_hps[e];
        p[2]=i<3?w->qi[i]:w->battle_enemy_qis[e];p[3]=i<3?w->max_qi[i]:w->battle_enemy_max_qis[e];
        p[4]=i<3?w->attack[i]+w->battle_skill_attack_bonus[i]:w->battle_enemy_attacks[e];
        p[5]=i<3?w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]:w->battle_enemy_defenses[e];
        p[6]=i<3?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        for(unsigned j=0;j<4;++j)p[7+j]=(uint16_t)(i<3?w->battle_party_status[i][j]:w->battle_enemy_status[e][j]);
        if(memcmp(p,c->records[i],sizeof(p)))fprintf(stderr,"%s record%u HP%u/%u DEF%u/%u\n",c->name,i,p[0],c->records[i][0],p[5],c->records[i][5]);
        assert(!memcmp(p,c->records[i],sizeof(p)));
    }
}
int main(int argc,char **argv){
    assert(argc==2 && !SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;NpeFont font={0};uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *win=SDL_CreateWindow("ordinary character groups",0,0,240,120,0);assert(win);
    SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(case_index=0;case_index<sizeof(character_group_cases)/sizeof(*character_group_cases);++case_index){
        for(unsigned headless=0;headless<2;++headless){
            NpeWorld w;prepare(&file,&w);BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
            assert(!npe_render_battle_actors_scene(&file,&w,display.actors));display.cached=1;display.epoch=w.battle_display_epoch;w.battle_scene_refreshed=1;
            if(!headless){w.battle_animation_observer=play_battle_animation;w.battle_animation_context=&display;}
            frames=0;recording=0;assert(npe_world_battle_select_attack(&w,0)==1);
            assert(w.battle_item_groups[0]==character_group_cases[case_index].group);
            assert(npe_world_battle_attack(&w,0)==NPE_BATTLE_CONTINUE);
            if(character_group_cases[case_index].surround){recording=!headless;
                assert(npe_world_battle_group_attack(&w)==NPE_BATTLE_CONTINUE);
                for(unsigned i=0;i<3;++i)assert(!w.battle_item_groups[i] && !w.battle_item_sides[i] &&
                    !w.battle_magic_types[i] && !w.battle_item_types[i] && w.battle_item_type_copies[i]==1);
            }else{
                assert(npe_world_battle_defend(&w)==NPE_BATTLE_CONTINUE);recording=!headless;
                assert(npe_world_battle_defend(&w)==NPE_BATTLE_CONTINUE);
            }
            if(!headless)assert(draw_battle_animation_commands(renderer,texture,&file,&w,&font,2)==1);
            recording=0;check(&w);if(!headless)assert(frames==character_group_cases[case_index].frames);
        }
    }
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);SDL_Quit();
    printf("Character attack groups:5 original paths,%u actual C/SDL LCD commits,66 fields each and headless results passed\n",total);return 0;
}
