#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"

static unsigned scenario, attacks, throws, picked_target, picked_item, popups;
static unsigned picker_entries, initial_targets[4], list_visits, last_selection;
static uint16_t quantities_after[2], first_enemy_after;
static unsigned last_menu, previous_calls;
static SDL_atomic_t ready;
static int probe_new_game(const NpeFile *, NpeWorld *);
static int probe_attack(NpeWorld *, unsigned);
static int probe_previous(NpeWorld *);
static int probe_throw(const NpeFile *, NpeWorld *, unsigned, unsigned);
static int probe_menu(const NpeFile *, unsigned, unsigned, uint8_t *);
static int probe_picker(const NpeFile *, const NpeWorld *, uint8_t *);
static int probe_pointer(const NpeFile *,const NpeWorld *,unsigned,uint8_t *);
static int probe_actors(const NpeFile *, const NpeWorld *, uint8_t *);
static int probe_battle_list(const NpeFile *,const NpeFont *,NpeWorld *,const unsigned *,const unsigned *,unsigned,unsigned,unsigned,int,int,uint8_t *);
static int probe_list(const NpeFile *, const NpeFont *, const unsigned *, const unsigned *, unsigned, unsigned, uint8_t *);
static int probe_message(Uint32 flags, const char *title, const char *message, SDL_Window *window) {
    (void)flags;(void)title;(void)message;(void)window;++popups;return 0;
}
static int probe_wait(SDL_Event *event, int timeout) {
    SDL_AtomicSet(&ready, 1);
    return SDL_WaitEventTimeout(event, timeout);
}
#define npe_world_new_game probe_new_game
#define npe_world_restart_game probe_new_game
#define npe_world_battle_attack probe_attack
#define npe_world_battle_previous_actor probe_previous
#define npe_world_battle_throw_item probe_throw
#define npe_render_battle_enemy_target probe_picker
#define npe_draw_battle_enemy_target_pointer probe_pointer
#define npe_render_battle_actors_scene probe_actors
#define npe_render_battle_item_list probe_battle_list
#define npe_render_item_list_screen probe_list
#define npe_render_battle_command_menu probe_menu
#define SDL_ShowSimpleMessageBox probe_message
#define SDL_WaitEventTimeout probe_wait
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_ShowSimpleMessageBox
#undef npe_render_battle_command_menu
#undef npe_render_item_list_screen
#undef npe_render_battle_enemy_target
#undef npe_draw_battle_enemy_target_pointer
#undef npe_render_battle_actors_scene
#undef npe_render_battle_item_list
#undef npe_world_battle_throw_item
#undef npe_world_battle_previous_actor
#undef npe_world_battle_attack
#undef npe_world_new_game
#undef npe_world_restart_game

