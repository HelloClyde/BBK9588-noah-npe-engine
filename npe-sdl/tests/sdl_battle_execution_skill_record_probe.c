#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#ifdef NPE_PREPARED_RECORDS
#include "battle_prepared_skill_native.h"
#define EXEC_FRAME_PREFIX "tmp/native-battle-prepared-skill-record"
#else
#include "battle_execution_skill_native.h"
#define EXEC_FRAME_PREFIX "tmp/native-battle-execution-skill-record"
#endif
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
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!recording)return;
    const ExecSkillCase *c=&exec_skill_cases[case_index];assert(frames<c->frames);
    char path[180];snprintf(path,sizeof(path),EXEC_FRAME_PREFIX "-%s-%u.idx",c->name,frames);
    uint8_t expected[28800];uint32_t rgba[28800];FILE *in=fopen(path,"rb");
    assert(in && fread(expected,1,sizeof(expected),in)==sizeof(expected) && !fclose(in));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned differences=0,sd=0;
    for(unsigned i=0;i<28800;++i){unsigned v=expected[i];differences+=v!=last_pixel_frame[i];
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Execution skill %s frame%u C=%u SDL=%u\n",c->name,frames,differences,sd);
    if(differences){FILE *out=fopen("tmp/battle-execution-skill-record-actual.idx","wb");assert(out && fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected) && !fclose(out));}
    assert(!differences && !sd);
#ifdef NPE_PREPARED_RECORDS
    snprintf(path,sizeof(path),EXEC_FRAME_PREFIX "-%s-%u-c.idx",c->name,frames);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected)&&!fclose(out));
