#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"
#include "buff_timers_native.h"

static unsigned scenario, picker_count, targets[32], uses, throws, prompts;
static SDL_atomic_t ready;
static NpeWorld *observed;
static int probe_picker(const NpeFile *,const NpeWorld *,unsigned,uint8_t *);
static int probe_use(const NpeFile *,NpeWorld *,unsigned,unsigned);
static int probe_throw(const NpeFile *,NpeWorld *,unsigned,unsigned);
static int probe_prompt(const NpeFont *,int,uint8_t *);
static int probe_wait(SDL_Event *event) {
    SDL_AtomicSet(&ready,1);
    return SDL_WaitEvent(event);
}
#define npe_draw_battle_party_target_pointer probe_picker
#define npe_world_battle_use_item probe_use
#define npe_world_battle_throw_item probe_throw
#define npe_render_battle_item_rejection probe_prompt
#define SDL_WaitEvent probe_wait
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef npe_render_battle_item_rejection
#undef npe_world_battle_throw_item
#undef npe_world_battle_use_item
#undef npe_draw_battle_party_target_pointer

static int probe_picker(const NpeFile *file,const NpeWorld *world,unsigned target,uint8_t *pixels) {
    unsigned actor=world->battle_actor,item=world->battle_item_reservations[actor];
    assert(item && world->condition_words[item]==(scenario==1 && !actor ? 1 : 0));
    assert(picker_count<32); targets[picker_count++]=target;
    return npe_draw_battle_party_target_pointer(file,world,target,pixels);
}
static int probe_use(const NpeFile *file,NpeWorld *world,unsigned id,unsigned target) {
    ++uses;
    assert(world->battle_item_reservations[world->battle_actor]==id);
    if(scenario==1) assert(target==1 && world->hp[0]==200 && world->hp[1]==210);
    if(scenario==2) assert(target==2 && world->hp[2]==0);
    return npe_world_battle_use_item(file,world,id,target);
}
static int probe_throw(const NpeFile *file,NpeWorld *world,unsigned id,unsigned target) {
    ++throws;
    assert(world->battle_item_reservations[world->battle_actor]==id && target==1);
    return npe_world_battle_throw_item(file,world,id,target);
}
static int probe_prompt(const NpeFont *font,int throwing,uint8_t *pixels) {
    ++prompts;
    assert(!observed->battle_item_reservations[0] && observed->battle_actor==0);
    assert(throwing==(scenario==8));
    return npe_render_battle_item_rejection(font,throwing,pixels);
}
static int keys(void *unused) {
    (void)unused;
    const SDL_Keycode cancel[]={SDLK_RETURN,SDLK_DOWN,SDLK_UP,SDLK_DOWN,
        SDLK_RIGHT,SDLK_LEFT,SDLK_ESCAPE,SDLK_F4,SDLK_RETURN,SDLK_F4,SDLK_F4};
    const SDL_Keycode settle[]={SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN};
    const SDL_Keycode fallen[]={SDLK_RETURN,SDLK_DOWN,SDLK_RETURN};
    const SDL_Keycode reject[]={SDLK_RETURN,SDLK_F4};
    const SDL_Keycode mixed[]={SDLK_RETURN,SDLK_RIGHT,SDLK_RETURN,SDLK_F4,SDLK_F4};
    const SDL_Keycode single[]={SDLK_RETURN};
    const SDL_Keycode empty_cancel[]={SDLK_RETURN,SDLK_F4,SDLK_RETURN,SDLK_F4,SDLK_F4};
    const SDL_Keycode empty_three[]={SDLK_RETURN,SDLK_RETURN,SDLK_RETURN};
    const SDL_Keycode *sequence=single; unsigned count=1;
    if(scenario==0) {sequence=cancel;count=sizeof(cancel)/sizeof(*cancel);}
    if(scenario==1) {sequence=settle;count=sizeof(settle)/sizeof(*settle);}
    if(scenario==2) {sequence=fallen;count=sizeof(fallen)/sizeof(*fallen);}
    if(scenario==6 || scenario==8) {sequence=reject;count=2;}
    if(scenario==7) {sequence=mixed;count=5;}
    if(scenario==11) {sequence=empty_cancel;count=5;}
    if(scenario==13) {sequence=reject;count=2;}
    if(scenario==14) {sequence=empty_three;count=3;}
    unsigned waited=0;
    while(!SDL_AtomicGet(&ready) && waited<5000) {SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&ready)) return -1;
    for(unsigned i=0;i<count;++i) {
        SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=sequence[i];
        if(SDL_PushEvent(&event)!=1) return -1;
        SDL_Delay(i==0 && (scenario==6 || scenario==7 || scenario==8 || scenario==13) ? 2200 : 50);
    }
    return 0;
}
static NpeWorld fixture(const NpeFile *file,unsigned party) {
    NpeWorld world;
    assert(npe_world_init(file,&world,9,29,13)==0);
    /* Set the map party before entry snapshots its physical record prefix.
       Changing only party_active afterward leaves7AFA and the enemy slots
       describing a one-person battle while the picker sees more members. */
    for(unsigned i=0;i<3;++i) {
        world.party_active[i]=(uint8_t)(i<party);
        if(i<party) {
            world.party_ids[i]=1;
            world.party_characters[i]=world.party_characters[0];
            memcpy(world.party_names[i],world.party_names[0],12);
        }
    }
    world.game_mode=3;world.story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,&world)==1);
    assert(world.battle_record_party_count==party);
    memset(world.condition_words,0,sizeof(world.condition_words));
    for(unsigned i=0;i<party;++i) {
        world.party_active[i]=1;world.party_ids[i]=1;world.party_characters[i]=world.party_characters[0];memcpy(world.party_names[i],world.party_names[0],12);memcpy(world.battle_names[i],world.party_names[0],12);world.battle_characters[i]=world.party_characters[0];
        world.hp[i]=200+10*i;world.max_hp[i]=world.max_qi[i]=999;world.qi[i]=20+10*i;
        world.attack[i]=15;world.defense[i]=world.battle_party_defenses[i]=13;
        world.other_stats[i][1]=999;
    }
    npe_world_seed(&world,1);
    return world;
}

