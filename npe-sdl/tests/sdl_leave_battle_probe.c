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
typedef struct {unsigned id,x,y,direction;} MenuNpc;
typedef struct {unsigned id,active,portrait,level,maxHp,hp,maxQi,qi,attack,defense,experience,experienceToNext;unsigned otherStats[3],gear[6],guards[4],skills[18];} MenuActor;
typedef struct {unsigned mode,event,map,progress,sx,sy,vx,vy,direction,money,enabled,slot,context,menu,member,equipmentSlot,gearSelected,descLength,descStart,descEnd,menuPartyCount;MenuNpc npc[20];MenuActor actors[3];} MenuState;
typedef struct {unsigned hp,maxHp,qi,maxQi,attack,defense,speed,status[4];} BattleRecord;
typedef struct {unsigned party,actor,target,enemies[3];BattleRecord records[6];} BattleState;
typedef struct {const char *name;unsigned commits,input;MenuState before,after,states[8];BattleState battle;uint32_t seed,result;unsigned calls;} LeaveCase;
#include "leave_battle_native.h"
static unsigned case_index,submits,total,active,sound_requests,sound_section;
static Uint32 fake_clock;
static Uint32 notice_start;
static unsigned input_sent;
static NpeWorld *live;
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *e){
    unsigned mask=leave_cases[case_index].input;
    if(!active||!mask||input_sent||(Uint32)(fake_clock-notice_start)<250)return 0;
    const SDL_Keycode keys[]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT,SDLK_RETURN,SDLK_ESCAPE,SDLK_F4};
    unsigned index=0;while((1u<<index)!=mask){assert(index<6);++index;}
    memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=keys[index];input_sent=1;return 1;
}
static void probe_present(SDL_Renderer *);
static void check_actors(const NpeWorld *,const MenuState *);
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_RenderPresent probe_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_RenderPresent
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[160];
    if(before)snprintf(path,sizeof(path),"tmp/native-leave-battle-%s-before.idx",leave_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-leave-battle-%s-%u.idx",leave_cases[case_index].name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void check_state(const NpeWorld *w,const MenuState *s){
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->event_context_id==s->context&&w->map_number==s->map&&w->story_progress==s->progress);
    assert(w->view.screen_x==s->sx&&w->view.screen_y==s->sy&&w->view.offset_x==s->vx&&w->view.offset_y==s->vy&&w->direction==s->direction);
    assert(w->music_enabled==s->enabled&&w->map_music_slot==s->slot&&w->money==s->money&&w->menu_party_count==s->menuPartyCount);
    check_actors(w,s);
    NpeMap map;assert(!npe_map(w->battle_file,w->map_number-1,&map));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){const uint8_t *npc;unsigned x,y;assert(!npe_map_npc(&map,i,&npc));npe_world_npc_position(w,npc,i,&x,&y);
        unsigned direction=w->npc_override_direction[i]!=255?w->npc_override_direction[i]:npc[6];
        assert(x==s->npc[i].x&&y==s->npc[i].y&&direction==s->npc[i].direction);}
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const LeaveCase *c=&leave_cases[case_index];assert(submits<c->commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"leave battle %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);
    if(d){FILE *out=fopen("tmp/leave-battle-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    /* Native advance(input) can complete several stages before returning.
       Its single post-call RAM sample is not the state of every commit. */
    assert(!d&&!sd);if(!c->input)check_state(live,&c->states[submits]);++submits;++total;
}
static void observe_sound(void *context,unsigned section,int loop){(void)context;assert(loop==1);++sound_requests;sound_section=section;}
static void check_battle(const NpeWorld *w,const BattleState *b){
    unsigned count=0;for(unsigned i=0;i<3;++i)count+=w->party_active[i]!=0;
    assert(w->battle_party_flags_saved&&w->battle_record_party_count==b->party);
    for(unsigned i=0;i<3;++i)assert(w->party_active[i]==(i<b->party));
    assert(count==b->party&&w->battle_actor==b->actor&&w->battle_target+count==b->target&&w->battle_enemy_count==3);
    for(unsigned i=0;i<3;++i)assert(w->battle_enemy_ids[i]==b->enemies[i]);
    for(unsigned i=0;i<count+3;++i){const BattleRecord *r=&b->records[i];unsigned e=i<count?0:i-count;
        unsigned hp=i<count?w->hp[i]:w->battle_enemy_hps[e],maxhp=i<count?w->max_hp[i]:w->battle_enemy_max_hps[e];
        unsigned qi=i<count?w->qi[i]:w->battle_enemy_qis[e],maxqi=i<count?w->max_qi[i]:w->battle_enemy_max_qis[e];
        unsigned attack=i<count?w->attack[i]:w->battle_enemy_attacks[e],defense=i<count?w->defense[i]:w->battle_enemy_defenses[e];
        fprintf(stderr,"compact%u hp%u/%u qi%u/%u atk%u/%u def%u/%u\n",i,hp,r->hp,qi,r->qi,attack,r->attack,defense,r->defense);
        assert(hp==r->hp&&maxhp==r->maxHp&&qi==r->qi&&maxqi==r->maxQi&&attack==r->attack&&defense==r->defense);
        assert((i<count?w->other_stats[i][1]:w->battle_enemy_speeds[e])==r->speed);
        for(unsigned s=0;s<4;++s)assert((uint16_t)(i<count?w->battle_party_status[i][s]:w->battle_enemy_status[e][s])==r->status[s]);
    }
}
static void check_actors(const NpeWorld *w,const MenuState *s){
    for(unsigned i=0;i<3;++i){const MenuActor *a=&s->actors[i];
        /* stateAt samples the map bank's 69F1, not the battle prefix.
           Entry retains that raw byte (including 2) in the map snapshot;
           the host party_active array selects the normalized battle prefix. */
        unsigned map_active=w->battle_party_flags_saved?w->battle_map_party_active[i]:w->party_active[i];
        if(w->party_ids[i]!=a->id||map_active!=a->active||w->hp[i]!=a->hp)
            fprintf(stderr,"actor difference %s/%u: id %u/%u active %u/%u hp %u/%u\n",
                leave_cases[case_index].name,i,w->party_ids[i],a->id,map_active,a->active,w->hp[i],a->hp);
        assert(w->party_ids[i]==a->id && map_active==a->active && w->level[i]==a->level &&
            w->max_hp[i]==a->maxHp && w->hp[i]==a->hp && w->max_qi[i]==a->maxQi && w->qi[i]==a->qi &&
            w->attack[i]==a->attack && w->defense[i]==a->defense && w->experience[i]==a->experience && w->experience_to_next[i]==a->experienceToNext);
        for(unsigned j=0;j<3;++j)assert(w->other_stats[i][j]==a->otherStats[j]);
        for(unsigned j=0;j<6;++j)assert(w->equipped_items[i][j]==a->gear[j]);
        for(unsigned j=0;j<4;++j)assert(w->status_guard[i][j]==a->guards[j]);
        for(unsigned j=0;j<18;++j)assert(w->skill_slots[i][j]==a->skills[j]);
    }
}
static void configure(const NpeFile *file,NpeWorld *w,const NpeWorld *baseline,const MenuState *s){
    /* The original reference actually loaded map117 through its UI. Changing
       only the map number leaves the new-game map's resident battle/monster
       tables in place. Load the complete current map before reordering actors. */
    assert(!npe_world_init(file,w,s->map,s->sx+s->vx,s->sy+s->vy));
#define COPY_ACTOR(field) memcpy(&w->field[i],&baseline->field[source],sizeof(w->field[i]))
    for(unsigned i=0;i<3;++i){unsigned source=s->actors[i].id-1;assert(source<3);
        COPY_ACTOR(party_names);COPY_ACTOR(party_characters);COPY_ACTOR(party_attack_groups);COPY_ACTOR(party_ids);COPY_ACTOR(party_active);COPY_ACTOR(skill_slots);COPY_ACTOR(learned_skill_count);
        COPY_ACTOR(equipped_items);COPY_ACTOR(hp);COPY_ACTOR(max_hp);COPY_ACTOR(qi);COPY_ACTOR(max_qi);
        COPY_ACTOR(attack);COPY_ACTOR(defense);COPY_ACTOR(experience);COPY_ACTOR(experience_to_next);
        COPY_ACTOR(level);COPY_ACTOR(growth);COPY_ACTOR(other_stats);COPY_ACTOR(damage_floor_stat);COPY_ACTOR(status_guard);
        w->party_active[i]=(uint8_t)s->actors[i].active;
    }
#undef COPY_ACTOR
    w->map_number=s->map;w->game_mode=10;w->story_event_id=(uint16_t)s->event;w->story_progress=(uint16_t)s->progress;
    w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
    w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=s->direction;w->money=s->money;
    w->event_context_id=(uint16_t)s->context;w->menu_party_count=(uint8_t)s->menuPartyCount;
    w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;w->lcd_background=0;npe_world_seed(w,leave_cases[case_index].seed);
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint8_t)s->npc[i].x;w->npc_override_y[i]=(uint8_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
    check_actors(w,s);
}
int main(int argc,char **argv){NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld baseline,w;
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("leave battle",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!npe_world_new_game(&file,&baseline));live=&w;
    for(case_index=0;case_index<sizeof(leave_cases)/sizeof(*leave_cases);++case_index){const LeaveCase *c=&leave_cases[case_index];NpeMap map;
        configure(&file,&w,&baseline,&c->before);w.battle_file=&file;
        assert(!npe_map(&file,w.map_number-1,&map)&&!npe_world_load_party_events(&w,&map));
        w.sound_observer=observe_sound;w.sound_context=NULL;sound_requests=sound_section=0;
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);
        active=0;draw_pixels(renderer,texture,pixels);save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&w,renderer,pixels,1);
        submits=input_sent=0;active=1;fake_clock=case_index==1?UINT32_MAX-1000:0;Uint32 start=fake_clock;notice_start=start;
        advance_world_events(window,renderer,texture,&file,&font,&w,10,0);
        assert(w.game_mode==3&&submits==c->commits-4);
        unsigned notices=(c->commits-4)/2;
        assert((Uint32)(fake_clock-start)==(c->input?250+(notices-1)*2000:notices*2000));
        assert(input_sent==(c->input!=0));
        fprintf(stderr,"leave entry %s: map=%u event=%u selected=%u,%u,%u enemies=%u,%u,%u\n",
            c->name,w.map_number,w.story_event_id,w.battle_selected_ids[0],
            w.battle_selected_ids[1],w.battle_selected_ids[2],w.battle_enemy_ids[0],
            w.battle_enemy_ids[1],w.battle_enemy_ids[2]);
        assert(!draw_battle_entry(renderer,texture,&file,&w,&font));
        check_state(&w,&c->after);check_battle(&w,&c->battle);
        assert(submits==c->commits&&w.random_state==c->result&&sound_requests==c->calls);
        if(c->calls)assert(sound_section==23u+((c->result>>16)&0x7fff)%60/10);
        fprintf(stderr,"leave battle %s: state,fields,battle,RNG and sound dispatch matched\n",c->name);
    }
    active=0;npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    printf("Leave battle:16 original UI-loaded cases,104 actual main C/SDL commits,fields,battle,RNG,sound,seven dismissing keys and single-key chain consumption passed\n");return 0;
}