#endif
    ++frames;++total;
}
static void state(const NpeWorld *w,const ExecSkillCase *c){
    assert(w->game_mode==3 && w->battle_actor==0 && !w->battle_pending_mask);
    assert(w->skill_record_id==c->after_id && !memcmp(w->skill_record_bytes,c->after,144));
    for(unsigned i=0;i<3;++i)assert(w->battle_skill_ids[i]==c->skill_ids[i]);
    for(unsigned i=0;i<6;++i){unsigned e=i<3?0:i-3,p[11];
        p[0]=i<3?w->hp[i]:w->battle_enemy_hps[e];p[1]=i<3?w->max_hp[i]:w->battle_enemy_max_hps[e];
        p[2]=i<3?w->qi[i]:w->battle_enemy_qis[e];p[3]=i<3?w->max_qi[i]:w->battle_enemy_max_qis[e];
        p[4]=i<3?w->attack[i]+w->battle_skill_attack_bonus[i]:w->battle_enemy_attacks[e];
        p[5]=i<3?w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]:w->battle_enemy_defenses[e];
        p[6]=i<3?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        for(unsigned j=0;j<4;++j)p[7+j]=(uint16_t)(i<3?w->battle_party_status[i][j]:w->battle_enemy_status[e][j]);
        if(memcmp(p,c->records[i],sizeof(p))){fprintf(stderr,"Execution %s record%u HP=%u QI=%u DEF=%u\n",c->name,i,p[0],p[2],p[5]);}
        assert(!memcmp(p,c->records[i],sizeof(p)));
#ifdef NPE_PREPARED_RECORDS
        uint8_t record[148];assert(!npe_world_battle_record_image(w,i,record));
        for(unsigned j=0;j<12;++j)if(record[102+j]!=c->prepared_after[i][j])
            fprintf(stderr,"Prepared skill %s record%u byte%u C=%02x original=%02x\n",c->name,i,102+j,record[102+j],c->prepared_after[i][j]);
        char path[180];snprintf(path,sizeof(path),EXEC_FRAME_PREFIX "-%s-record-%u-c.bin",c->name,i);
        FILE *out=fopen(path,"wb");assert(out&&fwrite(record+102,1,12,out)==12&&!fclose(out));
        assert(!memcmp(record+102,c->prepared_after[i],12));
#endif
    }
}
/* The original input selects defense from resident menu31. */
static int choose_defense(NpeWorld *w){
    w->battle_menu_phase=31;return npe_world_battle_defend(w);
}
int main(int argc,char **argv){
    assert(argc==2 && !SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    NpeFile file;NpeFont font={0};uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *win=SDL_CreateWindow("native execution cache",0,0,240,120,0);assert(win);
    SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(case_index=0;case_index<sizeof(exec_skill_cases)/sizeof(*exec_skill_cases);++case_index){
        const ExecSkillCase *c=&exec_skill_cases[case_index];NpeWorld w;
        assert(!npe_world_init(&file,&w,9,29,13));w.game_mode=3;w.story_event_id=1;
        for(unsigned i=0;i<3;++i){w.party_active[i]=1;w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];}
        const uint8_t outer[2]={51,51};assert(!npe_world_resident_write(&w,0x527626,outer,2));
        assert(npe_world_prepare_scripted_battle(&file,&w)==1);
        for(unsigned i=0;i<3;++i){
            w.hp[i]=50;w.max_hp[i]=200;w.qi[i]=(uint16_t)(i?100:c->qi);w.max_qi[i]=120;w.level[i]=99;
            w.attack[i]=90;w.defense[i]=w.battle_party_defenses[i]=0;w.other_stats[i][0]=1;w.other_stats[i][1]=(uint16_t)(999-i);w.other_stats[i][2]=1;
            memset(w.battle_party_status[i],0,sizeof(w.battle_party_status[i]));memset(w.skill_slots[i],0,18);w.skill_slots[i][0]=(uint8_t)(i==1&&c->second?c->second:6);
            w.battle_enemy_ids[i]=51;w.battle_enemy_hps[i]=1000;w.battle_enemy_max_hps[i]=2000;
            w.battle_enemy_qis[i]=100;w.battle_enemy_max_qis[i]=120;w.battle_enemy_spell_stats[i]=1;
            w.battle_enemy_attacks[i]=w.battle_enemy_base_attacks[i]=1;w.battle_enemy_defenses[i]=w.battle_enemy_base_defenses[i]=0;
            w.battle_enemy_speeds[i]=w.battle_enemy_lucks[i]=1;w.battle_enemy_damage_floor_stats[i]=99;
            memset(w.battle_enemy_status[i],0,8);w.battle_enemy_status[i][2]=-36;memset(w.battle_enemy_skill_ids[i],0,3);
        }
        assert(npe_world_battle_cast_magic(&file,&w,6,0)==NPE_BATTLE_CONTINUE && w.battle_actor==1);
        unsigned second_target=c->second==6?1:0;
#ifdef NPE_PREPARED_RECORDS
        if(c->second==4||c->second==20||c->second==9)second_target=1;
#endif
        assert((c->second?npe_world_battle_cast_magic(&file,&w,c->second,second_target):choose_defense(&w))==NPE_BATTLE_CONTINUE && w.battle_actor==2);
        w.skill_record_id=(uint8_t)c->cached;memcpy(w.skill_record_bytes,c->before,144);
        if(c->forget){memset(w.skill_slots[0],0,18);w.level[0]=0;}
#ifdef NPE_PREPARED_RECORDS
        for(unsigned i=0;i<6;++i)assert(!npe_world_record_write(&w,1,i,102,c->prepared_before[i],12));
#endif
        BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
        w.battle_animation_observer=play_battle_animation;w.battle_animation_context=&display;w.music_enabled=0;
        frames=0;recording=1;assert(choose_defense(&w)==NPE_BATTLE_CONTINUE);
        assert(draw_battle_animation_commands(renderer,texture,&file,&w,&font,2)==1);
        recording=0;assert(frames==c->frames);state(&w,c);
        uint8_t raw[42],held[144];NpeSkill view;memcpy(held,w.skill_record_bytes,144);
        assert(!npe_world_skill_view(&w,raw,&view) && !memcmp(held,w.skill_record_bytes,144));
    }
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);SDL_Quit();
    printf(
#ifdef NPE_PREPARED_RECORDS
        "Prepared skill record:"
#else
        "Execution skill record:"
#endif
        "%zu original rounds/%u actual C/SDL LCD commits,144 bytes,66 record fields,queued flags and QI fee order passed\n",sizeof(exec_skill_cases)/sizeof(*exec_skill_cases),total);return 0;
}