/* The native item effects below are isolated from enemy HP damage so that
   original effect values can be compared without equating host RNG seeds. */
static void effects(const NpeFile *file) {
    const unsigned ids[]={46,55,61,62,63,65,69,71};
    for(unsigned k=0;k<sizeof(ids)/sizeof(*ids);++k) {
        unsigned id=ids[k];NpeWorld world=fixture(file,2);
        world.condition_words[id]=1;
        world.battle_enemy_status[1][1]=-1;
        if(id==65) world.hp[1]=0;
        assert(npe_world_battle_use_item(file,&world,id,0)==NPE_BATTLE_CONTINUE);
        if(id!=65) assert(npe_world_battle_defend(&world)==NPE_BATTLE_CONTINUE);
        assert(!world.condition_words[id] && !world.battle_pending_mask);
        assert(world.qi[0]==(id==55?80:20) && world.qi[1]==30);
        if(id==46) assert(world.hp[0]==240 && world.hp[1]==210);
        if(id==61) assert(world.hp[0]==400 && world.hp[1]==410);
        if(id==62) assert(world.battle_skill_attack_bonus[0]==100 && world.battle_skill_attack_bonus[1]==100);
        if(id==63) assert(world.battle_skill_defense_bonus[0]==100);
        if(id==65) assert(world.hp[0]==200 && world.hp[1]==999);
        if(id==69) assert(world.battle_party_status[0][1]==5);
        if(id==71) assert(world.battle_party_status[0][2]==0 && world.battle_party_status[0][3]==5);
    }
    NpeWorld world=fixture(file,3);
    world.hp[2]=0;world.condition_words[46]=1;world.battle_enemy_status[1][1]=-1;
    assert(npe_world_battle_use_item(file,&world,46,2)==NPE_BATTLE_CONTINUE);
    assert(npe_world_battle_defend(&world)==NPE_BATTLE_CONTINUE);
    assert(world.hp[0]==240 && world.hp[1]==210 && world.hp[2]==0 && !world.condition_words[46]);
    world=fixture(file,2);world.condition_words[46]=1;
    assert(npe_world_battle_use_item(file,&world,46,0)==NPE_BATTLE_CONTINUE);
    assert(world.battle_actor==1 && !world.condition_words[46]);
    assert(npe_world_battle_previous_actor(&world)==1);
    assert(world.battle_actor==0 && world.condition_words[46]==1 && !world.battle_item_reservations[0]);
    const unsigned throw_ids[]={27,68,159,160};
    for(unsigned k=0;k<sizeof(throw_ids)/sizeof(*throw_ids);++k) {
        unsigned id=throw_ids[k];
        world=fixture(file,2);world.condition_words[id]=1;world.battle_enemy_status[1][1]=-1;
        assert(npe_world_battle_throw_item(file,&world,id,1)==NPE_BATTLE_CONTINUE);
        assert(npe_world_battle_defend(&world)==NPE_BATTLE_CONTINUE);
        assert(!world.condition_words[id]);
        if(id==27) assert(world.battle_enemy_hps[1]==500);
        if(id==68) assert(world.battle_enemy_status[1][0]==-4);
        if(id==159 || id==160) assert(world.battle_enemy_hps[1]==(id==159?460:440) &&
                                    !world.battle_enemy_status[1][0]);
    }
    puts("Original item healing, group bonuses, fallen revival, four statuses, retarget and equipment throw passed");
}

