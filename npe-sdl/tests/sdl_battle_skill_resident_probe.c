#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#include "battle_skill_resident_native.h"
#ifdef main
#undef main
#endif
static unsigned case_index,frames,total,phase_index,states_checked;
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
    for(unsigned i=0;i<3;++i){unsigned p[]={w->battle_native_kinds[i],w->battle_item_ids[i],w->battle_skill_ids[i],w->battle_magic_types[i],w->battle_item_types[i],w->battle_item_type_copies[i],w->battle_item_groups[i],w->battle_item_sides[i],w->battle_native_targets[i]};
        for(unsigned j=include_kind?0:1;j<9;++j){
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
static void prepare(const NpeFile *f,NpeWorld *w){
    const ResidentCase *c=&resident_cases[case_index];assert(!npe_world_init(f,w,9,29,13));w->game_mode=3;w->story_event_id=1;
    for(unsigned i=0;i<3;++i){w->party_active[i]=1;w->party_ids[i]=1;w->party_characters[i]=w->party_characters[0];memcpy(w->party_names[i],w->party_names[0],12);memcpy(w->battle_names[i],w->party_names[0],12);w->battle_characters[i]=w->party_characters[0];}
    assert(npe_world_prepare_scripted_battle(f,w)==1);
    for(unsigned i=0;i<3;++i){
        w->hp[i]=50;w->max_hp[i]=200;w->qi[i]=100;w->max_qi[i]=120;w->level[i]=99;
        w->attack[i]=90;w->defense[i]=w->battle_party_defenses[i]=0;w->experience[i]=0;w->experience_to_next[i]=10;
        w->other_stats[i][0]=1;w->other_stats[i][1]=(uint16_t)(999-i);w->other_stats[i][2]=1;
        memset(w->battle_party_status[i],0,8);memset(w->equipped_items[i],0,6);memset(w->skill_slots[i],0,18);w->skill_slots[i][0]=6;w->skill_slots[i][1]=4;
        /* The original reference also patches map69DE/69DF after entry. */
        const uint8_t map_level=99;
        assert(!npe_world_record_write(w,0,i,24,&map_level,1));
        assert(!npe_world_record_write(w,0,i,25,w->skill_slots[i],18));
        w->battle_enemy_ids[i]=51;w->battle_enemy_hps[i]=1000;w->battle_enemy_max_hps[i]=2000;
        /* The native fixture replaces all three enemy records. Drawing
           consumes the physical image word rather than the lookup ID. */
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
    w->music_enabled=0;npe_world_seed(w,30);
}

static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!recording)return;
    const ResidentCase *c=&resident_cases[case_index];assert(frames<c->frames);
    char path[180];snprintf(path,sizeof(path),"tmp/native-battle-skill-resident-%s-%u.idx",c->name,frames);
    uint8_t expected[28800];uint32_t rgba[28800];FILE *in=fopen(path,"rb");
    assert(in && fread(expected,1,sizeof(expected),in)==sizeof(expected) && !fclose(in));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned differences=0,sd=0;for(unsigned i=0;i<28800;++i){unsigned v=expected[i];differences+=v!=last_pixel_frame[i];sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Resident %s frame%u C=%u SDL=%u\n",c->name,frames,differences,sd);
    if(differences){FILE *out=fopen("tmp/battle-skill-resident-actual.idx","wb");assert(out && fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected) && !fclose(out));}
    assert(!differences && !sd);++frames;++total;
}
static void finish(const NpeWorld *w,unsigned round){
    const RetainedRound *c=&resident_cases[case_index].rounds[round];assert(w->game_mode==3 && !w->battle_pending_mask);
    stage(w,&c->after,1);assert(w->skill_record_id==c->cache_id && !memcmp(w->skill_record_bytes,c->cache,144));
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
        for(unsigned headless=0;headless<2;++headless){
            NpeWorld w;prepare(&file,&w);BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
            assert(!npe_render_battle_actors_scene(&file,&w,display.actors));display.cached=1;display.epoch=w.battle_display_epoch;w.battle_scene_refreshed=1;
            if(!headless){w.battle_animation_observer=play_battle_animation;w.battle_animation_context=&display;}
            frames=0;recording=0;
            for(unsigned round=0;round<2;++round){
                if(!round){
                    /* Actual list entry scans both known resources before
                      selecting6; the shorter description retains4's tail. */
                    unsigned ids[18];assert(npe_world_available_cached_skills(&file,&w,0,ids)==2);
                    assert(npe_world_battle_select_magic(&file,&w,6)==1);mark(&w);
                    if(case_index){mark(&w);assert(npe_world_battle_cancel_target(&w)==1);mark(&w);}
                    if(case_index==0)assert(npe_world_battle_cast_magic(&file,&w,6,0)==NPE_BATTLE_CONTINUE);
                    else if(case_index==1)assert(choose_defense(&w)==NPE_BATTLE_CONTINUE);
                    else if(case_index==2){
                        assert(!npe_world_battle_reserve_throw_item(&file,&w,159));mark(&w);
                        assert(npe_world_battle_throw_item(&file,&w,159,0)==NPE_BATTLE_CONTINUE);
                    }else{
                        assert(npe_world_available_cached_skills(&file,&w,0,ids)==2);
                        assert(npe_world_battle_select_magic(&file,&w,4)==1);mark(&w);
                        assert(npe_world_battle_cast_magic(&file,&w,4,0)==NPE_BATTLE_CONTINUE);
                        assert(npe_world_battle_previous_actor(&w)==1);mark(&w);
                        assert(choose_defense(&w)==NPE_BATTLE_CONTINUE);mark(&w);
                    }
                }else {assert(choose_defense(&w)==NPE_BATTLE_CONTINUE);mark(&w);}
                assert(choose_defense(&w)==NPE_BATTLE_CONTINUE);
                if(round)mark(&w);
                stage(&w,&resident_cases[case_index].rounds[round].before,1);
                unsigned start=frames;recording=!headless;
                assert(choose_defense(&w)==NPE_BATTLE_CONTINUE);
                if(!headless)assert(draw_battle_animation_commands(renderer,texture,&file,&w,&font,2)==1);
                recording=0;
                if(!headless)assert(frames-start==resident_cases[case_index].rounds[round].frames);
                finish(&w,round);if(!round)mark(&w);
            }
            assert(phase_index==resident_cases[case_index].phase_count);
            if(!headless)assert(frames==resident_cases[case_index].frames);
        }
    }
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);SDL_Quit();
    printf("Resident skill:4 original paths/8 rounds/%u actual C/SDL LCD commits,%u state checks,independent skill IDs,64D0,144 cache bytes,66 record fields and headless states passed\n",total,states_checked);return 0;
}
