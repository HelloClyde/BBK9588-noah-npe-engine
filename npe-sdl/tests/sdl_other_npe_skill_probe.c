#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#include "other_npe_skill_native.h"
#ifdef main
#undef main
#endif
static unsigned case_index,frames,total,selection_step,selection_pages;
static int recording;
static NpeWorld *live;
static int input(SDL_Event *);
static void present(SDL_Renderer *);
#define SDL_RenderPresent present
#define SDL_WaitEvent input
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
#undef SDL_WaitEvent

static void compare_frame(SDL_Renderer *renderer,const char *path,unsigned index){
    const OtherSkillCase *c=&other_skill_cases[case_index];
    uint8_t expected[28800];uint32_t rgba[28800];FILE *in=fopen(path,"rb");
    assert(in && fread(expected,1,sizeof(expected),in)==sizeof(expected) && !fclose(in));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned differences=0,sd=0;
    for(unsigned i=0;i<28800;++i){unsigned v=expected[i];differences+=v!=last_pixel_frame[i];
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Other NPE skill %s frame%u C=%u SDL=%u\n",c->name,index,differences,sd);
    if(differences){FILE *out=fopen("tmp/other-npe-skill-actual.idx","wb");assert(out && fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected) && !fclose(out));}
    assert(!differences && !sd);
}
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(recording!=1)return;
    const OtherSkillCase *c=&other_skill_cases[case_index];assert(frames<c->frames);
    char path[180];snprintf(path,sizeof(path),"tmp/native-other-npe-skill-%s-%u.idx",c->name,frames);
    compare_frame(renderer,path,frames);++frames;++total;
}
static int input(SDL_Event *event){
    const OtherSkillCase *c=&other_skill_cases[case_index];
    assert(recording==2 && selection_step<=2);
    unsigned gate=2+selection_step;char path[180];
    snprintf(path,sizeof(path),"tmp/native-other-npe-skill-%s-gate-%u.idx",c->name,gate);
    compare_frame(last_pixel_renderer,path,gate);++selection_pages;
    assert(live->battle_actor==0 && live->battle_menu_phase==(selection_step?27:21));
    assert(live->skill_record_id==6 && !memcmp(live->skill_record_bytes,c->cache,144));
    if(selection_step==2){assert(c->reject && !live->battle_pending_mask && live->qi[0]==100);return 0;}
    if(c->fallen && selection_step==1)live->battle_enemy_hps[0]=0;
    ++selection_step;memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;
    event->key.keysym.sym=SDLK_SPACE;return 1;
}
static int choose_defense(NpeWorld *w){w->battle_menu_phase=31;return npe_world_battle_defend(w);}
static void state(const NpeWorld *w,const OtherSkillCase *c){
    assert(w->game_mode==3 && w->battle_actor==0 && !w->battle_pending_mask);
    assert(w->skill_record_id==6 && !memcmp(w->skill_record_bytes,c->cache,144));
    for(unsigned i=0;i<6;++i){unsigned e=i<3?0:i-3,p[11];
        p[0]=i<3?w->hp[i]:w->battle_enemy_hps[e];p[1]=i<3?w->max_hp[i]:w->battle_enemy_max_hps[e];
        p[2]=i<3?w->qi[i]:w->battle_enemy_qis[e];p[3]=i<3?w->max_qi[i]:w->battle_enemy_max_qis[e];
        p[4]=i<3?w->attack[i]+w->battle_skill_attack_bonus[i]:w->battle_enemy_attacks[e];
        p[5]=i<3?w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]:w->battle_enemy_defenses[e];
        p[6]=i<3?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        for(unsigned j=0;j<4;++j)p[7+j]=(uint16_t)(i<3?w->battle_party_status[i][j]:w->battle_enemy_status[e][j]);
        if(memcmp(p,c->records[i],sizeof(p)))fprintf(stderr,"Other skill %s record%u HP=%u QI=%u DEF=%u\n",c->name,i,p[0],p[2],p[5]);
        assert(!memcmp(p,c->records[i],sizeof(p)));
    }
}
int main(int argc,char **argv){
    assert(argc==2 && !SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;NpeFont font={0};uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *win=SDL_CreateWindow("other NPE skill dispatch",0,0,240,120,0);assert(win);
    SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(case_index=0;case_index<sizeof(other_skill_cases)/sizeof(*other_skill_cases);++case_index){
        const OtherSkillCase *c=&other_skill_cases[case_index];NpeWorld w;
        assert(!npe_world_init(&file,&w,9,29,13));w.game_mode=3;w.story_event_id=1;
        for(unsigned i=0;i<3;++i){w.party_active[i]=1;w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];}
        const uint8_t outer[2]={51,51};assert(!npe_world_resident_write(&w,0x527626,outer,2));
        assert(npe_world_prepare_scripted_battle(&file,&w)==1);
        for(unsigned i=0;i<3;++i){
            w.hp[i]=50;w.max_hp[i]=200;w.qi[i]=100;w.max_qi[i]=120;w.level[i]=99;
            w.attack[i]=90;w.defense[i]=w.battle_party_defenses[i]=0;w.other_stats[i][0]=1;w.other_stats[i][1]=(uint16_t)(999-i);w.other_stats[i][2]=1;
            memset(w.battle_party_status[i],0,sizeof(w.battle_party_status[i]));memset(w.skill_slots[i],0,18);w.skill_slots[i][0]=6;
            w.battle_enemy_ids[i]=51;w.battle_enemy_hps[i]=1000;w.battle_enemy_max_hps[i]=2000;
            w.battle_enemy_qis[i]=100;w.battle_enemy_max_qis[i]=120;w.battle_enemy_spell_stats[i]=1;
            w.battle_enemy_attacks[i]=w.battle_enemy_base_attacks[i]=1;w.battle_enemy_defenses[i]=w.battle_enemy_base_defenses[i]=0;
            w.battle_enemy_speeds[i]=w.battle_enemy_lucks[i]=1;w.battle_enemy_damage_floor_stats[i]=99;
            memset(w.battle_enemy_status[i],0,8);w.battle_enemy_status[i][2]=-36;memset(w.battle_enemy_skill_ids[i],0,3);
        }
        /* Check the actual external resource using the production parser. */
        NpeFile source;NpeSkill skill;uint8_t *source_bytes;size_t source_size;
        assert(!read_file(c->source,&source_bytes,&source_size) && !npe_parse(&source,source_bytes,source_size,error,sizeof(error)));
        assert(!npe_skill(&source,c->source_skill-1,&skill));
        assert(!memcmp(skill.raw,c->cache,12) && !memcmp(skill.raw+12,c->cache+114,3));
        assert(skill.required_level==c->cache[115] && skill.qi_cost==((unsigned)c->cache[128]*256+c->cache[129]));
        w.skill_record_id=0;memset(w.skill_record_bytes,0,144);
        assert(!npe_world_skill(&source,&w,c->source_skill,&skill) && !memcmp(w.skill_record_bytes,c->cache,144));
        npe_free(&source);
        w.skill_record_id=6;memcpy(w.skill_record_bytes,c->cache,144);
        assert(!npe_world_resources_open(&file,&w));w.music_enabled=0;
        live=&w;selection_step=0;recording=2;w.battle_menu_phase=20;
        show_battle_skill(win,renderer,texture,&file,&font,&w);recording=0;
        fprintf(stderr,"Selector %s inputs=%u actor=%u menu=%u resources=%u enemyCount=%u\n",c->name,selection_step,w.battle_actor,w.battle_menu_phase,w.battle_resource_flags,w.battle_enemy_count);
        assert(w.battle_magic_types[0]==c->cache[116] && w.battle_magic_groups[0]==c->cache[117] && w.battle_magic_sides[0]==c->cache[118]);
        if(c->reject){assert(w.battle_actor==0 && w.battle_menu_phase==27 && !w.battle_pending_mask && w.qi[0]==100);npe_rpg_runtime_close(&file);continue;}
        assert(w.battle_actor==1 && w.battle_pending_mask==1);
        assert(choose_defense(&w)==NPE_BATTLE_CONTINUE && w.battle_actor==2);
        BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
        if(c->fallen){
            /* Original RAM death occurs after the target page has drawn
               the living enemy. Construct that retained plane in C. */
            w.battle_enemy_hps[0]=1000;
            assert(!npe_render_battle_actors_scene(&file,&w,display.actors));
            w.battle_enemy_hps[0]=0;
            display.cached=display.entry_cached=1;display.epoch=w.battle_display_epoch;
        }
        w.battle_animation_observer=play_battle_animation;w.battle_animation_context=&display;w.music_enabled=0;
        frames=0;recording=1;assert(choose_defense(&w)==NPE_BATTLE_CONTINUE);
        assert(draw_battle_animation_commands(renderer,texture,&file,&w,&font,2)==1);
        recording=0;assert(frames==c->frames);state(&w,c);
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
    }
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);SDL_Quit();
    printf("Other NPE skills:%zu original paths/%u selection pages/%u round C/SDL LCD commits,shared cache and66 record fields passed\n",sizeof(other_skill_cases)/sizeof(*other_skill_cases),selection_pages,total);return 0;
}
