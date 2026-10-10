#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#ifdef NPE_CHARACTER_METADATA_ANIMATION
#include "character_escape_native.h"
#else
#include "flee_animation_native.h"
#endif
static unsigned scenario,frame,capture,calls,map_checked;
static SDL_Renderer *probe_renderer;
static SDL_atomic_t ready,finished;
static int setup(const NpeFile *,NpeWorld *);
static int flee(NpeWorld *);
static void present(SDL_Renderer *);
static SDL_Renderer *create_renderer(SDL_Window *w,int i,Uint32 flags){probe_renderer=SDL_CreateRenderer(w,i,flags);return probe_renderer;}
static SDL_Window *create_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
static int wait_event(SDL_Event *,int);
static void seed(NpeWorld *,uint32_t);
#define SDL_CreateRenderer create_renderer
#define SDL_CreateWindow create_window
#define SDL_RenderPresent present
#define SDL_WaitEventTimeout wait_event
#define npe_world_new_game setup
#define npe_world_restart_game setup
#define npe_world_battle_flee flee
#define npe_world_seed seed
#define main npe_app_main
#include "scene_wait_input.h"
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_CreateRenderer
#undef SDL_CreateWindow
#undef SDL_RenderPresent
#undef SDL_WaitEventTimeout
#undef npe_world_new_game
#undef npe_world_restart_game
#undef npe_world_battle_flee
#undef npe_world_seed
static const FleeAnimationCase *fixture(void){return &flee_animation_cases[scenario];}
static void reference(unsigned index,uint8_t *out){
    char path[160];snprintf(path,sizeof(path),"tmp/native-%s-commit-%u.idx",fixture()->name,index);
    FILE *in=fopen(path,"rb");assert(in);assert(fread(out,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void compare_c(const uint8_t *pixels){
    uint8_t ref[NPE_WIDTH*NPE_HEIGHT];reference(frame,ref);
    unsigned differences=0;for(unsigned i=0;i<sizeof(ref);++i)differences+=pixels[i]!=ref[i];
    fprintf(stderr,"C %s commit%u: %u differences\n",fixture()->name,frame,differences);
    if(differences){FILE *out=fopen("tmp/flee-animation-c.idx","wb");assert(out);fwrite(pixels,1,sizeof(ref),out);fclose(out);}
    assert(!differences);++frame;
}
static void compare_sdl(SDL_Renderer *renderer,unsigned index){
    uint8_t ref[NPE_WIDTH*NPE_HEIGHT];uint32_t pixels[NPE_WIDTH*NPE_HEIGHT];reference(index,ref);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,pixels,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<sizeof(ref);++i){unsigned v=ref[i];differences+=pixels[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL %s commit%u: %u differences\n",fixture()->name,index,differences);assert(!differences);
}
static void present(SDL_Renderer *renderer){SDL_RenderPresent(renderer);if(capture){compare_sdl(renderer,frame);if(++frame==fixture()->frames-1)capture=0;}}
static int wait_event(SDL_Event *event,int timeout){
    if(calls&&!map_checked){assert(!capture&&frame==fixture()->frames-1);compare_sdl(probe_renderer,frame++);map_checked=1;SDL_AtomicSet(&finished,1);}
    SDL_AtomicSet(&ready,1);return SDL_WaitEventTimeout(event,timeout);
}
static void seed(NpeWorld *w,uint32_t unused){(void)unused;npe_world_seed(w,1);}
static int setup(const NpeFile *f,NpeWorld *w){
    assert(!npe_world_init(f,w,9,29,13));w->direction=1;w->story_progress=6;
    for(unsigned i=0;i<3;++i){w->party_active[i]=i<fixture()->party;w->party_ids[i]=fixture()->character;w->hp[i]=fixture()->before[i].hp;w->max_hp[i]=30000;
        assert(!npe_character_metadata(f,w->party_ids[i]-1,&w->party_characters[i]));{NpeCharacter c;assert(!npe_character(f,w->party_ids[i]-1,&c));memcpy(w->party_names[i],c.name,12);memcpy(w->battle_names[i],c.name,12);}}
    w->game_mode=3;w->story_event_id=1;assert(npe_world_prepare_scripted_battle(f,w)==1);w->story_event_id=0;
#ifdef NPE_CHARACTER_METADATA_ANIMATION
    for(unsigned i=0;i<3;++i)w->battle_characters[i].animation_ids[0]=(uint8_t)(fixture()->character==1?11:10);
#endif
    for(unsigned i=0;i<fixture()->party;++i){const NegativeHpRecord *r=&fixture()->before[i];
        w->hp[i]=r->hp;w->max_hp[i]=r->maximum;w->qi[i]=r->qi;w->max_qi[i]=999;
        w->attack[i]=r->attack;w->defense[i]=w->battle_party_defenses[i]=r->defense;w->level[i]=r->level;w->experience[i]=r->experience;
        w->other_stats[i][0]=r->mental;w->other_stats[i][1]=999-i;w->other_stats[i][2]=1;memcpy(w->battle_party_status[i],r->status,8);
    }
    for(unsigned i=0;i<3;++i){const NegativeHpRecord *r=&fixture()->before[fixture()->party+i];
        w->battle_enemy_hps[i]=r->hp;w->battle_enemy_max_hps[i]=r->maximum;w->battle_enemy_ids[i]=i==1?51:0;
        w->battle_enemy_qis[i]=r->qi;w->battle_enemy_spell_stats[i]=r->mental;w->battle_enemy_attacks[i]=w->battle_enemy_base_attacks[i]=r->attack;
        w->battle_enemy_defenses[i]=w->battle_enemy_base_defenses[i]=r->defense;w->battle_enemy_speeds[i]=w->battle_enemy_lucks[i]=1;
        w->battle_enemy_damage_floor_stats[i]=r->level;w->battle_enemy_experience_rewards[i]=r->experience;memcpy(w->battle_enemy_status[i],r->status,8);
    }
    w->battle_actor=0;w->condition_words[159]=1;seed(w,0);return 0;
}
static void verify_records(const NpeWorld *w){
    for(unsigned i=0;i<fixture()->party+3;++i){unsigned e=i<fixture()->party?0:i-fixture()->party;NegativeHpRecord r={0};
        r.hp=i<fixture()->party?w->hp[i]:w->battle_enemy_hps[e];r.maximum=i<fixture()->party?w->max_hp[i]:w->battle_enemy_max_hps[e];
        r.qi=i<fixture()->party?w->qi[i]:w->battle_enemy_qis[e];r.mental=i<fixture()->party?w->other_stats[i][0]:w->battle_enemy_spell_stats[e];
        r.attack=i<fixture()->party?w->attack[i]:w->battle_enemy_attacks[e];r.defense=i<fixture()->party?w->battle_party_defenses[i]:w->battle_enemy_defenses[e];
        r.level=i<fixture()->party?w->level[i]:w->battle_enemy_damage_floor_stats[e];r.experience=i<fixture()->party?w->experience[i]:w->battle_enemy_experience_rewards[e];
        r.attack_timer=i<fixture()->party?w->battle_attack_turns[i]:w->battle_enemy_attack_turns[e];r.defense_timer=i<fixture()->party?w->battle_defense_turns[i]:w->battle_enemy_defense_turns[e];
        memcpy(r.status,i<fixture()->party?w->battle_party_status[i]:w->battle_enemy_status[e],8);assert(!memcmp(&r,&fixture()->after[i],sizeof(r)));
    }
    assert(w->game_mode==1&&!w->battle_enemy_count&&w->condition_words[159]==1);
}
static int flee(NpeWorld *w){capture=1;frame=0;uint32_t money=w->money;seed(w,0);assert(npe_world_battle_flee(w)==NPE_BATTLE_FLED);assert(w->money==money);verify_records(w);++calls;return NPE_BATTLE_FLED;}
static void c_observer(void *unused,const NpeWorld *w,unsigned action,unsigned target,unsigned phase){
    (void)unused;(void)target;assert(action==NPE_BATTLE_ANIMATION_FLEE&&phase==NPE_BATTLE_ANIMATION_BEGIN);
    static const int x[3][3]={{168,0,0},{136,168,0},{136,168,200}},y[3][3]={{51,0,0},{75,51,0},{75,51,28}};
    uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[NPE_WIDTH*NPE_HEIGHT];unsigned position=0;
    assert(!npe_render_battle_actors_scene(w->battle_file,w,base));memcpy(pixels,base,sizeof(pixels));
    for(unsigned i=0;i<3;++i)if(w->party_active[i]){unsigned compact=position++;if((int16_t)w->hp[i]<=0)continue;
        NpeCharacter character;assert(!npe_character(w->battle_file,w->party_ids[i]-1,&character));assert(!npe_render_battle_flee_stage(w->battle_file,w,i,base));compare_c(base);
        unsigned group=npe_world_battle_character(w,i)->animation_ids[0],frames=group?npe_animation_frames(w->battle_file,group-1):0;if(frames)compare_c(base);
        for(unsigned f=0;f<frames;++f){memcpy(pixels,base,sizeof(pixels));assert(!npe_render_battle_animation_frame(w->battle_file,group-1,f,x[fixture()->party-1][compact]-15,y[fixture()->party-1][compact],pixels));compare_c(pixels);if(f+1<frames)compare_c(base);}
        if(!frames)memcpy(pixels,base,sizeof(pixels));
    }
    assert(!npe_render_battle_reward_panel(w->battle_file,0,0,pixels));compare_c(pixels);compare_c(pixels);
}
static int keys(void *unused){(void)unused;const SDL_Keycode codes[]={SDLK_RETURN,SDLK_LEFT,SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN};
    for(unsigned i=0;!SDL_AtomicGet(&ready)&&i<500;++i)SDL_Delay(10);
    assert(SDL_AtomicGet(&ready));
    for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);++i){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=codes[i];assert(SDL_PushEvent(&e)==1);SDL_Delay(70);}
    for(unsigned i=0;!SDL_AtomicGet(&finished)&&i<500;++i)SDL_Delay(10);
    assert(SDL_AtomicGet(&finished));SDL_Event e={0};e.type=SDL_QUIT;return SDL_PushEvent(&e)==1?0:-1;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    unsigned total=0;
    for(scenario=0;scenario<sizeof(flee_animation_cases)/sizeof(*flee_animation_cases);++scenario){
        NpeWorld w;assert(!setup(&f,&w));frame=0;
        w.battle_animation_observer=c_observer;w.battle_animation_context=NULL;
        assert(npe_world_battle_flee(&w)==NPE_BATTLE_FLED);verify_records(&w);
        uint8_t map[NPE_WIDTH*NPE_HEIGHT];assert(!npe_render_world(&f,&w,map));compare_c(map);assert(frame==fixture()->frames);
        calls=map_checked=capture=0;SDL_AtomicSet(&ready,0);SDL_AtomicSet(&finished,0);SDL_Thread *t=SDL_CreateThread(keys,"native escape keys",NULL);assert(t);
        char *args[]={"npe-sdl",argv[1],NULL};assert(!npe_app_main(2,args));int result=-1;SDL_WaitThread(t,&result);assert(!result&&calls==1&&map_checked&&frame==fixture()->frames);total+=frame;
    }
    assert(npe_game_fingerprint(&f)==UINT64_C(0x114861a601117d94));npe_free(&f);
    printf("%u escape fixtures and %u C/actual SDL commits including map returns passed\n",(unsigned)(sizeof(flee_animation_cases)/sizeof(*flee_animation_cases)),total);return 0;
}