static int probe_new_game(const NpeFile *file, NpeWorld *world) {
    if (npe_world_init(file, world, 9, 29, 13)) return -1;
    world->game_mode = 3;world->story_event_id = 1;
    /* Enter with the intended party so physical records and coordinates
       are initialized by the same production entry as the real game. */
    if(scenario>=7){
        unsigned party=scenario==9?3:2;
        for(unsigned i=1;i<party;++i){
            world->party_active[i]=1;world->party_ids[i]=1;
            world->party_characters[i]=world->party_characters[0];
            memcpy(world->party_names[i],world->party_names[0],12);
        }
    }
    if (npe_world_prepare_scripted_battle(file,world) != 1) return -1;
    world->hp[0]=10000;world->max_hp[0]=30000;
    world->attack[0]=60;world->other_stats[0][1]=999;
    /* Isolated fixture: an additional intact NPE enemy51 plus two original
       throw items. These changes are not a continuous story replay save. */
    if (scenario!=2 && scenario!=6) {
#define COPY_ENEMY(field) world->field[0]=world->field[1]
        COPY_ENEMY(battle_enemy_ids);COPY_ENEMY(battle_enemy_hps);
        COPY_ENEMY(battle_enemy_max_hps);COPY_ENEMY(battle_enemy_attacks);
        COPY_ENEMY(battle_enemy_defenses);COPY_ENEMY(battle_enemy_speeds); COPY_ENEMY(battle_enemy_lucks);
        COPY_ENEMY(battle_enemy_damage_floor_stats);COPY_ENEMY(battle_enemy_spell_stats);
#undef COPY_ENEMY
        memcpy(world->battle_enemy_skill_ids[0],world->battle_enemy_skill_ids[1],3);
        unsigned party=world->battle_record_party_count;
        memcpy(world->battle_record_residue[party]+46,
               world->battle_record_residue[party+1]+46,2);
    }
    world->condition_words[159]=world->condition_words[160]=1;
    if (scenario>=7) {
        unsigned party=scenario==9?3:2;
        for(unsigned i=1;i<party;++i) {
            world->party_active[i]=1;world->hp[i]=10000;world->max_hp[i]=30000;
            world->other_stats[i][1]=999;
        }
        world->condition_words[159]=scenario==8?1:party;
        world->condition_words[160]=0;
    }
    /* Entry must reset a previous target and find the first living slot. */
    world->battle_target=1;
    quantities_after[0]=world->condition_words[159];quantities_after[1]=world->condition_words[160];first_enemy_after=world->battle_enemy_hps[0];
    npe_world_seed(world,1);
    return 0;
}
static int probe_menu(const NpeFile *file,unsigned menu,unsigned selection,uint8_t *pixels) {
    last_menu=menu;
    return npe_render_battle_command_menu(file,menu,selection,pixels);
}
static void record_picker(const NpeWorld *world) {
    if (scenario>=7) {
        unsigned initial=scenario==8?1:scenario==9?3:2;
        assert(world->condition_words[159]==initial-picker_entries-1 &&
            world->battle_item_reservations[world->battle_actor]==159);
    } else if (scenario>=3) assert(world->condition_words[160]==0 &&
        world->battle_item_reservations[world->battle_actor]==160);
    unsigned actor=world->battle_actor;
    unsigned target=world->battle_commands[actor].kind==3?
        world->battle_commands[actor].target:world->battle_target;
    unsigned party_count=0;
    for(unsigned i=0;i<3;++i)party_count+=world->party_active[i]!=0;
    assert(world->battle_native_targets[actor]==party_count+target+1);
    assert(picker_entries<4);initial_targets[picker_entries++]=target;
}
static int probe_picker(const NpeFile *file, const NpeWorld *world, uint8_t *pixels) {
    record_picker(world);
    return npe_render_battle_enemy_target(file,world,pixels);
}
static int probe_pointer(const NpeFile *file,const NpeWorld *world,unsigned target,uint8_t *pixels){
    /* The ordinary selector overlays its first arrow on the retained LCD.
       Item selectors already record their entry through probe_actors. */
    if(world->battle_menu_phase==17)record_picker(world);
    return npe_draw_battle_enemy_target_pointer(file,world,target,pixels);
}
static int probe_actors(const NpeFile *file,const NpeWorld *world,uint8_t *pixels) {
    /* Current item selector enters through the native actor scene. Action
       animation uses the same renderer, but is outside the selector. */
    if(!world->battle_executing && world->battle_actor<3 &&
       world->battle_commands[world->battle_actor].kind==3 &&
       world->battle_item_reservations[world->battle_actor])record_picker(world);
    return npe_render_battle_actors_scene(file,world,pixels);
}
static void record_list(const unsigned *ids,const unsigned *quantities,unsigned count,unsigned selection) {
    if(scenario>=7) assert(count==1 && ids[0]==159 && quantities[0]>=1);
    else {
        assert(count==2 && ids[0]==159 && ids[1]==160);
        assert(quantities[0]==1 && quantities[1]==1);
    }
    ++list_visits;last_selection=selection;
}
static int probe_battle_list(const NpeFile *file,const NpeFont *font,NpeWorld *world,
    const unsigned *ids,const unsigned *quantities,unsigned count,unsigned selection,
    unsigned previous,int full,int empty_return,uint8_t *pixels) {
    record_list(ids,quantities,count,selection);
    return npe_render_battle_item_list(file,font,world,ids,quantities,count,selection,
        previous,full,empty_return,pixels);
}
static int probe_list(const NpeFile *file,const NpeFont *font,const unsigned *ids,
    const unsigned *quantities,unsigned count,unsigned selection,uint8_t *pixels) {
    record_list(ids,quantities,count,selection);
    return npe_render_item_list_screen(file,font,ids,quantities,count,selection,pixels);
}
static int probe_attack(NpeWorld *world,unsigned target) {
    ++attacks;picked_target=target;
    int result=npe_world_battle_attack(world,target);
    first_enemy_after=world->battle_enemy_hps[0];
    assert(result==NPE_BATTLE_CONTINUE && world->battle_enemy_hps[1]<=500);
    return result;
}
static int probe_previous(NpeWorld *world) {
    ++previous_calls;
    int result=npe_world_battle_previous_actor(world);
    quantities_after[0]=world->condition_words[159];quantities_after[1]=world->condition_words[160];
    if(scenario==8) assert(result==1 && world->battle_actor==0 &&
        !world->battle_pending_mask && !world->battle_item_reservations[0]);
    return result;
}
static int probe_throw(const NpeFile *file,NpeWorld *world,unsigned item,unsigned target) {
    ++throws;picked_item=item;picked_target=target;
    int result=npe_world_battle_throw_item(file,world,item,target);
    quantities_after[0]=world->condition_words[159];quantities_after[1]=world->condition_words[160];
    first_enemy_after=world->battle_enemy_hps[0];
    assert(result==NPE_BATTLE_CONTINUE);
    if(scenario<7) {
        /* The app seeds from the host performance counter after new-game.
           Native enemy51 can heal the selected enemy back to its maximum.
           Item160's exact60 HP effect has an independent isolated probe. */
        assert(world->battle_enemy_hps[1]<=500);
        if(world->battle_enemy_hps[1]==500) {
            int healed=0;
            for(unsigned enemy=0;enemy<world->battle_enemy_count;++enemy) {
                NpeSkill spell;unsigned skill=world->battle_enemy_last_skill[enemy];
                if(skill && !npe_skill(file,skill-1,&spell) && spell.raw[14]==1 &&
                   spell.raw[38]==1 && spell.hp_effect &&
                   (spell.raw[37] || world->battle_enemy_last_target[enemy]==4))healed=1;
            }
            assert(healed);
        }
    }
    else if(scenario!=8 && throws==(scenario==9?3u:2u)) {
        assert(!world->battle_pending_mask && world->battle_enemy_hps[1]<=500);
        for(unsigned i=0;i<3;++i) assert(!world->battle_item_reservations[i]);
    } else assert(world->battle_enemy_hps[1]==500);
    return result;
}
static int send_keys(void *unused) {
    SDL_Keycode keys[40];unsigned count=0,waited=0;
    (void)unused;
#define KEY(k) keys[count++]=(k)
    KEY(SDLK_RETURN); /* title */
    if(scenario>=7) {
        KEY(SDLK_LEFT);KEY(SDLK_DOWN);KEY(SDLK_RETURN); /* flag */
        KEY(SDLK_DOWN);KEY(SDLK_RETURN); /* items */
        KEY(SDLK_DOWN);KEY(SDLK_RETURN); /* throw list */
        KEY(SDLK_RETURN);KEY(SDLK_DOWN);KEY(SDLK_RETURN); /* reserve -> target -> queue */
        if(scenario==8) KEY(SDLK_F4); /* undo previous actor's pending throw */
        else for(unsigned i=1;i<(scenario==9?3u:2u);++i) {
            KEY(SDLK_RETURN);KEY(SDLK_RETURN);KEY(SDLK_RETURN); /* retained flag/items/throw */
            KEY(SDLK_RETURN);KEY(SDLK_DOWN);KEY(SDLK_RETURN);
        }
    } else if(scenario<3) {
        KEY(SDLK_RETURN); /* sword */
        if(scenario==0) {
            KEY(SDLK_RIGHT);KEY(SDLK_UP);KEY(SDLK_ESCAPE);KEY(SDLK_RETURN);
        } else if(scenario==1) KEY(SDLK_F4);
    } else {
        KEY(SDLK_LEFT);KEY(SDLK_DOWN);KEY(SDLK_RETURN); /* flag */
        KEY(SDLK_DOWN);KEY(SDLK_RETURN); /* items */
        KEY(SDLK_DOWN);KEY(SDLK_RETURN); /* throw list */
        KEY(SDLK_RIGHT);KEY(SDLK_RETURN); /* original item160 */
        if(scenario!=6) {
            KEY(SDLK_DOWN);
            if(scenario==4 || scenario==5) {
                KEY(SDLK_F4); /* return with second item selected */
                if(scenario==5) KEY(SDLK_F4);
                else {
                    KEY(SDLK_RETURN);KEY(SDLK_DOWN);KEY(SDLK_RETURN);
                }
            } else {
                KEY(SDLK_RIGHT);KEY(SDLK_ESCAPE);KEY(SDLK_RETURN);
            }
        }
    }
#undef KEY
    while(!SDL_AtomicGet(&ready) && waited<5000) {SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&ready)) return -1;
    for(unsigned i=0;i<count;++i) {
        SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=keys[i];
        if(SDL_PushEvent(&event)!=1) return -1;
        SDL_Delay(70);
    }
    SDL_Event event={0};event.type=SDL_QUIT;
    return SDL_PushEvent(&event)==1?0:-1;
}
int main(int argc,char **argv) {
    char *arguments[]={"npe-sdl",NULL,NULL};
    assert(argc==2);arguments[1]=argv[1];
    assert(SDL_setenv("SDL_VIDEODRIVER","dummy",1)==0);
    assert(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0);
    for(scenario=0;scenario<10;++scenario) {
        attacks=throws=popups=picker_entries=list_visits=last_selection=last_menu=previous_calls=0;
        SDL_AtomicSet(&ready,0);int thread_result=-1;
        SDL_Thread *thread=SDL_CreateThread(send_keys,"target-keys",NULL);
        assert(thread && run(2,arguments)==0);
        SDL_WaitThread(thread,&thread_result);
        assert(thread_result==0 && popups==0);
        if(scenario>=7) {
            unsigned party=scenario==9?3:2;
            assert(!attacks && throws==(scenario==8?1:party) && picked_item==159 && picked_target==1);
            assert(quantities_after[0]==(scenario==8?1:0) && quantities_after[1]==0);
            assert(previous_calls==(scenario==8?1:0));
            assert(first_enemy_after==500 && picker_entries==(scenario==8?1:party));
            assert(list_visits>=(scenario==8?1:party) && last_selection==0);
        } else if(scenario==1 || scenario==5) {
            assert(!attacks && !throws && quantities_after[0]==1 && quantities_after[1]==1);
        } else if(scenario<3) {
            assert(attacks==1 && !throws && picked_target==1);
        } else {
            assert(!attacks && throws==1 && picked_target==1 && picked_item==160);
            assert(quantities_after[0]==1 && quantities_after[1]==0);
        }
        assert(first_enemy_after==((scenario==2 || scenario==6)?0:500));
        if(scenario<7) assert(picker_entries==((scenario==2 || scenario==6)?0:scenario==4?2:1));
        for(unsigned i=0;i<picker_entries;++i) assert(initial_targets[i]==0);
        if(scenario>=3 && scenario<7) assert(list_visits>=2 && last_selection==1);
        if(scenario==5) assert(last_menu==2);
        fprintf(stderr,"SDL ordinary/throw scenario%u: passed (attacks%u throws%u pickers%u)\n",
            scenario,attacks,throws,picker_entries);
    }
    return 0;
}
