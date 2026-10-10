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
typedef struct {unsigned id,graphic,first,last,direction,x,y,moving;} EntryNpc;
typedef struct {unsigned graphic,first,last,enemy,x,y,cooldown;} EntryChase;
typedef struct {unsigned hp,maximum,qi,max_qi,attack,defense,speed,status[4];} EntryRecord;
typedef struct {
    const char *name;unsigned commits,hp_before,encounters,steps_before,hp_after,mode,steps_after;
    uint32_t rng_after;unsigned chase_slot;uint32_t seed;unsigned party,hp_before_slots[3],encounter_ids[8];
    EntryNpc npc[20];EntryChase chase_before[2],chase_after[2];
    unsigned enemies[3];EntryRecord records[6];
#ifdef NPE_BATTLE_SELECTION_ENTRY
    unsigned before_ids[3],before_num,native_count,native_flag;
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
    unsigned event,trigger[5];uint32_t money;
#endif
} EntryCase;
#ifdef NPE_EXIT_RECORDS
typedef struct {unsigned key,commits,icon,level,selection,item,actor,mode;} EntryNavigation;
#ifdef NPE_REENTRY_ACTIONS
typedef struct {EntryNavigation ui;unsigned execute;uint32_t rng;
#ifdef NPE_BATTLE_COMMAND_RESIDENT
    uint8_t previous_kind;
#ifdef NPE_BATTLE_PHASE_RESIDENT
    uint8_t phase;
#endif
#endif
} ReentryAction;
#ifdef NPE_BATTLE_MENU_RESIDENT
#ifdef NPE_BATTLE_COMMAND_RESIDENT
#ifdef NPE_BATTLE_PHASE_RESIDENT
#ifdef NPE_REENTRY_ATTACK
#if defined(NPE_REENTRY_GROUPS)
#include "battle_reentry_groups_native.h"
#define FRAME_PREFIX "tmp/native-battle-reentry-groups"
#elif defined(NPE_REENTRY_MIXED)
#include "battle_reentry_mixed_native.h"
#define FRAME_PREFIX "tmp/native-battle-reentry-mixed"
#elif defined(NPE_REENTRY_TWO_ATTACKS)
#include "battle_reentry_two_attacks_native.h"
#define FRAME_PREFIX "tmp/native-battle-reentry-two-attacks"
#else
#ifdef NPE_REENTRY_ATTACK_SINGLE
#include "battle_reentry_attack_one_enemy_native.h"
#define FRAME_PREFIX "tmp/native-battle-reentry-attack-one-enemy"
#else
#include "battle_reentry_attack_native.h"
#define FRAME_PREFIX "tmp/native-battle-reentry-attack"
#endif
#endif
#else
#ifdef NPE_ENTRY_EXIT_RESOURCES
#include "battle_resources_phase_resident_native.h"
#define FRAME_PREFIX "tmp/native-battle-resources-phase-resident"
#else
#include "battle_phase_resident_native.h"
#define FRAME_PREFIX "tmp/native-battle-phase-resident"
#endif
#endif
#else
#include "battle_command_resident_native.h"
#define FRAME_PREFIX "tmp/native-battle-command-resident"
#endif
#else
#include "battle_menu_resident_native.h"
#define FRAME_PREFIX "tmp/native-battle-menu-resident"
#endif
#else
#include "battle_reentry_actions_native.h"
#define FRAME_PREFIX "tmp/native-battle-reentry-actions"
#endif
#elif defined(NPE_RECORD_REENTRY)
#include "battle_record_reentry_native.h"
#define FRAME_PREFIX "tmp/native-battle-record-reentry"
#elif defined(NPE_EXIT_COMMAND_RECORDS)
#include "battle_exit_command_records_native.h"
#define FRAME_PREFIX "tmp/native-battle-exit-command-records"
#else
#ifdef NPE_ENTRY_EXIT_RESOURCES
#include "battle_resources_exit_records_native.h"
#define FRAME_PREFIX "tmp/native-battle-resources-exit-records"
#else
#include "battle_exit_records_native.h"
#define FRAME_PREFIX "tmp/native-battle-exit-records"
#endif
#endif
#elif defined(NPE_COORDINATE_TARGETS)
typedef struct {unsigned key,commits,icon,level,selection,item,actor,mode,target;} EntryNavigation;
#include "battle_coordinate_targets_native.h"
#define FRAME_PREFIX "tmp/native-battle-coordinate-targets"
#elif defined(NPE_LIVE_COORDINATES)
typedef struct {unsigned key,commits,icon,level,selection,item,actor,mode;} EntryNavigation;
#include "battle_live_coordinates_native.h"
#define FRAME_PREFIX "tmp/native-battle-live-coordinates"
#elif defined(NPE_PARTY_DESCRIPTOR_ENTRY)
typedef struct {unsigned key,commits,icon,level,selection,item,actor,mode;} EntryNavigation;
#include "party_battle_records_native.h"
#define FRAME_PREFIX "tmp/native-party-battle-records"
#elif defined(NPE_BATTLE_ZERO_FLEE)
typedef struct {unsigned key,commits,icon,level,selection,item,actor,mode;} EntryNavigation;
#include "battle_zero_flee_native.h"
#define FRAME_PREFIX "tmp/native-battle-zero-flee"
#elif defined(NPE_BATTLE_ZERO_MENU)
typedef struct {unsigned key,commits,icon,level,selection,item,actor,mode;} EntryNavigation;
#include "battle_zero_menu_native.h"
#define FRAME_PREFIX "tmp/native-battle-zero-menu"
#elif defined(NPE_BATTLE_ZERO_ENTRY)
#include "battle_zero_entry_native.h"
#define FRAME_PREFIX "tmp/native-battle-zero-entry"
#elif defined(NPE_ENEMY_RECORD_ENTRY)
#include "enemy_record_entry_native.h"
#define FRAME_PREFIX "tmp/native-enemy-record-entry"
#elif defined(NPE_BATTLE_SELECTION_ENTRY)
#include "battle_selection_entry_native.h"
#define FRAME_PREFIX "tmp/native-battle-selection-entry"
#elif defined(NPE_ENCOUNTER_CONTROL)
#include "encounter_control_native.h"
#define FRAME_PREFIX "tmp/native-encounter-control"
#elif defined(NPE_WALK_TRIGGER_MATRIX)
#include "walk_trigger_continuation_native.h"
#define FRAME_PREFIX "tmp/native-walk-trigger-continuation"
#else
#include "walk_battle_entry_native.h"
#define FRAME_PREFIX "tmp/native-walk-battle-entry"
#endif
static void word(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
#if defined(NPE_ENCOUNTER_CONTROL) || defined(NPE_BATTLE_SELECTION_ENTRY)
static unsigned get_word(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
#else
#include "chase_fixture_map.h"
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
#include "trigger_fixture_map.h"
#endif
static unsigned main_case,stage,main_commits;
#ifdef NPE_BATTLE_ZERO_MENU
static unsigned navigation_step,navigation_commits,navigation_checked;
#define NAVIGATION_STEPS (sizeof(entry_navigation[0])/sizeof(entry_navigation[0][0]))
static int direct_navigation;
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
static unsigned menu_commits;
#endif
static int active;
#ifdef NPE_PARTY_DESCRIPTOR_ENTRY
static const char *entry_record_route="direct";
#ifdef NPE_ENTRY_EXIT_RESOURCES
static void check_entry_resource(const NpeWorld *w,unsigned index,const char *label,unsigned expected){
    uint8_t flags[2];assert(w->battle_resource_flags==expected);
    assert(w->battle_file && npe_rpg_runtime_owner(w->battle_file)==w);
    assert(!npe_rpg_runtime_read(w->battle_file,0x527aa4,flags,2));
    assert(((unsigned)flags[0]*256+flags[1])==expected);
    char path[200];snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-%s-resource.bin",index,entry_record_route,label);
    FILE *out=fopen(path,"wb");assert(out && fwrite(flags,1,2,out)==2 && !fclose(out));
    unsigned display_expected;
    if(!strcmp(label,"entry"))display_expected=entry_display_after[index];
    else if(!strcmp(label,"exit"))display_expected=exit_display_target[index];
#ifdef NPE_RECORD_REENTRY
    else if(!strcmp(label,"reentry-retained"))display_expected=reentry_display_target[index][0];
    else if(!strcmp(label,"reentry-input"))display_expected=reentry_display_target[index][1];
    else if(!strcmp(label,"reentry-after"))display_expected=reentry_display_target[index][2];
#ifdef NPE_REENTRY_ACTIONS
    else {unsigned step;assert(sscanf(label,"action-%u",&step)==1 && step<reentry_action_steps[index]);display_expected=action_display_target[index][step];}
#else
    else {assert(0);display_expected=0;}
#endif
#else
    else {assert(0);display_expected=0;}
#endif
    uint8_t display;assert(!npe_rpg_runtime_read(w->battle_file,0x527ac0,&display,1));
    if(display!=display_expected)fprintf(stderr,"%s %s global target:C%u original%u\n",entry_cases[index].name,label,display,display_expected);
    assert(display==display_expected && w->battle_display_target==display_expected);
    snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-%s-display-target.bin",index,entry_record_route,label);
    out=fopen(path,"wb");assert(out && fwrite(&display,1,1,out)==1 && !fclose(out));
    assert(!npe_rpg_runtime_failures(w->battle_file));
}
#endif
static void configure_voice_cache(const NpeFile *file,unsigned index){
    const uint32_t addresses[]={0x527946,0x527942,0x527966,0x527962};
    for(unsigned i=0;i<4;++i){uint32_t v=entry_voices[index][i];
        uint8_t raw[]={(uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v};
        assert(!npe_rpg_runtime_write(file,addresses[i],raw,4));
    }
}
#endif
#ifdef NPE_LIVE_COORDINATES
static void mutate_live_coordinates(NpeWorld *w,unsigned index){
    unsigned count=entry_cases[index].party;
    for(unsigned m=0;m<count+3;++m){
        assert(!npe_world_record_write(w,1,m,124,entry_coordinates[index][m],2));
        assert(npe_world_battle_coordinate(w,m>=count,m>=count?m-count:m,0)==entry_coordinates[index][m][0]);
        assert(npe_world_battle_coordinate(w,m>=count,m>=count?m-count:m,1)==entry_coordinates[index][m][1]);
    }
}
#endif
static Uint32 fake_clock;
#ifdef NPE_RECORD_REENTRY
static unsigned reentry_started,reentry_checked;
#ifdef NPE_REENTRY_ACTIONS
static unsigned reentry_action_step,reentry_action_commits;
static void check_reentry_action_records(const NpeWorld *w,unsigned index,unsigned step){
#ifdef NPE_ENTRY_EXIT_RESOURCES
    char label[32];snprintf(label,sizeof(label),"action-%u",step);
    check_entry_resource(w,index,label,action_resource_flags[index][step]);
#endif
#ifdef NPE_REENTRY_MIXED
    assert(npe_world_inventory_word(w,mixed_inventory_item[index])==mixed_stock[index][step]);
    assert(w->skill_record_id==mixed_skill_ids[index][step]);
    assert(w->item_record_id==mixed_item_ids[index][step]);
    assert(!memcmp(w->skill_record_bytes,mixed_skill_bytes[index][step],144));
    assert(!memcmp(w->item_record_bytes,mixed_item_bytes[index][step],142));
    uint8_t caches[290];word(caches,npe_world_inventory_word(w,mixed_inventory_item[index]));
    caches[2]=w->skill_record_id;caches[3]=w->item_record_id;
    memcpy(caches+4,w->skill_record_bytes,144);memcpy(caches+148,w->item_record_bytes,142);
    char cache_path[180];snprintf(cache_path,sizeof(cache_path),FRAME_PREFIX "-c-%u-%s-action-state-%u-caches.bin",index,entry_record_route,step);
    FILE *cache_out=fopen(cache_path,"wb");assert(cache_out&&fwrite(caches,1,sizeof(caches),cache_out)==sizeof(caches)&&!fclose(cache_out));
#endif
    const ReentryAction *n=&reentry_actions[index][step];
    fprintf(stderr,"%s action%u state mode%u actor%u RNG%u expected%u\n",entry_cases[index].name,step,w->game_mode,w->battle_actor,w->random_state,n->rng);
    assert(w->game_mode==n->ui.mode&&w->battle_actor==n->ui.actor&&w->random_state==n->rng);
#ifdef NPE_BATTLE_MENU_RESIDENT
    uint8_t rows[
#ifdef NPE_BATTLE_COMMAND_RESIDENT
#ifdef NPE_BATTLE_PHASE_RESIDENT
        5
#else
        4
#endif
#else
        2
#endif
    ];
    assert(!npe_world_resident_read(w,0x527a9c,rows,1)&&!npe_world_resident_read(w,0x527aa2,rows+1,1));
    assert(rows[0]==n->ui.selection&&rows[1]==n->ui.item);
#ifdef NPE_BATTLE_COMMAND_RESIDENT
    assert(!npe_world_resident_read(w,0x527a9e,rows+2,1)&&!npe_world_resident_read(w,0x527aae,rows+3,1));
    fprintf(stderr,"%s action%u actor/previous-kind C=%u/%u original=%u/%u\n",entry_cases[index].name,step,rows[2],rows[3],n->ui.actor,n->previous_kind);
    assert(rows[2]==n->ui.actor&&rows[3]==n->previous_kind);
#ifdef NPE_BATTLE_PHASE_RESIDENT
    assert(!npe_world_resident_read(w,0x527ac8,rows+4,1));
    fprintf(stderr,"%s action%u phase C=%u original=%u\n",entry_cases[index].name,step,rows[4],n->phase);
    assert(rows[4]==n->phase);
#endif
#endif
    char row_path[180];snprintf(row_path,sizeof(row_path),FRAME_PREFIX "-c-%u-%s-action-state-%u-menu.bin",index,entry_record_route,step);
    FILE *row_out=fopen(row_path,"wb");assert(row_out&&fwrite(rows,1,sizeof(rows),row_out)==sizeof(rows)&&!fclose(row_out));
#endif
    for(unsigned battle=0;battle<2;++battle){
        const uint8_t (*expected)[148]=battle?reentry_action_battle[index][step]:reentry_action_map[index][step];
        for(unsigned slot=0;slot<(battle?6u:3u);++slot){
            uint8_t actual[148];assert(!(battle?npe_world_battle_record_image(w,slot,actual):npe_world_map_record_image(w,slot,actual)));
            unsigned diff=0;
            for(unsigned b=0;b<148;++b)if(actual[b]!=expected[slot][b]){
                if(diff++<12)fprintf(stderr,"%s action%u %s%u byte%u C=%02x original=%02x\n",entry_cases[index].name,step,battle?"battle":"map",slot,b,actual[b],expected[slot][b]);
            }
            char path[180];snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-action-state-%u-%s-record-%u.bin",index,entry_record_route,step,battle?"battle":"map",slot);
            FILE *out=fopen(path,"wb");assert(out&&fwrite(actual,1,148,out)==148&&!fclose(out));assert(!diff);
        }
    }
}
#endif
static void check_reentry_records(const NpeWorld *w,unsigned index,unsigned phase){
    const char *name=phase==0?"retained":phase==1?"input":"after";
#ifdef NPE_ENTRY_EXIT_RESOURCES
    char label[32];snprintf(label,sizeof(label),"reentry-%s",name);
    check_entry_resource(w,index,label,reentry_resource_flags[index][phase]);
#endif
    for(unsigned battle=0;battle<2;++battle){
        const uint8_t (*expected)[148]=battle?
            (phase==0?reentry_retained_battle[index]:phase==1?reentry_input_battle[index]:reentry_after_battle[index]):
            (phase==0?reentry_retained_map[index]:phase==1?reentry_input_map[index]:reentry_after_map[index]);
        for(unsigned slot=0;slot<(battle?6u:3u);++slot){
            uint8_t actual[148];assert(!(battle?npe_world_battle_record_image(w,slot,actual):npe_world_map_record_image(w,slot,actual)));
            unsigned diff=0;
            for(unsigned b=0;b<148;++b)if(actual[b]!=expected[slot][b]){
                if(diff++<12)fprintf(stderr,"%s %s %s%u byte%u C=%02x original=%02x\n",entry_cases[index].name,name,battle?"battle":"map",slot,b,actual[b],expected[slot][b]);
            }
            char path[180];snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-%s-%s-record-%u.bin",index,entry_record_route,name,battle?"battle":"map",slot);
            FILE *out=fopen(path,"wb");assert(out&&fwrite(actual,1,148,out)==148&&!fclose(out));
            assert(!diff);
        }
    }
    if(phase==2){
#ifdef NPE_REENTRY_ATTACK
        unsigned enemy_count=reentry_enemy_count[index];
#else
        unsigned enemy_count=2;
#endif
        assert(w->game_mode==3&&w->battle_record_party_count==reentry_party[index]&&
            w->battle_enemy_count==enemy_count&&w->random_state==reentry_rng[index]);
    }
#ifdef NPE_BATTLE_PHASE_RESIDENT
    if(phase==2)assert(w->battle_menu_phase==reentry_phase_after[index]);
#endif
}
static void configure_reentry(NpeWorld *w,unsigned index){
    check_reentry_records(w,index,0);
    for(unsigned m=0;m<3;++m){const uint8_t flag=(uint8_t)(m<reentry_party[index]);
        assert(!npe_world_record_write(w,0,m,43,&flag,1));
        if(flag){uint8_t hp[2];word(hp,(unsigned[]){200,180,220}[m]);assert(!npe_world_record_write(w,0,m,80,hp,2));}
    }
#ifdef NPE_REENTRY_ATTACK
    uint8_t ids[6];for(unsigned e=0;e<3;++e)word(ids+2*e,reentry_enemy_ids[index][e]);
    const uint8_t count=reentry_enemy_count[index],empty[8]={0},control[]={3,232},enabled[]={0,1};
#else
    const uint8_t ids[]={0,1,0,2,0,0},count=2,empty[8]={0},control[]={3,232},enabled[]={0,1};
#endif
    assert(!npe_world_resident_write(w,0x52792e,ids,6)&&!npe_world_resident_write(w,0x527af0,&count,1));
    assert(!npe_world_resident_write(w,0x527916,empty,8)&&!npe_world_resident_write(w,0x527a02,control,2));
    assert(!npe_world_resident_write(w,0x527a5c,enabled,2));npe_world_seed(w,11);
    check_reentry_records(w,index,1);
}
#endif
#ifdef NPE_EXIT_RECORDS
#ifdef NPE_EXIT_COMMAND_RECORDS
static void save_exit_inventory(const NpeWorld *w,unsigned index,const char *route){
    const unsigned items[]={1,8,159};uint8_t raw[6];
    for(unsigned i=0;i<3;++i)word(raw+2*i,npe_world_inventory_word(w,items[i]));
    char path[180];snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-inventory.bin",index,route);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(raw,1,sizeof(raw),out)==sizeof(raw)&&!fclose(out));
}
#endif
static void mutate_exit_inputs(NpeWorld *w,unsigned index){
    unsigned count=entry_cases[index].party;
    for(unsigned slot=0;slot<count+entry_cases[index].native_count;++slot){
        const uint8_t *expected=slot<count?exit_input_party[index][slot]:exit_input_enemy[index][slot-count];
        uint8_t before[148],actual[148];assert(!npe_world_battle_record_image(w,slot,before));
        for(unsigned b=0;b<148;++b)if(before[b]!=expected[b])
            assert(!npe_world_record_write(w,1,slot,b,expected+b,1));
        assert(!npe_world_battle_record_image(w,slot,actual)&&!memcmp(actual,expected,148));
    }
    if(exit_scripted[index]){
        const uint8_t event[]={0,1,0,0,1,0,0,0};
        assert(!npe_world_resident_write(w,0x5277fc,event,sizeof(event)));
        w->story_event_id=1;w->battle_scripted_entry=1;
    }
#ifdef NPE_EXIT_COMMAND_RECORDS
    const unsigned items[]={1,8,159};
    for(unsigned i=0;i<3;++i)
        assert(!npe_world_set_inventory_word(w,items[i],exit_inventory_before[index][i]));
#endif
}
static void verify_exit_records(const NpeWorld *w,unsigned index){
#ifdef NPE_ENTRY_EXIT_RESOURCES
    check_entry_resource(w,index,"exit",exit_resource_flags[index]);
#endif
    assert(w->game_mode==exit_mode[index]&&w->random_state==exit_rng[index]);
#ifdef NPE_BATTLE_PHASE_RESIDENT
    uint8_t phase;assert(!npe_world_resident_read(w,0x527ac8,&phase,1)&&phase==exit_phase[index]);
    char phase_path[180];snprintf(phase_path,sizeof(phase_path),FRAME_PREFIX "-c-%u-%s-exit-phase.bin",index,entry_record_route);
    FILE *phase_out=fopen(phase_path,"wb");assert(phase_out&&fwrite(&phase,1,1,phase_out)==1&&!fclose(phase_out));
#endif
#ifdef NPE_EXIT_COMMAND_RECORDS
    const unsigned items[]={1,8,159};
    for(unsigned i=0;i<3;++i){
        fprintf(stderr,"%s exit item%u count C=%u original=%u\n",entry_cases[index].name,items[i],npe_world_inventory_word(w,items[i]),exit_inventory_after[index][i]);
        assert(npe_world_inventory_word(w,items[i])==exit_inventory_after[index][i]);
    }
    save_exit_inventory(w,index,entry_record_route);
#endif
    for(unsigned m=0;m<entry_cases[index].party;++m)for(unsigned battle=0;battle<2;++battle){
        const uint8_t *expected=battle?exit_battle_records[index][m]:exit_map_records[index][m];
        uint8_t actual[148];assert(!(battle?npe_world_battle_record_image(w,m,actual):npe_world_map_record_image(w,m,actual)));
        unsigned differences=0;
        for(unsigned b=0;b<148;++b)if(actual[b]!=expected[b]){
            fprintf(stderr,"%s exit %s%u byte%u C=%02x original=%02x\n",entry_cases[index].name,battle?"battle":"map",m,b,actual[b],expected[b]);++differences;
        }
        fprintf(stderr,"%s exit %s%u full148 differences=%u\n",entry_cases[index].name,battle?"battle":"map",m,differences);
        char path[180];snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-exit-%s-record-%u.bin",index,entry_record_route,battle?"battle":"map",m);
        FILE *out=fopen(path,"wb");assert(out&&fwrite(actual,1,148,out)==148&&!fclose(out));
        assert(!differences);
    }
    if(w->game_mode==1 && !strcmp(entry_record_route,"direct")){
        char path[180];snprintf(path,sizeof(path),FRAME_PREFIX "-own-save-%u.npes",index);
        NpeWorld cold,warm=*w;
        assert(!npe_world_save(w->battle_file,w,path));
        assert(!npe_world_load(w->battle_file,&cold,path));
        warm.hp[0]=600;
        const uint8_t zeros[12]={0};
        assert(!npe_world_record_write(&warm,0,0,114,zeros,sizeof(zeros)));
        assert(!npe_world_restore(w->battle_file,&warm,path));
        for(unsigned m=0;m<3;++m){
            uint8_t expected[148],actual[148];
            assert(!npe_world_map_record_image(w,m,expected));
            for(unsigned running=0;running<2;++running){
                const NpeWorld *restored=running?&warm:&cold;
                assert(!npe_world_map_record_image(restored,m,actual)&&!memcmp(expected,actual,148));
                char record_path[180];snprintf(record_path,sizeof(record_path),FRAME_PREFIX "-c-%u-save-%s-map-record-%u.bin",index,running?"warm":"cold",m);
                FILE *out=fopen(record_path,"wb");assert(out&&fwrite(actual,1,148,out)==148&&!fclose(out));
            }
        }
        assert(cold.random_state==w->random_state&&warm.random_state==w->random_state);
#ifdef NPE_EXIT_COMMAND_RECORDS
        for(unsigned i=0;i<3;++i){
            assert(npe_world_inventory_word(&cold,items[i])==npe_world_inventory_word(w,items[i]));
            assert(npe_world_inventory_word(&warm,items[i])==npe_world_inventory_word(w,items[i]));
        }
        save_exit_inventory(&cold,index,"save-cold");
        save_exit_inventory(&warm,index,"save-warm");
#endif
        assert(cold.x==w->x&&cold.y==w->y&&warm.x==w->x&&warm.y==w->y);
        assert(!remove(path));
        fprintf(stderr,"%s own-save cold/warm full map records passed\n",entry_cases[index].name);
    }
}
#endif
static NpeWorld *live;
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static int wait_outer(SDL_Event *,int);
#ifdef NPE_COORDINATE_TARGETS
static int wait_target(SDL_Event *event){return wait_outer(event,0);}
#define SDL_WaitEvent wait_target
#endif
static void present_probe(SDL_Renderer *);
static Uint32 ticks(void){return fake_clock;}
static SDL_Window *create_window(const char *s,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);
}
#define SDL_GetTicks ticks
/* Keep original logical deadline and blocking keys when the production
   bridge polls its physical backend in short slices. */
#define SDL_WaitEventTimeout(event,slice) wait_outer(event,!strcmp(caller,"game_WaitEvent")?0:timeout)
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
#ifdef NPE_COORDINATE_TARGETS
#undef SDL_WaitEvent
#endif
#undef SDL_CreateWindow
#undef npe_sound_select
#undef SDL_RenderPresent
#ifdef NPE_BATTLE_ZERO_MENU
#include "battle_input_keys.h"
static unsigned ignored_battle_index,ignored_battle_pending,ignored_battle_total;
static NpeWorld ignored_battle_before;
static uint8_t ignored_battle_pixels[NPE_WIDTH*NPE_HEIGHT];
static int ignored_battle_input(SDL_Event *event){
    if(!getenv("NPE_PROBE_BATTLE_IGNORED_KEYS"))return 0;
    if(ignored_battle_pending){
        if(!same_display_world(&ignored_battle_before,live)){
            fprintf(stderr,"Ignored battle input changed World: case%u phase%u key%u\n",main_case,ignored_battle_before.battle_menu_phase,ignored_battle_index);
            assert(0);
        }
        assert(!memcmp(ignored_battle_pixels,last_pixel_frame,sizeof(ignored_battle_pixels)));
        ignored_battle_pending=0;++ignored_battle_total;
    }
    static const SDL_Keycode keys[]={SDLK_d,SDLK_f,SDLK_u,SDLK_TAB,SDLK_SPACE,SDLK_q};
    if(live->game_mode==3 && ignored_battle_index<sizeof(keys)/sizeof(*keys)){
        ignored_battle_before=*live;memcpy(ignored_battle_pixels,last_pixel_frame,sizeof(ignored_battle_pixels));
        memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;event->key.keysym.sym=keys[ignored_battle_index++];
        const char *variant=getenv("NPE_PROBE_BATTLE_KEYS");
        event->key.keysym.mod=variant&&!strcmp(variant,"shift")?KMOD_SHIFT:variant&&!strcmp(variant,"caps")?KMOD_CAPS:KMOD_NONE;
        ignored_battle_pending=1;return 1;
    }
    ignored_battle_index=0;return 0;
}
#endif
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;if(stage>=1)w->music_enabled=0;npe_sound_select(p,w);}
static void frame(unsigned index,int commit,uint8_t *pixels){
    char name[160];
    if(commit<0)snprintf(name,sizeof(name),FRAME_PREFIX "-%u-before.idx",index);
    else snprintf(name,sizeof(name),FRAME_PREFIX "-%u-commit-%u.idx",index,(unsigned)commit);
    FILE *f=fopen(name,"rb");assert(f&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,f)==NPE_WIDTH*NPE_HEIGHT);
    assert(fgetc(f)==EOF&&!fclose(f));
}
static void configure(NpeFile *file,NpeWorld *w,unsigned index){
#if defined(NPE_ENCOUNTER_CONTROL) || defined(NPE_BATTLE_SELECTION_ENTRY)
    const EntryCase *c=&entry_cases[index];uint64_t fingerprint=npe_game_fingerprint(file);
    assert(!npe_world_new_game(file,w));w->music_enabled=0;
    if(npe_rpg_runtime_owner(file)!=w)assert(!npe_world_resources_open(file,w));
#ifdef NPE_ENTRY_EXIT_RESOURCES
    w->battle_resource_flags=entry_resource_before[index];
    w->battle_display_target=entry_display_before[index];
#endif
#if defined(NPE_ENEMY_RECORD_ENTRY) || defined(NPE_BATTLE_ZERO_ENTRY)
    uint8_t directory[4];assert(!npe_rpg_runtime_read(file,0x5279b6,directory,4));
    uint32_t value=(uint32_t)directory[0]<<24|(uint32_t)directory[1]<<16|(uint32_t)directory[2]<<8|directory[3];
    assert(value==enemy_entry_directory[index]);
#endif
    uint8_t npc[280]={0},chase[28]={0};
    for(unsigned i=0;i<20;++i){uint8_t *p=npc+14*i;const EntryNpc *n=&c->npc[i];
        word(p,n->id);word(p+2,n->graphic);word(p+4,n->first);word(p+6,n->last);
        p[8]=(uint8_t)n->direction;p[9]=(uint8_t)n->x;p[10]=(uint8_t)n->y;p[11]=(uint8_t)n->moving;}
    for(unsigned i=0;i<2;++i){uint8_t *p=chase+14*i;const EntryChase *n=&c->chase_before[i];
        word(p,n->graphic);word(p+2,n->first);word(p+4,n->last);word(p+6,n->enemy);
        word(p+8,n->x);word(p+10,n->y);p[12]=(uint8_t)n->cooldown;}
    uint8_t enemies[8],control[2];for(unsigned i=0;i<8;++i)enemies[i]=(uint8_t)c->encounter_ids[i];
    assert(!npe_rpg_runtime_write(file,0x526ca2,npc,280)&&!npe_rpg_runtime_write(file,0x52783a,chase,28));
    assert(!npe_rpg_runtime_write(file,0x527916,enemies,8));
    word(control,c->encounters);assert(!npe_rpg_runtime_write(file,0x527a5c,control,2));
    word(control,c->steps_before);assert(!npe_rpg_runtime_write(file,0x527a02,control,2));
    for(unsigned i=0;i<4;++i){uint8_t zero[2]={0};
        assert(!npe_rpg_runtime_write(file,0x527620+i*14,zero,2)&&!npe_rpg_runtime_write(file,0x527625+i*14,zero,1));}
    for(unsigned i=0;i<3;++i){w->party_active[i]=(uint8_t)(i<c->party);w->hp[i]=(uint16_t)c->hp_before_slots[i];}
#ifdef NPE_BATTLE_SELECTION_ENTRY
    for(unsigned i=0;i<3;++i){word(control,c->before_ids[i]);assert(!npe_rpg_runtime_write(file,0x52792e + i*2,control,2));}
    uint8_t num=(uint8_t)c->before_num;assert(!npe_rpg_runtime_write(file,0x527af0,&num,1));
#endif
    npe_world_seed(w,c->seed);assert(npe_game_fingerprint(file)==fingerprint);
#ifdef NPE_PARTY_DESCRIPTOR_ENTRY
    for(unsigned m=0;m<c->party;++m){
        assert(!npe_world_record_write(w,0,m,17,entry_map_before[index][m]+17,1));
        assert(!npe_world_record_write(w,0,m,114,entry_map_before[index][m]+114,12));
    }
    configure_voice_cache(file,index);
#endif
#else
    const EntryCase *c=&entry_cases[index];NpeMap map;assert(!npe_map(file,0,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *raw;assert(!npe_map_npc(&map,i,&raw));
        uint8_t *p=file->bytes+(raw-file->bytes);const EntryNpc *n=&c->npc[i];
        word(p,n->id);p[2]=(uint8_t)n->x;p[3]=(uint8_t)n->y;word(p+4,n->graphic);
        p[6]=(uint8_t)n->direction;p[7]=(uint8_t)n->moving;word(p+8,n->first);word(p+10,n->last);
    }
    for(unsigned i=0;i<8;++i)file->bytes[(map.tail-file->bytes)+644+i]=(uint8_t)(c->encounters?c->encounter_ids[i]:0);
    for(unsigned i=0;i<4;++i){uint8_t *p=file->bytes+(map.tail-file->bytes)+592+i*13;p[0]=0;word(p+9,0);}
#ifdef NPE_WALK_TRIGGER_MATRIX
    for(unsigned i=1;i<=10;++i){NpeMapMoveTrigger old;int found=npe_map_move_event(&map,i,&old);
        assert(found>=0);if(!found)break;word(file->bytes+(old.raw-file->bytes),65535);}
    NpeMapMoveTrigger trigger;assert(npe_map_move_event(&map,1,&trigger)==1);
    uint8_t *p=file->bytes+(trigger.raw-file->bytes);word(p,c->trigger[0]);p[2]=(uint8_t)c->trigger[1];
    word(p+3,c->trigger[2]);word(p+5,c->trigger[3]);word(p+7,c->trigger[4]);
#endif
    for(unsigned i=0;i<2;++i){NpeMapChase row;assert(npe_map_chase(&map,i,&row)==1);
        uint8_t *p=file->bytes+(row.raw-file->bytes);const EntryChase *n=&c->chase_before[i];
        word(p,n->graphic);word(p+2,n->first);word(p+4,n->last);word(p+6,n->enemy);
        p[8]=(uint8_t)n->x;p[9]=(uint8_t)n->y;
    }
    assert(!npe_world_new_game(file,w));w->music_enabled=0;
#ifdef NPE_WALK_TRIGGER_MATRIX
    w->money=c->money;
#endif
    for(unsigned i=0;i<3;++i){w->party_active[i]=(uint8_t)(i<c->party);w->hp[i]=(uint16_t)c->hp_before_slots[i];}
    w->encounter_steps=(uint16_t)c->steps_before;npe_world_seed(w,c->seed);
    for(unsigned i=0;i<2;++i){const EntryChase *n=&c->chase_before[i];
        w->chase_x[i]=(uint16_t)n->x;w->chase_y[i]=(uint16_t)n->y;w->chase_cooldown[i]=(uint8_t)n->cooldown;}
#endif
}
static void records(const NpeWorld *w,unsigned index){
#ifdef NPE_ENTRY_EXIT_RESOURCES
    check_entry_resource(w,index,"entry",entry_resource_after[index]);
#endif
#ifdef NPE_PARTY_DESCRIPTOR_ENTRY
    for(unsigned m=0;m<entry_cases[index].party;++m)for(unsigned battle=0;battle<2;++battle){
        uint8_t raw[148];const uint8_t *expected=battle?entry_party_after[index][m]:entry_map_after[index][m];
        assert(!(battle?npe_world_battle_record_image(w,m,raw):npe_world_map_record_image(w,m,raw)));
        unsigned diff=0;for(unsigned j=0;j<148;++j)if(raw[j]!=expected[j]){
            if(diff++<8)fprintf(stderr,"%s entry %s%u byte%u C=%02x original=%02x\n",entry_cases[index].name,battle?"battle":"map",m,j,raw[j],expected[j]);
        }
        assert(!diff);
        char path[180];snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-entry-%s-record-%u.bin",
            index,entry_record_route,battle?"battle":"map",m);
        FILE *out=fopen(path,"wb");assert(out&&fwrite(raw,1,148,out)==148&&!fclose(out));
    }
#endif
    const EntryCase *c=&entry_cases[index];
    fprintf(stderr,"%s state: mode=%u HP=%u counter=%u RNG=%u chase=%u actor=%u\n",
        c->name,w->game_mode,w->hp[0],w->encounter_steps,w->random_state,w->battle_chase_slot,w->battle_actor);
    assert(w->game_mode==c->mode&&w->hp[0]==c->hp_after&&w->encounter_steps==c->steps_after&&
           w->random_state==c->rng_after&&w->battle_chase_slot==c->chase_slot);
#if defined(NPE_ENCOUNTER_CONTROL) || defined(NPE_BATTLE_SELECTION_ENTRY)
    assert(w->encounters_enabled==c->encounters);
    uint8_t control[2];assert(!npe_world_resident_read(w,0x527a5c,control,2)&&get_word(control)==c->encounters);
    assert(!npe_world_resident_read(w,0x527a02,control,2)&&get_word(control)==c->steps_after);
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
    assert(w->story_event_id==c->event&&w->money==c->money);
#endif
    assert(w->map_number==1&&w->story_progress==1&&w->direction==0&&
           w->view.offset_x==36&&w->view.offset_y==1&&w->view.screen_x==7&&w->view.screen_y==1);
    for(unsigned i=0;i<2;++i){const EntryChase *n=&c->chase_after[i];
        assert(w->chase_x[i]==n->x&&w->chase_y[i]==n->y&&w->chase_cooldown[i]==n->cooldown);}
    if(c->mode!=3){assert(!w->battle_enemy_count);return;}
#ifdef NPE_BATTLE_SELECTION_ENTRY
    unsigned count=c->native_count;assert(w->battle_formation_count==count&&w->battle_scripted_entry==c->native_flag);
    for(unsigned i=0;i<3;++i)assert(w->battle_selected_ids[i]==c->enemies[i]);
#else
    unsigned count=0;for(unsigned i=0;i<3;++i)count+=c->enemies[i]!=0;
#endif
    assert(w->battle_actor==0&&w->story_event_id==0&&w->battle_enemy_count==count);
    for(unsigned slot=0;slot<c->party;++slot){const EntryRecord *p=&c->records[slot];
        assert(w->hp[slot]==p->hp&&w->max_hp[slot]==p->maximum&&w->qi[slot]==p->qi&&w->max_qi[slot]==p->max_qi&&
               w->attack[slot]==p->attack&&w->defense[slot]==p->defense);
        for(unsigned i=0;i<4;++i)assert((uint16_t)w->battle_party_status[slot][i]==p->status[i]);
    }
    for(unsigned slot=0;slot<count;++slot){const EntryRecord *e=&c->records[c->party+slot];
        if((int16_t)c->enemies[slot]>0)assert(w->battle_enemy_ids[slot]==(uint8_t)c->enemies[slot]);
        if(w->battle_enemy_hps[slot]!=e->hp||w->battle_enemy_max_hps[slot]!=e->maximum)
            fprintf(stderr,"enemy%u selection=%u C HP=%u max=%u qi=%u attack=%u speed=%u original HP=%u max=%u qi=%u attack=%u speed=%u\n",
                slot,c->enemies[slot],w->battle_enemy_hps[slot],w->battle_enemy_max_hps[slot],w->battle_enemy_qis[slot],w->battle_enemy_attacks[slot],w->battle_enemy_speeds[slot],
                e->hp,e->maximum,e->qi,e->attack,e->speed);
        assert(w->battle_enemy_hps[slot]==e->hp&&w->battle_enemy_max_hps[slot]==e->maximum&&
               w->battle_enemy_qis[slot]==e->qi&&w->battle_enemy_max_qis[slot]==e->max_qi&&
               w->battle_enemy_attacks[slot]==e->attack&&w->battle_enemy_defenses[slot]==e->defense&&w->battle_enemy_speeds[slot]==e->speed);
        for(unsigned i=0;i<4;++i){
            if((uint16_t)w->battle_enemy_status[slot][i]!=e->status[i])
                fprintf(stderr,"enemy%u status%u C=%u native=%u\n",slot,i,(uint16_t)w->battle_enemy_status[slot][i],e->status[i]);
            assert((uint16_t)w->battle_enemy_status[slot][i]==e->status[i]);}
    }
#if defined(NPE_ENEMY_RECORD_ENTRY) || defined(NPE_BATTLE_ZERO_ENTRY)
    for(unsigned e=0;e<(count?count:3);++e){
        uint8_t raw[148];assert(!npe_world_battle_record_image(w,c->party+e,raw));
        unsigned differences=0;
        for(unsigned j=0;j<148;++j)if(raw[j]!=enemy_entry_records[index][e][j]){
            if(differences++<8)fprintf(stderr,"%s enemy%u byte%u C=%02x original=%02x\n",c->name,e,j,raw[j],enemy_entry_records[index][e][j]);
        }
        assert(!differences);
    }
#endif
}
static void compare(SDL_Renderer *renderer,const uint8_t *pixels,unsigned index,unsigned commit,const char *route){
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];frame(index,(int)commit,native);
    unsigned diff=0,sdl_diff=0;for(unsigned i=0;i<sizeof(native);++i)diff+=pixels[i]!=native[i];
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        sdl_diff+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s %s commit%u C=%u SDL=%u\n",route,entry_cases[index].name,commit,diff,sdl_diff);
    if(diff){FILE *out=fopen(FRAME_PREFIX "-actual.idx","wb");assert(out);
        assert(fwrite(pixels,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!diff&&!sdl_diff);
#if defined(NPE_ENCOUNTER_CONTROL) || defined(NPE_BATTLE_SELECTION_ENTRY)
    char path[180];
#ifdef NPE_BATTLE_ZERO_MENU
    snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-entry-%u.idx",index,route,commit);
#elif defined(NPE_BATTLE_ZERO_ENTRY)
    snprintf(path,sizeof(path),"tmp/c-battle-zero-entry-%u-%s-%u.idx",index,route,commit);
#elif defined(NPE_ENEMY_RECORD_ENTRY)
    snprintf(path,sizeof(path),"tmp/c-enemy-record-entry-%u-%s-%u.idx",index,route,commit);
#elif defined(NPE_BATTLE_SELECTION_ENTRY)
    snprintf(path,sizeof(path),"tmp/c-battle-selection-entry-%u-%s-%u.idx",index,route,commit);
#else
    snprintf(path,sizeof(path),"tmp/c-encounter-control-%u-%s-%u.idx",index,route,commit);
#endif
    FILE *out=fopen(path,"wb");assert(out&&fwrite(pixels,1,sizeof(native),out)==sizeof(native)&&!fclose(out));
#endif
}
#ifdef NPE_BATTLE_ZERO_FLEE
static void verify_flee_records(const NpeWorld *world,unsigned index){
    assert(world->game_mode==1&&!world->battle_enemy_count&&!world->battle_party_flags_saved&&world->random_state==flee_rng[index]);
    for(unsigned m=0;m<entry_cases[index].party;++m)for(unsigned battle=0;battle<2;++battle){
        uint8_t raw[148];const uint8_t *expected=battle?flee_battle_records[index][m]:flee_map_records[index][m];
        assert(!(battle?npe_world_battle_record_image(world,m,raw):npe_world_map_record_image(world,m,raw)));
        unsigned diff=0;
        for(unsigned j=0;j<148;++j)if(raw[j]!=expected[j]){
            if(diff++<10)fprintf(stderr,"%s flee %s%u byte%u C=%02x original=%02x\n",entry_cases[index].name,battle?"battle":"map",m,j,raw[j],expected[j]);
        }
        fprintf(stderr,"%s flee %s%u full148-byte record differences=%u\n",
            entry_cases[index].name,battle?"battle":"map",m,diff);
        char path[180];snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-%s-record-%u.bin",index,direct_navigation?"direct":"main",battle?"battle":"map",m);
        FILE *f=fopen(path,"wb");assert(f&&fwrite(raw,1,sizeof(raw),f)==sizeof(raw)&&!fclose(f));
        assert(!diff);
    }
    if(direct_navigation){
        const NpeFile *file=world->battle_file;
        char path[180];snprintf(path,sizeof(path),FRAME_PREFIX "-own-save-%u.npes",index);
        NpeWorld cold,warm=*world;
        assert(!npe_world_save(file,world,path));
        assert(!npe_world_load(file,&cold,path));
        uint8_t zeros[12]={0};
        assert(!npe_world_record_write(&warm,0,0,114,zeros,12));
        assert(!npe_world_restore(file,&warm,path));
        for(unsigned m=0;m<entry_cases[index].party;++m){
            uint8_t expected[148],actual[148];
            assert(!npe_world_map_record_image(world,m,expected));
            assert(!npe_world_map_record_image(&cold,m,actual)&&!memcmp(expected,actual,148));
            assert(!npe_world_map_record_image(&warm,m,actual)&&!memcmp(expected,actual,148));
        }
        assert(cold.x==world->x&&cold.y==world->y&&!memcmp(&cold.view,&world->view,sizeof(world->view)));
        assert(warm.x==world->x&&warm.y==world->y&&!memcmp(&warm.view,&world->view,sizeof(world->view)));
        assert(!memcmp(cold.npc_override_direction,world->npc_override_direction,20));
        /* Running restore rebuilds NPC records from NPE; cold host load
           retains the encoded diagnostic direction bytes. */
        for(unsigned s=0;s<20;++s)assert(warm.npc_override_direction[s]==255);
        assert(!remove(path));
    }
}
#endif
typedef struct {SDL_Renderer *renderer;SDL_Texture *texture;unsigned index,commits;} Comparison;
#ifdef NPE_BATTLE_ZERO_MENU
static void compare_navigation_at(SDL_Renderer *renderer,unsigned index,unsigned step,unsigned commit,const char *route,const char *label){
    char path[180];uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    snprintf(path,sizeof(path),FRAME_PREFIX "-%u-%s-%u-%u.idx",index,label,step,commit);
    FILE *f=fopen(path,"rb");assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned diff=0,sdl_diff=0;
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];diff+=last_pixel_frame[i]!=v;
        sdl_diff+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s %s %s%u commit%u C=%u SDL=%u\n",route,entry_cases[index].name,label,step,commit,diff,sdl_diff);
    if(diff){f=fopen("tmp/battle-zero-menu-actual.idx","wb");assert(f&&fwrite(last_pixel_frame,1,sizeof(native),f)==sizeof(native)&&!fclose(f));}
    assert(!diff&&!sdl_diff);++navigation_checked;
    snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-%s-%u-%u.idx",index,route,label,step,commit);
    f=fopen(path,"wb");assert(f&&fwrite(last_pixel_frame,1,sizeof(native),f)==sizeof(native)&&!fclose(f));
}
static void compare_navigation(SDL_Renderer *renderer,unsigned index,unsigned step,unsigned commit,const char *route){
    compare_navigation_at(renderer,index,step,commit,route,"menu");
}
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
static void compare_menu(SDL_Renderer *renderer,unsigned index,const char *route){
    char name[160];uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    snprintf(name,sizeof(name),FRAME_PREFIX "-0-menu-%u-0.idx",index);
    FILE *f=fopen(name,"rb");assert(f&&fread(native,1,sizeof(native),f)==sizeof(native));
    assert(fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned diff=0,sdl_diff=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        diff+=last_pixel_frame[i]!=v;
        sdl_diff+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s menu navigation%u selection%u C=%u SDL=%u\n",route,index,menu_selections[index],diff,sdl_diff);
    if(diff){FILE *out=fopen(FRAME_PREFIX "-menu-actual.idx","wb");assert(out);
        assert(fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!diff&&!sdl_diff);
}
#endif
static int commit(void *opaque,const uint8_t *pixels,unsigned wait_parameter){
    Comparison *c=opaque;(void)wait_parameter;assert(c->commits<entry_cases[c->index].commits);
    draw_pixels(c->renderer,c->texture,pixels);compare(c->renderer,pixels,c->index,c->commits++,"direct");return 0;
}
#ifdef NPE_RECORD_REENTRY
static void compare_reentry(SDL_Renderer *renderer,unsigned index){
    assert(reentry_checked<reentry_commits[index]);
    char path[180];uint8_t native[28800];uint32_t rgba[28800];
    snprintf(path,sizeof(path),FRAME_PREFIX "-%u-reentry-%u.idx",index,reentry_checked);
    FILE *f=fopen(path,"rb");assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned diff=0,sdl_diff=0;
    for(unsigned i=0;i<28800;++i){unsigned v=native[i];diff+=last_pixel_frame[i]!=v;
        sdl_diff+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s %s reentry%u C=%u SDL=%u\n",entry_record_route,entry_cases[index].name,reentry_checked,diff,sdl_diff);
    snprintf(path,sizeof(path),FRAME_PREFIX "-c-%u-%s-reentry-%u.idx",index,entry_record_route,reentry_checked++);
    f=fopen(path,"wb");assert(f&&fwrite(last_pixel_frame,1,28800,f)==28800&&!fclose(f));
    assert(!diff&&!sdl_diff);
}
static int reentry_commit(void *opaque,const uint8_t *pixels,unsigned delay){
    Comparison *c=opaque;(void)delay;draw_pixels(c->renderer,c->texture,pixels);compare_reentry(c->renderer,c->index);return 0;
}
#endif
static void present_probe(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!active)return;
#ifdef NPE_BATTLE_ZERO_MENU
    if(ignored_battle_pending){fprintf(stderr,"Unexpected LCD on ignored battle input: case%u phase%u key%u\n",main_case,ignored_battle_before.battle_menu_phase,ignored_battle_index);assert(0);}
#endif
#ifdef NPE_RECORD_REENTRY
    if(reentry_started==1){compare_reentry(renderer,main_case);return;}
#ifdef NPE_REENTRY_ACTIONS
    if(reentry_started==2){
        assert(reentry_action_step&&reentry_action_commits<reentry_actions[main_case][reentry_action_step-1].ui.commits);
        compare_navigation_at(renderer,main_case,reentry_action_step-1,reentry_action_commits++,entry_record_route,"action");return;
    }
#endif
#endif
#ifdef NPE_BATTLE_ZERO_MENU
    if(main_commits==entry_cases[main_case].commits){
        assert(navigation_step&&navigation_step<=NAVIGATION_STEPS);
        assert(navigation_commits<entry_navigation[main_case][navigation_step-1].commits);
        compare_navigation(renderer,main_case,navigation_step-1,navigation_commits++,direct_navigation?"direct":"main");return;
    }
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
    if(main_case==0&&main_commits==entry_cases[main_case].commits){
        assert(menu_commits<8);compare_menu(renderer,menu_commits++,"main");return;
    }
#endif
    assert(main_commits<entry_cases[main_case].commits);
    compare(renderer,last_pixel_frame,main_case,main_commits++,"main");
}
static int wait_outer(SDL_Event *event,int timeout){
    memset(event,0,sizeof(*event));
    if(!stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;++stage;return 1;}
    if(stage==1){assert(live->game_mode==1);NpeWorld saved=*live;NpeFile *file=(NpeFile *)live->battle_file;
#if !defined(NPE_ENCOUNTER_CONTROL) && !defined(NPE_BATTLE_SELECTION_ENTRY)
        add_chase_directory(file);
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
        add_trigger_directory(file);
#endif
        configure(file,live,main_case);
        /* The original walking sampler publishes this viewport and checks
           its105 resident words. Match that input after replacing World;
           retaining the new game's old cache would erase a different tile. */
        uint16_t cached[105];
        assert(npe_rpg_runtime_map_cache(file,live->map_number,live->view.offset_x,
                                        live->view.offset_y,cached)==1);
        live->sound_observer=saved.sound_observer;live->sound_range_observer=saved.sound_range_observer;live->sound_context=saved.sound_context;
        live->battle_animation_observer=saved.battle_animation_observer;live->battle_animation_context=saved.battle_animation_context;
        frame(main_case,-1,world_map_frame);world_map_frame_state=*live;world_map_frame_file=file;
        world_map_frame_renderer=last_pixel_renderer;world_map_frame_valid=1;world_map_frame_uncommitted=0;
        fake_clock=1234;active=1;++stage;event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_DOWN;return 1;
    }
    assert(stage==2);if(timeout>0&&timeout<=16){fake_clock+=(Uint32)timeout;return 0;}
#ifdef NPE_RECORD_REENTRY
    if(reentry_started){
        if(reentry_started==1){
            assert(reentry_checked==reentry_commits[main_case]);check_reentry_records(live,main_case,2);
#ifdef NPE_REENTRY_MIXED
            /* Only the original BEFORE-input diagnostic is seeded. */
            for(unsigned slot=0;slot<3;++slot)
                assert(!npe_world_record_write(live,0,slot,0,mixed_input_map[main_case][slot],148));
            for(unsigned slot=0;slot<6;++slot)
                assert(!npe_world_record_write(live,1,slot,0,mixed_input_battle[main_case][slot],148));
            for(unsigned id=0;id<255;++id)assert(!npe_world_set_inventory_word(live,id,id==mixed_inventory_item[main_case]?2:0));
            const uint8_t zero[144]={0};
            assert(!npe_world_resident_write(live,0x527a74,zero,1));
            assert(!npe_world_resident_write(live,0x527a72,zero,1));
            assert(!npe_world_resident_write(live,0x52730e,zero,144));
            assert(!npe_world_resident_write(live,0x52739e,zero,142));
#endif
#ifdef NPE_REENTRY_TWO_ATTACKS
            for(unsigned member=0;member<3;++member)
                assert(!npe_world_record_write(live,1,member,126,reentry_target_input[main_case]+member,1));
#endif
#ifdef NPE_BATTLE_MENU_RESIDENT
            assert(!npe_rpg_runtime_write(live->battle_file,0x527a9c,reentry_menu_input[main_case],1));
            assert(!npe_rpg_runtime_write(live->battle_file,0x527aa2,reentry_menu_input[main_case]+1,1));
#ifdef NPE_BATTLE_COMMAND_RESIDENT
            assert(!npe_rpg_runtime_write(live->battle_file,0x527a9e,reentry_command_input[main_case],1));
            assert(!npe_rpg_runtime_write(live->battle_file,0x527aae,reentry_command_input[main_case]+1,1));
#ifdef NPE_BATTLE_PHASE_RESIDENT
            assert(!npe_rpg_runtime_write(live->battle_file,0x527ac8,reentry_phase_input+main_case,1));
#endif
#endif
#endif
#ifdef NPE_REENTRY_ACTIONS
            reentry_started=2;reentry_action_step=reentry_action_commits=0;
#endif
        }
#ifdef NPE_REENTRY_ACTIONS
        if(reentry_action_step){
            assert(reentry_action_commits==reentry_actions[main_case][reentry_action_step-1].ui.commits);
            check_reentry_action_records(live,main_case,reentry_action_step-1);
        }
        if(reentry_action_step<reentry_action_steps[main_case]){
#ifdef NPE_BATTLE_ZERO_MENU
            if(ignored_battle_input(event))return 1;
#endif
            unsigned key=reentry_actions[main_case][reentry_action_step++].ui.key;reentry_action_commits=0;
            probe_battle_input(event,key,SDLK_RETURN);return 1;
        }
#endif
        active=0;++stage;event->type=SDL_QUIT;return 1;
    }
#endif
#ifdef NPE_BATTLE_ZERO_MENU
#ifdef NPE_PARTY_DESCRIPTOR_ENTRY
    if(!navigation_step&&!ignored_battle_pending&&!ignored_battle_index){records(live,main_case);
#ifdef NPE_EXIT_RECORDS
        mutate_exit_inputs(live,main_case);
#endif
#ifdef NPE_LIVE_COORDINATES
        mutate_live_coordinates(live,main_case);
#endif
    }
#endif
    if(navigation_step)assert(navigation_commits==entry_navigation[main_case][navigation_step-1].commits);
    if(navigation_step<NAVIGATION_STEPS){
        if(ignored_battle_input(event))return 1;
        const EntryNavigation *n=&entry_navigation[main_case][navigation_step++];navigation_commits=0;
        probe_battle_input(event,n->key,SDLK_RETURN);return 1;
    }
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
    if(main_case==0&&main_commits==entry_cases[main_case].commits&&menu_commits<8){
        event->type=SDL_KEYDOWN;event->key.keysym.sym=menu_keys[menu_commits]==2 ? SDLK_DOWN : SDLK_UP;return 1;
    }
#endif
#ifdef NPE_BATTLE_ZERO_FLEE
    verify_flee_records(live,main_case);
    for(unsigned i=0;i<3;++i)assert(live->hp[i]==flee_hp[main_case][i]);
    assert(main_commits==entry_cases[main_case].commits);
#else
#ifdef NPE_EXIT_RECORDS
    verify_exit_records(live,main_case);assert(main_commits==entry_cases[main_case].commits);
#ifdef NPE_RECORD_REENTRY
    configure_reentry(live,main_case);reentry_started=1;reentry_checked=0;
    event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_DOWN;return 1;
#endif
#else
#ifdef NPE_COORDINATE_TARGETS
    assert(live->game_mode==3&&main_commits==entry_cases[main_case].commits);
    for(unsigned m=0;m<entry_cases[main_case].party+3;++m)
        assert(!memcmp(live->battle_record_residue[m]+124,entry_coordinates[main_case][m],2));
#else
    records(live,main_case);assert(main_commits==entry_cases[main_case].commits&&fake_clock==1298&&timeout==3936);
#endif
#endif
#endif
    active=0;++stage;event->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
#if !defined(NPE_ENCOUNTER_CONTROL) && !defined(NPE_BATTLE_SELECTION_ENTRY)
    add_chase_directory(&file);
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
    add_trigger_directory(&file);
#endif
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
#ifdef NPE_EXIT_RECORDS
    assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
#endif
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("battle entry",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    unsigned checked=0;
    unsigned direct_cases=(unsigned)(sizeof(entry_cases)/sizeof(*entry_cases));
#ifdef NPE_REENTRY_ATTACK
    /* This extension verifies the actual SDL main loop, including its
       nested target selector. The older direct adapter only selects defense. */
    direct_cases=0;
#endif
    for(unsigned i=0;i<direct_cases;++i){
        NpeWorld numeric,world;uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];configure(&file,&numeric,i);
#if defined(NPE_ENCOUNTER_CONTROL) || defined(NPE_BATTLE_SELECTION_ENTRY)
        /* Closing an owner captures its current terrain for restoration.
           Capture before cloning the input; otherwise the later numeric
           shutdown adds a snapshot that the display run never received. */
        uint16_t numeric_input_flags=numeric.battle_resource_flags;
        npe_rpg_runtime_close(&file);
        numeric.battle_resource_flags=numeric_input_flags;
        assert(!npe_world_resources_open(&file,&numeric));
#endif
        world=numeric;
        int moved=npe_world_step(&file,&numeric,0);assert(moved>=0);records(&numeric,i);frame(i,-1,pixels);
#if defined(NPE_ENCOUNTER_CONTROL) || defined(NPE_BATTLE_SELECTION_ENTRY)
        /* Compare the two entry results before either run's shutdown.
           Closing the numeric run now faithfully clears its resource flag. */
        uint16_t numeric_entry_flags=numeric.battle_resource_flags;
        npe_rpg_runtime_close(&file);
        assert(numeric.battle_resource_flags==(numeric_entry_flags>>8==1?
               (numeric_entry_flags&255u):numeric_entry_flags));
        numeric.battle_resource_flags=numeric_entry_flags;
        assert(!npe_world_resources_open(&file,&world));
#ifdef NPE_PARTY_DESCRIPTOR_ENTRY
        configure_voice_cache(&file,i);
#endif
#endif
        Comparison c={renderer,texture,i,0};assert(npe_world_step_with_display(&file,&world,0,pixels,commit,&c)==moved);
        if(memcmp(&world,&numeric,sizeof(world))){
            const uint8_t *actual=(const uint8_t *)&world,*expected=(const uint8_t *)&numeric;
            unsigned shown=0;
            for(size_t offset=0;offset<sizeof(world)&&shown<16;++offset)
                if(actual[offset]!=expected[offset]){
                    fprintf(stderr,"entry state byte%zu display=%02x numeric=%02x\n",offset,actual[offset],expected[offset]);
                    ++shown;
                }
        }
        assert(!memcmp(&world,&numeric,sizeof(world)));
        BattleAnimationDisplay entry_display={0};
        entry_display.renderer=renderer;entry_display.texture=texture;entry_display.font=&font;
        world.battle_animation_observer=play_battle_animation;world.battle_animation_context=&entry_display;
        if(world.game_mode==3)for(unsigned j=0;j<4;++j){assert(!npe_render_battle_entry_frame(&file,&font,&world,j,pixels));assert(!commit(&c,pixels,0));retain_battle_entry(&world,renderer,j,pixels);}
#ifdef NPE_LIVE_COORDINATES
        mutate_live_coordinates(&world,i);
#endif
#ifdef NPE_EXIT_RECORDS
        mutate_exit_inputs(&world,i);
#endif
#ifdef NPE_BATTLE_ZERO_MENU
        for(unsigned step=0;step<NAVIGATION_STEPS;++step){const EntryNavigation *n=&entry_navigation[i][step];
#ifdef NPE_BATTLE_PHASE_RESIDENT
            (void)battle_menu_navigation(&world,n->key);
#endif
            if(!n->commits)continue;
#ifdef NPE_EXIT_RECORDS
            if(step+1==NAVIGATION_STEPS){
                main_case=i;main_commits=entry_cases[i].commits;navigation_step=step+1;navigation_commits=0;
                active=direct_navigation=1;
                int result=exit_result[i]==3?npe_world_battle_flee(&world):npe_world_battle_defend(&world);
                assert(result==(exit_result[i]==1?NPE_BATTLE_WON:exit_result[i]==3?NPE_BATTLE_FLED:NPE_BATTLE_LOST));
                assert(!advance_world_events(NULL,renderer,texture,&file,&font,&world,3,0));
                if(world.game_mode==7){
                    assert(!npe_render_native_title_screen(&file,&font,0,pixels));draw_pixels(renderer,texture,pixels);
                }else{
                    unsigned levels[3]={0};assert(!draw_world(renderer,texture,&file,&world,0,0,0,0,0,0,levels,0,&font));
                }
                assert(navigation_commits==n->commits);
                verify_exit_records(&world,i);
                active=direct_navigation=0;main_commits=0;
#ifdef NPE_RECORD_REENTRY
                configure_reentry(&world,i);reentry_checked=0;
                memcpy(pixels,last_pixel_frame,sizeof(pixels));
                assert(npe_world_step_with_display(&file,&world,0,pixels,reentry_commit,&c)>=0);
                for(unsigned j=0;j<4;++j){
                    assert(!npe_render_battle_entry_frame(&file,&font,&world,j,pixels));
                    draw_pixels(renderer,texture,pixels);compare_reentry(renderer,i);retain_battle_entry(&world,renderer,j,pixels);
                }
                assert(reentry_checked==reentry_commits[i]);check_reentry_records(&world,i,2);
#if defined(NPE_REENTRY_ACTIONS) && !defined(NPE_REENTRY_ATTACK)
#ifdef NPE_BATTLE_MENU_RESIDENT
                assert(!npe_rpg_runtime_write(&file,0x527a9c,reentry_menu_input[i],1));
                assert(!npe_rpg_runtime_write(&file,0x527aa2,reentry_menu_input[i]+1,1));
#ifdef NPE_BATTLE_COMMAND_RESIDENT
                assert(!npe_rpg_runtime_write(&file,0x527a9e,reentry_command_input[i],1));
                assert(!npe_rpg_runtime_write(&file,0x527aae,reentry_command_input[i]+1,1));
#ifdef NPE_BATTLE_PHASE_RESIDENT
                assert(!npe_rpg_runtime_write(&file,0x527ac8,reentry_phase_input+i,1));
#endif
#endif
                unsigned previous_level=0;
#endif
                for(unsigned step=0;step<reentry_action_steps[i];++step){
                    const ReentryAction *n=&reentry_actions[i][step];
                    main_case=i;active=direct_navigation=1;reentry_started=2;reentry_action_step=step+1;reentry_action_commits=0;
#ifndef NPE_BATTLE_PHASE_RESIDENT
                    world.battle_menu_phase=(uint8_t)(battle_phase_for_icon(n->ui.icon)+n->ui.level);
#endif
#ifdef NPE_BATTLE_MENU_RESIDENT
#ifdef NPE_BATTLE_PHASE_RESIDENT
                    previous_level=battle_menu_level(&world);
                    (void)battle_menu_navigation(&world,n->ui.key);
#else
                    if(previous_level && (n->ui.key==1 || n->ui.key==2))
                        move_battle_list_row(&world,previous_level==2,n->ui.key==2);
#endif
                    if(previous_level==1 && n->ui.key==64)
                        for(unsigned stage=0;stage<2;++stage)
                            assert(!draw_battle_command_return_stage(renderer,texture,&file,&font,&world,n->ui.icon,stage));
#endif
                    if(n->execute)assert(npe_world_battle_defend(&world)==NPE_BATTLE_CONTINUE);
                    if(n->execute || reentry_action_commits<n->ui.commits)
#ifdef NPE_BATTLE_PHASE_RESIDENT
                        assert(!draw_battle_scene(renderer,texture,&file,world.battle_enemy_id,&world,&font,(unsigned)battle_menu_icon(&world),battle_menu_level(&world),world.battle_menu_row,world.battle_item_row));
#else
                        assert(!draw_battle_scene(renderer,texture,&file,world.battle_enemy_id,&world,&font,n->ui.icon,n->ui.level,n->ui.selection,n->ui.item));
#endif
                    assert(reentry_action_commits==n->ui.commits);check_reentry_action_records(&world,i,step);
#ifdef NPE_BATTLE_MENU_RESIDENT
                    previous_level=n->ui.level;
#endif
                    active=direct_navigation=reentry_started=0;
                }
#endif
#endif
                continue;
            }
#endif
#ifdef NPE_BATTLE_ZERO_FLEE
            if(step==15){
                BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
                world.battle_animation_observer=play_battle_animation;world.battle_animation_context=&display;
                main_case=i;main_commits=entry_cases[i].commits;navigation_step=step+1;navigation_commits=0;
                active=direct_navigation=1;
                assert(npe_world_battle_flee(&world)==NPE_BATTLE_FLED);
                fprintf(stderr,"flee animation commits=%u pending=%u\n",navigation_commits,world.map_init_pending);
                assert(world.game_mode==1&&!world.battle_party_flags_saved&&world.random_state==flee_rng[i]);
                assert(!advance_world_events(NULL,renderer,texture,&file,&font,&world,3,0));
                fprintf(stderr,"flee dispatch commits=%u\n",navigation_commits);
                unsigned levels[3]={0};
                assert(!draw_world(renderer,texture,&file,&world,0,0,0,0,0,0,levels,0,&font));
                assert(navigation_commits==n->commits);
                verify_flee_records(&world,i);
                active=direct_navigation=0;main_commits=0;
                world.battle_animation_observer=NULL;world.battle_animation_context=NULL;
                continue;
            }
#endif
#ifdef NPE_COORDINATE_TARGETS
            uint8_t target_pixels[NPE_WIDTH*NPE_HEIGHT];
            main_case=i;main_commits=entry_cases[i].commits;navigation_step=step+1;navigation_commits=0;
            active=direct_navigation=1;
            if(n->key==64){
                retain_battle_enemy_pointer_clear(&world,world.battle_target);
                assert(!draw_battle_scene(renderer,texture,&file,world.battle_enemy_id,&world,&font,1,0,0,0));
            }else if(n->key==16){
                world.battle_target=(uint8_t)(world.battle_enemy_count-1);
                assert(npe_world_battle_cycle_target(&world,1)>=0);
                assert(npe_world_battle_select_attack(&world,world.battle_target)>=0);
                assert(world.battle_target==n->target);
                memcpy(target_pixels,last_pixel_frame,sizeof(target_pixels));
                assert(!npe_draw_battle_enemy_target_pointer(&file,&world,world.battle_target,target_pixels));
                draw_pixels(renderer,texture,target_pixels);
            }else{
                memcpy(target_pixels,last_pixel_frame,sizeof(target_pixels));
                unsigned previous=world.battle_target;
                retain_battle_enemy_pointer_clear(&world,previous);
                assert(!npe_clear_battle_enemy_target(&world,previous,target_pixels));
                draw_pixels(renderer,texture,target_pixels);
                assert(npe_world_battle_cycle_target(&world,n->key==1?-1:1)>=0);
                assert(world.battle_target==n->target);
                assert(!npe_update_battle_enemy_target(&file,&world,previous,target_pixels));
                draw_pixels(renderer,texture,target_pixels);
            }
            assert(navigation_commits==n->commits);
            active=direct_navigation=0;main_commits=0;
#else
            if(n->commits==2){
                for(unsigned stage=0;stage<2;++stage){
                    assert(!draw_battle_command_return_stage(renderer,texture,&file,&font,&world,n->icon,stage));
                    compare_navigation(renderer,i,step,stage,"direct");
                }
            }else{
                assert(n->commits==1);
                assert(!draw_battle_scene(renderer,texture,&file,world.battle_enemy_id,&world,&font,n->icon,n->level,n->selection,n->item));
                compare_navigation(renderer,i,step,0,"direct");
            }
#endif
        }
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
        if(c.commits<entry_cases[i].commits){
            assert(entry_cases[i].commits==c.commits+1);unsigned levels[3]={0};
            if(world.game_mode==1)assert(!npe_render_world_load(&file,&world,pixels));
            memcpy(world_map_frame,world.game_mode==6 ? last_pixel_frame : pixels,sizeof(pixels));
            world_map_frame_state=world;
            world_map_frame_file=&file;world_map_frame_renderer=renderer;world_map_frame_valid=1;world_map_frame_uncommitted=0;
            assert(!draw_world(renderer,texture,&file,&world,0,0,0,0,0,0,levels,0,&font));
            compare(renderer,last_pixel_frame,i,c.commits++,"direct");
        }
        if(i==0){unsigned levels[3]={0};
            for(unsigned j=0;j<8;++j){
                assert(!draw_world(renderer,texture,&file,&world,menu_selections[j],0,0,0,0,0,levels,0,&font));
                compare_menu(renderer,j,"direct");}
        }
#endif
        assert(c.commits==entry_cases[i].commits);checked+=c.commits;
#if defined(NPE_ENCOUNTER_CONTROL) || defined(NPE_BATTLE_SELECTION_ENTRY)
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
#endif
    }
    fprintf(stderr,"Direct original walking/battle commits=%u\n",checked);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_font_free(&font);npe_free(&file);
    unsigned main_checked=0;
#ifdef NPE_PARTY_DESCRIPTOR_ENTRY
    entry_record_route="main";
#endif
    for(main_case=0;main_case<sizeof(entry_cases)/sizeof(*entry_cases);++main_case){
#ifdef NPE_BATTLE_ZERO_MENU
        navigation_step=navigation_commits=0;
#endif
#ifdef NPE_WALK_TRIGGER_MATRIX
        menu_commits=0;
#endif
#ifdef NPE_RECORD_REENTRY
        reentry_started=reentry_checked=0;
#endif
        fake_clock=stage=main_commits=0;active=0;char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==3);
        main_checked+=main_commits;
    }
    fprintf(stderr,"Main original walking/battle commits=%u, zero differences\n",main_checked);
#ifdef NPE_BATTLE_ZERO_MENU
    if(getenv("NPE_PROBE_BATTLE_IGNORED_KEYS"))fprintf(stderr,"Ignored battle inputs:%u, no World change or LCD submit\n",ignored_battle_total);
#endif
    return 0;
}