static void timer_phase(const NpeWorld *w,const char *name,unsigned step) {
    unsigned matched=0;
    for(unsigned i=0;i<sizeof(native_buff_phases)/sizeof(*native_buff_phases);++i) {
        const NativeBuffPhase *p=&native_buff_phases[i];
        if(strcmp(p->variant,name)||p->phase!=step)continue;
        unsigned n=p->member;
        assert((uint16_t)(w->attack[n]+w->battle_skill_attack_bonus[n])==p->attack);
        assert((uint16_t)(w->battle_party_defenses[n]+w->battle_skill_defense_bonus[n])==p->defense);
        assert(w->battle_attack_turns[n]==p->attack_timer && w->battle_defense_turns[n]==p->defense_timer);
        assert(w->qi[n]==p->qi);++matched;
    }
    assert(matched==(!strcmp(name,"group-order")?3u:2u));
}
static void timer_flows(const NpeFile *file,const NpeFont *font,SDL_Renderer *renderer,SDL_Texture *texture) {
    const char *names[]={"mixed","refresh","group-order","fallen"};
    const unsigned steps[]={6,7,5,5};
    for(unsigned c=0;c<4;++c) {
        scenario=15+c;
        unsigned count=c==2?3:2;NpeWorld w=fixture(file,count);observed=&w;
        for(unsigned i=0;i<count;++i) {
            w.hp[i]=w.max_hp[i]=10000;w.qi[i]=w.max_qi[i]=999;
            w.level[i]=99;w.other_stats[i][1]=(uint16_t)(999-i);
        }
        if(c==3)w.hp[1]=0;
        w.condition_words[62]=c==1?2:1;w.condition_words[63]=c==0||c==2?1:0;
        timer_phase(&w,names[c],0);
        for(unsigned round=0;round<steps[c];++round) {
            for(unsigned actor=0;actor<count;++actor) {
                if(!w.hp[actor])continue;
                assert(w.battle_actor==actor);
                unsigned item=0,spell=42;
                if(c==0 && actor==0 && round<2)item=round?63:62;
                if(c==1 && actor==0 && (round==0||round==2))item=62;
                if(c==2 && !round) {if(actor==0)spell=14;else item=actor==1?62:63;}
                if(c==3 && !round)spell=14;
                SDL_AtomicSet(&ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
                SDL_Thread *thread=SDL_CreateThread(keys,"timer action keys",NULL);assert(thread);
                if(item) {
                    /* Real SDL confirm selects the remaining positive item;
                       the unchanged native group flag auto-submits it. */
                    assert(show_battle_items(renderer,texture,file,font,&w,1)==1);
                    assert(!w.condition_words[item] || (c==1 && !round && w.condition_words[item]==1));
                }else {
                    memset(w.skill_slots[actor],0,sizeof(w.skill_slots[actor]));w.skill_slots[actor][0]=(uint8_t)spell;
                    show_battle_skill(NULL,renderer,texture,file,font,&w);
                }
                int result=-1;SDL_WaitThread(thread,&result);assert(!result);
            }
            timer_phase(&w,names[c],round+1);
        }
        fprintf(stderr,"SDL item timer sequence %s: passed\n",names[c]);
    }
}

int main(int argc,char **argv) {
    assert(argc==2);
    NpeFile file;NpeFont font={0};char error[128];
    uint8_t *bytes;size_t size;
    assert(read_file(argv[1],&bytes,&size)==0);
    assert(npe_parse(&file,bytes,size,error,sizeof(error))==0);
    assert(npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")==0);
    assert(npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin")==0);
    assert(SDL_setenv("SDL_VIDEODRIVER","dummy",1)==0);
    assert(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0);
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)==0);
    SDL_Window *window=SDL_CreateWindow("item probe",0,0,240,120,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,0);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    effects(&file);
    for(scenario=0;scenario<15;++scenario) {
        NpeWorld world=fixture(&file,scenario==2||scenario==14?3:scenario==3?1:2);observed=&world;
        unsigned id=scenario==4?61:scenario==5||scenario==6?65:scenario==9?27:scenario==10?68:46;
        if(scenario<11) world.condition_words[id]=scenario==1?2:1;
        if(scenario==2) world.hp[2]=0;
        if(scenario==5) world.hp[1]=0;
        if(scenario==7) world.condition_words[27]=world.condition_words[159]=1;
        if(scenario==11) {
#define COPY_ENEMY(field) world.field[0]=world.field[1]
            COPY_ENEMY(battle_enemy_ids);COPY_ENEMY(battle_enemy_hps);
            COPY_ENEMY(battle_enemy_max_hps);COPY_ENEMY(battle_enemy_attacks);
            COPY_ENEMY(battle_enemy_defenses);COPY_ENEMY(battle_enemy_speeds); COPY_ENEMY(battle_enemy_lucks);
        COPY_ENEMY(battle_enemy_base_attacks);COPY_ENEMY(battle_enemy_base_defenses);
        COPY_ENEMY(battle_enemy_qis);COPY_ENEMY(battle_enemy_max_qis);
            COPY_ENEMY(battle_enemy_damage_floor_stats);COPY_ENEMY(battle_enemy_spell_stats);
#undef COPY_ENEMY
            memcpy(world.battle_enemy_skill_ids[0],world.battle_enemy_skill_ids[1],3);
        }
        picker_count=uses=throws=prompts=0;SDL_AtomicSet(&ready,0);
        SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        SDL_Thread *thread=SDL_CreateThread(keys,"item keys",NULL);assert(thread);
        int result=show_battle_items(renderer,texture,&file,&font,&world,scenario>=8 && scenario!=13?2:1);
        if(scenario==1) {
            assert(result==1 && world.battle_actor==1 && world.condition_words[46]==1);
            result=show_battle_items(renderer,texture,&file,&font,&world,1);
        }
        if(scenario==14) for(unsigned i=1;i<3;++i) {
            assert(result==1 && world.battle_actor==i);
            result=show_battle_items(renderer,texture,&file,&font,&world,2);
        }
        int thread_result=-1;SDL_WaitThread(thread,&thread_result);assert(thread_result==0);
        if(scenario>=11) {
            assert(!picker_count && !uses && prompts==(scenario==13));
            if(scenario==11 || scenario==13) assert(!result && !throws &&
                !world.battle_pending_mask && npe_world_inventory_word(&world,255)==((uint16_t)((unsigned)world.battle_names[0][0]<<8|world.battle_names[0][1])));
            else assert(result==1 && throws==(scenario==12?1:3) &&
                npe_world_inventory_word(&world,255)==(uint16_t)(((uint16_t)((unsigned)world.battle_names[0][0]<<8|world.battle_names[0][1]))-throws));
            if(scenario==14) assert(!world.battle_pending_mask && world.qi[0]==20 && world.qi[1]==30 && world.qi[2]==40);
            fprintf(stderr,"SDL empty item scenario%u: passed\n",scenario);
            continue;
        }
        if(scenario==0 || scenario==6 || scenario==7 || scenario==8) {
            assert(result==0 && world.condition_words[id]==1 && !world.battle_pending_mask);
        } else assert(result==1 && !world.condition_words[id]);
        assert(prompts==(scenario>=6 && scenario<=8));
        if(scenario==0) {
            const unsigned expected[]={0,1,0,1,0};
            assert(picker_count==5);
            for(unsigned i=0;i<5;++i) assert(targets[i]==expected[i]);
            assert(!uses && !throws);
        } else if(scenario==1) assert(uses==2 && picker_count==3 && targets[0]==0 && targets[1]==1 && targets[2]==1);
        else if(scenario==2) assert(uses==1 && picker_count==2 && targets[1]==2);
        else if(scenario==7) assert(!uses && picker_count==1);
        else assert(!picker_count && uses==(scenario>=3 && scenario<=5) && throws==(scenario>=9));
        fprintf(stderr,"SDL battle item scenario%u: passed\n",scenario);
    }
    timer_flows(&file,&font,renderer,texture);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    npe_font_free(&font);npe_free(&file);return 0;
}
