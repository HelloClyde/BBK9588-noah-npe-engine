#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "friendly_hp_picker_native.h"
static SDL_atomic_t picker_ready;
static unsigned draws,last_target;
static const FriendlyHpPickerCase *picker_case;
static SDL_Renderer *picker_renderer;
static NpeWorld *picker_world;
static void picker_frame(void);
static int picker_wait(SDL_Event *event){picker_frame();SDL_AtomicSet(&picker_ready,1);return SDL_WaitEvent(event);}
static int picker_target(const NpeFile *f,const NpeWorld *w,unsigned target,uint8_t *pixels){last_target=target;return npe_render_battle_party_target(f,w,target,pixels);}
static int picker_update(const NpeFile *f,const NpeWorld *w,unsigned previous,unsigned target,uint8_t *pixels){last_target=target;return npe_update_battle_party_target(f,w,previous,target,pixels);}
static int picker_pointer(const NpeFile *f,const NpeWorld *w,unsigned target,uint8_t *pixels){last_target=target;return npe_draw_battle_party_target_pointer(f,w,target,pixels);}
#define npe_draw_battle_party_target_pointer picker_pointer
#define npe_update_battle_party_target picker_update
#define SDL_WaitEvent picker_wait
#define npe_render_battle_party_target picker_target
#define main npe_app_main
#include "../main.c"
#undef main
#undef npe_draw_battle_party_target_pointer
#undef npe_update_battle_party_target
#undef npe_render_battle_party_target
#undef SDL_WaitEvent
static void picker_records(const NpeWorld *w){
    assert(w->game_mode==3&&!w->battle_lost&&w->battle_actor==0);
    for(unsigned i=0;i<6;++i){unsigned e=i<3?0:i-3;NegativeHpRecord r;
        r.hp=i<3?w->hp[i]:w->battle_enemy_hps[e];r.maximum=i<3?w->max_hp[i]:w->battle_enemy_max_hps[e];
        r.qi=i<3?w->qi[i]:w->battle_enemy_qis[e];r.mental=i<3?w->other_stats[i][0]:w->battle_enemy_spell_stats[e];
        r.attack=i<3?(uint16_t)(w->attack[i]+w->battle_skill_attack_bonus[i]):w->battle_enemy_attacks[e];
        r.defense=i<3?(uint16_t)(w->battle_party_defenses[i]+w->battle_skill_defense_bonus[i]):w->battle_enemy_defenses[e];
        r.level=i<3?w->level[i]:w->battle_enemy_damage_floor_stats[e];
        r.experience=i<3?w->experience[i]:w->battle_enemy_experience_rewards[e];
        r.attack_timer=i<3?w->battle_attack_turns[i]:w->battle_enemy_attack_turns[e];
        r.defense_timer=i<3?w->battle_defense_turns[i]:w->battle_enemy_defense_turns[e];
        memcpy(r.status,i<3?w->battle_party_status[i]:w->battle_enemy_status[e],8);
        if(memcmp(&r,&picker_case->records[i],sizeof(r)))fprintf(stderr,"%s phase%u record%u HP %u/%u QI %u/%u DEF %u/%u\n",
            picker_case->name,draws,i,r.hp,picker_case->records[i].hp,r.qi,picker_case->records[i].qi,r.defense,picker_case->records[i].defense);
        assert(!memcmp(&r,&picker_case->records[i],sizeof(r)));
    }
}
static NpeWorld picker_fixture(const NpeFile *f){
    NpeWorld world,*w=&world;
    assert(!npe_world_init(f,w,9,29,13));
    for(unsigned m=0;m<3;++m){w->party_active[m]=1;w->party_ids[m]=1;w->party_characters[m]=w->party_characters[0];memcpy(w->party_names[m],w->party_names[0],12);memcpy(w->battle_names[m],w->party_names[0],12);w->battle_characters[m]=w->party_characters[0];}
    w->game_mode=3;w->story_event_id=1;assert(npe_world_prepare_scripted_battle(f,w)==1);
    const NegativeHpRecord *ref=picker_case->records;
    for(unsigned m=0;m<3;++m){const NegativeHpRecord *r=&ref[m];
        w->hp[m]=r->hp;w->max_hp[m]=r->maximum;w->qi[m]=r->qi;w->max_qi[m]=60;
        w->other_stats[m][0]=r->mental;w->other_stats[m][1]=999-m;w->other_stats[m][2]=1;
        w->attack[m]=r->attack;w->defense[m]=w->battle_party_defenses[m]=r->defense;
        w->level[m]=(uint8_t)r->level;w->experience[m]=r->experience;
        w->battle_attack_turns[m]=r->attack_timer;
        w->battle_defense_turns[m]=r->defense_timer;
        memcpy(w->battle_party_status[m],r->status,8);
        r=&ref[m+3];w->battle_enemy_hps[m]=r->hp;w->battle_enemy_max_hps[m]=r->maximum;w->battle_enemy_qis[m]=r->qi;
        w->battle_enemy_spell_stats[m]=r->mental;w->battle_enemy_lucks[m]=1;w->battle_enemy_speeds[m]=1;
        w->battle_enemy_attacks[m]=w->battle_enemy_base_attacks[m]=r->attack;
        w->battle_enemy_defenses[m]=w->battle_enemy_base_defenses[m]=r->defense;
        w->battle_enemy_damage_floor_stats[m]=(uint8_t)r->level;w->battle_enemy_experience_rewards[m]=r->experience;
        w->battle_enemy_attack_turns[m]=r->attack_timer;
        w->battle_enemy_defense_turns[m]=r->defense_timer;
        memcpy(w->battle_enemy_status[m],r->status,8);memset(w->battle_enemy_skill_ids[m],0,3);
    }

    w->battle_enemy_ids[0]=w->battle_enemy_ids[1];
    /* Numeric fixtures copied enemy IDs before the resident graphic word
       became authoritative. Keep that word tied to the actual NPE art. */
    for(unsigned m=0;m<3;++m)if(w->battle_enemy_ids[m]){
        NpeEnemy enemy;assert(!npe_enemy(f,w->battle_enemy_ids[m]-1,&enemy));
        unsigned slot=w->battle_record_party_count+m;
        w->battle_record_residue[slot][46]=(uint8_t)(enemy.graphic>>8);
        w->battle_record_residue[slot][47]=(uint8_t)enemy.graphic;
    }
    memset(w->skill_slots[0],0,18);w->skill_slots[0][0]=(uint8_t)picker_case->skill;
    w->battle_scene_refreshed=(uint8_t)picker_case->refreshed;
    /* Refreshed native fixtures completed slot0's defense while both
       other slots were nonpositive. Reconstruct the saved drawing phase
       as well as the numeric records; the C round executor is not invoked
       by this isolated picker fixture. */
    if(picker_case->refreshed)
        for(unsigned m=0;m<3;++m)
            if(m==0||(int16_t)w->hp[m]<=(int16_t)w->max_hp[m]/5)
                w->battle_party_refresh_mask|=(uint8_t)(1u<<m);
    return world;
}
static uint32_t picker_palette(unsigned value){return UINT32_C(0xff000000)|((183-value*142/15)<<16)|((200-value*146/15)<<8)|(161-value*119/15);}
static void picker_frame(void){
    if(draws<5){
        picker_records(picker_world);
        if(draws){
            fprintf(stderr,"Original picker %s phase%u target %u/%u\n",picker_case->name,draws,last_target,picker_case->targets[draws-1]);
            assert(last_target==picker_case->targets[draws-1]);
        }
        if(picker_case->frames){
            char path[160];snprintf(path,sizeof(path),"tmp/native-%s-%u.idx",picker_case->frames,draws+1);
            uint8_t ref[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *in=fopen(path,"rb");assert(in);
            assert(fread(ref,1,sizeof(ref),in)==sizeof(ref)&&fgetc(in)==EOF&&!fclose(in));
            assert(!SDL_RenderReadPixels(picker_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
            unsigned differences=0;for(unsigned i=0;i<sizeof(ref);++i)differences+=rgba[i]!=picker_palette(ref[i]);
            fprintf(stderr,"SDL original picker %s phase%u: %u differences\n",picker_case->name,draws,differences);
            if(differences){FILE *out=fopen("tmp/friendly-hp-picker-c.rgba","wb");assert(out);fwrite(rgba,1,sizeof(rgba),out);fclose(out);}
            assert(!differences);
        }
    }
    ++draws;
}
static int picker_keys(void *unused){
    (void)unused;unsigned waited=0;while(!SDL_AtomicGet(&picker_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&picker_ready))return -1;
    const SDL_Keycode keys[]={SDLK_RETURN,SDLK_UP,SDLK_UP,SDLK_DOWN,SDLK_F4,SDLK_F4};
    for(unsigned i=0;i<sizeof(keys)/sizeof(*keys);++i){
        SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=keys[i];
        SDL_AtomicSet(&picker_ready,0);if(SDL_PushEvent(&event)!=1)return -1;
        SDL_Delay(60);
    }
    return 0;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;NpeFont font={0};uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    SDL_Window *window=SDL_CreateWindow("original friendly target",0,0,240,120,SDL_WINDOW_HIDDEN);assert(window);
    picker_renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(picker_renderer);
    SDL_Texture *texture=SDL_CreateTexture(picker_renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned i=0;i<sizeof(friendly_hp_picker_cases)/sizeof(*friendly_hp_picker_cases);++i){
        picker_case=&friendly_hp_picker_cases[i];NpeWorld world=picker_fixture(&f),before=world;
        picker_world=&world;draws=0;last_target=0;picker_records(&world);
        assert(!npe_world_resources_open(&f,&world));
        SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);SDL_AtomicSet(&picker_ready,0);
        SDL_Thread *thread=SDL_CreateThread(picker_keys,"original picker keys",NULL);assert(thread);
        show_battle_skill(window,picker_renderer,texture,&f,&font,&world);
        int result=-1;SDL_WaitThread(thread,&result);assert(!result&&draws==6);
        /* The native selector preserves character values while updating
           command fields and resident spell/UI caches. The host structure
           as a whole is therefore not an unchanged-state invariant. */
        picker_records(&world);
        for(unsigned m=0;m<3;++m){uint8_t old_record[148],record[148];
            assert(!npe_world_map_record_image(&before,m,old_record));
            assert(!npe_world_map_record_image(&world,m,record));
            assert(!memcmp(old_record,record,sizeof(record)));
        }
        fprintf(stderr,"Original friendly picker %s passed\n",picker_case->name);
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));
    puts("13 original friendly picker sequences, 468 complete record phases and 20 SDL whole frame comparisons passed");
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(picker_renderer);SDL_DestroyWindow(window);SDL_Quit();npe_font_free(&font);npe_free(&f);return 0;
}
