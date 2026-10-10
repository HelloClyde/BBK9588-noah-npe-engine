#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned hp,maximum,qi,max_qi,attack,defense,speed,status[4];} ScriptEntryRecord;
typedef struct {const char *name;unsigned party,hps[3],enemies[3];ScriptEntryRecord records[6];
#ifdef NPE_BATTLE_EVENT_RESIDENT
    unsigned event,win_mode,win_event,lose_mode,lose_event;
    uint8_t battle_records[32],monster_records[56];unsigned alias_address;uint8_t alias[8];
#ifdef NPE_BATTLE_FALLBACK
    unsigned enemy_count,context,seed,after_rng;uint8_t random[8],chase[28];unsigned chase_enemy;
#endif
#endif
#ifdef NPE_BATTLE_PARTY_MAPPING
    unsigned flags[3],order[3],targets[3];
#endif
} ScriptEntryCase;
#ifdef NPE_BATTLE_ZERO_EVENT
#include "battle_zero_event_native.h"
#elif defined(NPE_BATTLE_FALLBACK)
#include "battle_fallback_entry_native.h"
#elif defined(NPE_BATTLE_EVENT_RESIDENT)
#include "battle_event_resident_entry_native.h"
#elif defined(NPE_BATTLE_PARTY_MAPPING)
#include "battle_party_mapping_native.h"
#else
#include "scripted_battle_entry_native.h"
#endif
static unsigned scenario,stage,main_commits;
static int active;
static NpeWorld *live;
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static int wait_outer(SDL_Event *,int);
static void present_probe(SDL_Renderer *);
static Uint32 ticks(void){return 1234;}
static SDL_Window *create_window(const char *s,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
#define SDL_GetTicks ticks
#define SDL_WaitEventTimeout wait_outer
#define SDL_CreateWindow create_window
#define npe_sound_select select_sound
#define SDL_RenderPresent present_probe
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_GetTicks
#undef SDL_WaitEventTimeout
#undef SDL_CreateWindow
#undef npe_sound_select
#undef SDL_RenderPresent
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;if(stage>=1)w->music_enabled=0;npe_sound_select(p,w);}
static void reference(unsigned index,unsigned commit,uint8_t *pixels){
    char path[160];
#ifdef NPE_BATTLE_ZERO_EVENT
    snprintf(path,sizeof(path),"tmp/native-battle-zero-event-%u-commit-%u.idx",index,commit);
#elif defined(NPE_BATTLE_FALLBACK)
    snprintf(path,sizeof(path),"tmp/native-battle-fallback-entry-%u-commit-%u.idx",index,commit);
#elif defined(NPE_BATTLE_EVENT_RESIDENT)
    snprintf(path,sizeof(path),"tmp/native-battle-event-resident-entry-%u-commit-%u.idx",index,commit);
#elif defined(NPE_BATTLE_PARTY_MAPPING)
    snprintf(path,sizeof(path),"tmp/native-battle-party-mapping-%s-commit-%u.idx",script_cases[index].name,commit);
#else
    snprintf(path,sizeof(path),"tmp/native-scripted-entry-%u-commit-%u.idx",index,commit);
#endif
    FILE *f=fopen(path,"rb");assert(f&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT);
    assert(fgetc(f)==EOF&&!fclose(f));
}
static void check(const NpeWorld *w,unsigned index){
    const ScriptEntryCase *c=&script_cases[index];
    fprintf(stderr,"%s entry HP=%u actor=%u\n",c->name,w->hp[0],w->battle_actor);
#ifdef NPE_BATTLE_ZERO_EVENT
    const unsigned expected_target=0;
#else
    const unsigned expected_target=1;
#endif
    assert(w->game_mode==3&&w->map_number==9&&w->story_progress==6&&
           w->battle_actor==0&&w->battle_target==expected_target);
#ifdef NPE_BATTLE_FALLBACK
    assert(w->battle_enemy_count==c->enemy_count&&w->event_context_id==c->context&&w->random_state==c->after_rng);
    uint8_t random[8],chase[28];
    assert(!npe_world_resident_read(w,0x527916,random,8)&&!memcmp(random,c->random,8));
    assert(!npe_rpg_runtime_read(w->battle_file,0x52783a,chase,28)&&!memcmp(chase,c->chase,28));
#else
    assert(w->battle_enemy_count==3);
#endif
#ifdef NPE_BATTLE_EVENT_RESIDENT
    assert(w->story_event_id==c->event);
#ifdef NPE_BATTLE_ZERO_EVENT
    assert(!w->battle_scripted_entry&&!w->battle_win_mode&&!w->battle_win_event_id&&!w->battle_lose_mode&&!w->battle_lose_event_id);
#else
    assert(w->battle_win_mode==c->win_mode&&w->battle_win_event_id==c->win_event&&w->battle_lose_mode==c->lose_mode&&w->battle_lose_event_id==c->lose_event);
#endif
    uint8_t records[32],monster[56],alias[8];
    assert(!npe_world_resident_read(w,0x5277fc,records,32)&&!memcmp(records,c->battle_records,32));
    assert(!npe_world_resident_read(w,0x52761a,monster,56)&&!memcmp(monster,c->monster_records,56));
    assert(!npe_rpg_runtime_read(w->battle_file,0x520000+c->alias_address,alias,8)&&!memcmp(alias,c->alias,8));
#else
    assert(w->story_event_id==1);
#endif
#ifdef NPE_BATTLE_PARTY_MAPPING
    assert(w->battle_party_flags_saved);
    for(unsigned i=0;i<3;++i){
        assert(w->party_active[i]==(i<c->party));
        assert(w->battle_map_party_active[i]==c->flags[i]);
        assert(w->party_ids[i]==c->order[i]+1);
        if(i<c->party)assert(w->battle_native_targets[i]==c->targets[i]);
    }
#endif
    for(unsigned i=0;i<c->party+3;++i){
        const ScriptEntryRecord *r=&c->records[i];unsigned e=i<c->party?0:i-c->party;
        if(i>=c->party){
#ifdef NPE_BATTLE_FALLBACK
            assert(w->battle_selected_ids[e]==c->enemies[e]);
            if(e>=c->enemy_count){assert(!w->battle_enemy_hps[e]);continue;}
#endif
            if(c->enemies[e])assert(w->battle_enemy_ids[e]==c->enemies[e]);
            // Empty records retain unrelated original RAM; they are not
            // claimed as initialized or compared as active combatants.
            if(!c->enemies[e])continue;}
        unsigned hp=i<c->party?w->hp[i]:w->battle_enemy_hps[e];
        unsigned maximum=i<c->party?w->max_hp[i]:w->battle_enemy_max_hps[e];
        unsigned qi=i<c->party?w->qi[i]:w->battle_enemy_qis[e];
        unsigned max_qi=i<c->party?w->max_qi[i]:w->battle_enemy_max_qis[e];
        unsigned attack=i<c->party?w->attack[i]:w->battle_enemy_attacks[e];
        unsigned defense=i<c->party?w->defense[i]:w->battle_enemy_defenses[e];
        fprintf(stderr,"record%u HP%u/%u max%u/%u atk%u/%u def%u/%u\n",i,hp,r->hp,maximum,r->maximum,attack,r->attack,defense,r->defense);
        assert(hp==r->hp&&maximum==r->maximum&&qi==r->qi&&max_qi==r->max_qi&&attack==r->attack&&defense==r->defense);
        if(i>=c->party)assert(w->battle_enemy_speeds[e]==r->speed);
        for(unsigned s=0;s<4;++s)assert((uint16_t)(i<c->party?w->battle_party_status[i][s]:w->battle_enemy_status[e][s])==r->status[s]);
    }
}
static void configure(const NpeFile *f,NpeWorld *w,unsigned index,int prepare){
    (void)prepare;
    const ScriptEntryCase *c=&script_cases[index];assert(!npe_world_init(f,w,9,29,13));
    w->game_mode=3;w->story_event_id=1;w->story_progress=6;w->music_enabled=0;
#ifdef NPE_BATTLE_PARTY_MAPPING
    /* Copy the requested NPE character records without changing their IDs
       or resource attributes; match the original diagnostic map record order. */
    NpeWorld original=*w;
    for(unsigned i=0;i<3;++i){unsigned j=c->order[i];
#define COPY_PARTY(field) memcpy(&w->field[i],&original.field[j],sizeof(w->field[i]))
        COPY_PARTY(party_names);COPY_PARTY(party_characters);COPY_PARTY(party_attack_groups);COPY_PARTY(party_ids);COPY_PARTY(skill_slots);COPY_PARTY(equipped_items);
        COPY_PARTY(max_hp);COPY_PARTY(qi);COPY_PARTY(max_qi);COPY_PARTY(attack);COPY_PARTY(defense);
        COPY_PARTY(experience);COPY_PARTY(experience_to_next);COPY_PARTY(level);COPY_PARTY(growth);
        COPY_PARTY(other_stats);COPY_PARTY(damage_floor_stat);COPY_PARTY(status_guard);
#undef COPY_PARTY
        w->party_active[i]=(uint8_t)c->flags[i];w->hp[i]=(uint16_t)c->hps[i];
    }
#else
    for(unsigned i=0;i<3;++i){w->party_active[i]=(uint8_t)(i<c->party);
        if(i<c->party)w->hp[i]=(uint16_t)c->hps[i];
        for(unsigned s=0;s<4;++s)w->battle_party_status[i][s]=(int16_t)(s%2?(s==1?3:5):(s==0?-2:-4));}
#endif
#ifdef NPE_BATTLE_EVENT_RESIDENT
    w->story_event_id=(uint16_t)c->event;
    if(npe_rpg_runtime_owner(f)!=w)assert(!npe_world_resources_open((NpeFile *)f,w));
    assert(!npe_rpg_runtime_write(f,0x5277fc,c->battle_records,32)&&!npe_rpg_runtime_write(f,0x52761a,c->monster_records,56));
    assert(!npe_rpg_runtime_write(f,0x520000+c->alias_address,c->alias,8));
#ifdef NPE_BATTLE_FALLBACK
    assert(!npe_rpg_runtime_write(f,0x527916,c->random,8)&&!npe_rpg_runtime_write(f,0x52783a,c->chase,28));
    w->event_context_id=(uint16_t)c->context;
    w->battle_enemy_ids[0]=1;w->battle_enemy_ids[1]=51;w->battle_enemy_ids[2]=2;
    w->battle_selected_ids[0]=1;w->battle_selected_ids[1]=51;w->battle_selected_ids[2]=2;
#ifdef NPE_BATTLE_ZERO_EVENT
    w->battle_formation_count=3;
#endif
    if(c->context>0&&c->context<32768){uint8_t word[]={(uint8_t)(c->chase_enemy>>8),(uint8_t)c->chase_enemy};
        assert(!npe_rpg_runtime_write(f,0x527840+(c->context-1u)*14u,word,2));}
    npe_world_seed(w,c->seed);
#endif
#endif
#ifdef NPE_BATTLE_ZERO_EVENT
    if(prepare){assert(npe_world_prepare_battle(f,w)==1);check(w,index);}
    else w->map_init_pending=1;
#else
    assert(npe_world_prepare_scripted_battle(f,w)==1);check(w,index);
#endif
}
static void compare(SDL_Renderer *renderer,const uint8_t *pixels,unsigned index,unsigned commit,const char *route){
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];reference(index,commit,native);
    unsigned diff=0,sdl_diff=0;for(unsigned i=0;i<sizeof(native);++i)diff+=pixels[i]!=native[i];
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        sdl_diff+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s %s commit%u C=%u SDL=%u\n",route,script_cases[index].name,commit,diff,sdl_diff);
    assert(!diff&&!sdl_diff);
#ifdef NPE_BATTLE_EVENT_RESIDENT
    char path[180];
#ifdef NPE_BATTLE_ZERO_EVENT
    snprintf(path,sizeof(path),"tmp/c-battle-zero-event-%u-%s-%u.idx",index,route,commit);
#elif defined(NPE_BATTLE_FALLBACK)
    snprintf(path,sizeof(path),"tmp/c-battle-fallback-entry-%u-%s-%u.idx",index,route,commit);
#else
    snprintf(path,sizeof(path),"tmp/c-battle-event-resident-entry-%u-%s-%u.idx",index,route,commit);
#endif
    FILE *out=fopen(path,"wb");assert(out&&fwrite(pixels,1,sizeof(native),out)==sizeof(native)&&!fclose(out));
#endif
}
static void present_probe(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!active)return;assert(main_commits<4);
    compare(renderer,last_pixel_frame,scenario,main_commits++,"main");
}
static int wait_outer(SDL_Event *event,int timeout){
    (void)timeout;memset(event,0,sizeof(*event));
    if(!stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;++stage;return 1;}
    if(stage==1){assert(live->game_mode==1);NpeWorld saved=*live;
        configure(live->battle_file,live,scenario,0);
#ifdef NPE_BATTLE_ZERO_EVENT
        /* Enter through a no-op party followup, as real NPE records do.
           Injecting unfinished mode3 before input would instead run its
           command menu and consume an unsupported key before entry. */
        live->party_events[0]=(NpeMapPartyEvent){.character_id=1,.next_mode=3};
        live->game_mode=10;live->story_event_id=1;
#endif
        live->sound_observer=saved.sound_observer;live->sound_context=saved.sound_context;
        live->battle_animation_observer=saved.battle_animation_observer;live->battle_animation_context=saved.battle_animation_context;
        active=1;++stage;event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_UNKNOWN;return 1;}
    assert(stage==2&&main_commits==4);check(live,scenario);active=0;++stage;event->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("scripted entry",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    for(unsigned i=0;i<sizeof(script_cases)/sizeof(*script_cases);++i){NpeWorld w;uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];configure(&file,&w,i,1);
        for(unsigned j=0;j<4;++j){assert(!npe_render_battle_entry_frame(&file,&font,&w,j,pixels));draw_pixels(renderer,texture,pixels);compare(renderer,pixels,i,j,"direct");}
#ifdef NPE_BATTLE_PARTY_MAPPING
        w.story_event_id=0;npe_world_seed(&w,1);
        assert(npe_world_battle_flee(&w)==NPE_BATTLE_FLED && !w.battle_party_flags_saved);
        for(unsigned m=0;m<3;++m)assert(w.party_active[m]==script_cases[i].flags[m]);
        const char *path="tmp/battle-party-prefix-exit.npes";NpeWorld loaded;
        assert(!npe_world_save(&file,&w,path) && !npe_world_load(&file,&loaded,path));
        assert(!memcmp(w.party_active,loaded.party_active,sizeof(w.party_active)) &&
               !memcmp(w.party_ids,loaded.party_ids,sizeof(w.party_ids)) &&
               !memcmp(w.hp,loaded.hp,sizeof(w.hp)) && !loaded.battle_party_flags_saved);
        assert(!remove(path));
#endif
#ifdef NPE_BATTLE_EVENT_RESIDENT
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
#endif
    }
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_font_free(&font);npe_free(&file);
    for(scenario=0;scenario<sizeof(script_cases)/sizeof(*script_cases);++scenario){stage=main_commits=0;active=0;
        char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==3);}
#ifdef NPE_BATTLE_ZERO_EVENT
    fprintf(stderr,"Zero-event entry4 cases,16 C/direct-SDL/main-SDL original commits,shared RNG/retained IDs/party fields,zero differences\n");
#elif defined(NPE_BATTLE_FALLBACK)
    fprintf(stderr,"Battle fallback13 cases,52 C/direct-SDL/main-SDL commits,retained IDs/shared RNG,zero differences\n");
#elif defined(NPE_BATTLE_EVENT_RESIDENT)
    fprintf(stderr,"Battle event resident7 cases,28 C/direct-SDL/main-SDL commits,zero differences\n");
#elif defined(NPE_BATTLE_PARTY_MAPPING)
    fprintf(stderr,"Battle party mapping 4 cases,16 C/direct-SDL/main-SDL commits,zero differences\n");
#else
    fprintf(stderr,"Scripted entry 5 cases, 20 C/direct-SDL/main-SDL commits, zero differences\n");
#endif
    return 0;
}
