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
typedef struct {unsigned add,character,mode,next;} CachedParty;
typedef struct {unsigned context,npcContext,kind,selected,first,last,row,ui,quantity,choice,
    descStart,descEnd,menu,member,equipmentSlot,gearSelected,descLength,menuPartyCount,
    exitChoice,exitSlot,music,musicSlot;CachedParty partyCache[3];} Resident;
typedef struct {unsigned id,x,y,direction;} Npc;
typedef struct {unsigned id,active,level,maxHp,hp,maxQi,qi,attack,defense,experience,experienceToNext;
    unsigned otherStats[3],gear[6],guards[4],skills[18];} Actor;
typedef struct {unsigned mode,event,map,progress,sx,sy,vx,vy,direction,money;Npc npc[20];Actor actors[3];} State;
typedef struct {const char *route;unsigned music,event_seed,commits;uint32_t seed,result;Resident before,after;State state;} RestartCase;
#include "new_game_lifetime_native.h"
static NpeWorld *live;
static unsigned case_index,phase,active,submits,total,requests;
static unsigned sections[8],loops[8];
static Uint32 ticks(void){return 0;}
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/new-game-lifetime-");}
static int wait_event(SDL_Event *,int);
static void present(SDL_Renderer *);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetTicks ticks
#define SDL_GetPrefPath probe_pref
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout wait_event
#define SDL_RenderPresent present
#define npe_sound_select select_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_GetPrefPath
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef npe_sound_select
static void observe(void *context,unsigned section,int loop){(void)context;if(!active)return;
    assert(requests<8);sections[requests]=section;loops[requests++]=(unsigned)loop;}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){(void)p;live=w;w->sound_observer=observe;w->sound_context=NULL;}
static void configure(NpeWorld *w,const RestartCase *c){const Resident *s=&c->before;
    w->event_context_id=(uint16_t)s->context;w->mode2_context=(uint8_t)s->npcContext;
    w->shop_kind=w->shop_ready=(uint8_t)s->kind;w->shop_list_ready=1;
    w->shop_selected_item=(uint16_t)s->selected;w->shop_first_item=(uint16_t)s->first;w->shop_last_item=(uint16_t)s->last;
    w->shop_row=(uint8_t)s->row;w->shop_ui=(uint8_t)s->ui;w->shop_quantity=(uint8_t)s->quantity;w->shop_choice=(uint8_t)s->choice;
    w->shop_description_start=(uint16_t)s->descStart;w->shop_description_end=(uint16_t)s->descEnd;
    w->system_menu_stage=(uint8_t)s->menu;
    w->property_page=(uint8_t)s->menu;w->property_member=(uint8_t)s->member;w->equipment_view_slot=(uint8_t)(s->equipmentSlot-1);
    w->equipment_description_item=(uint8_t)s->gearSelected;w->item_description_length=(uint16_t)s->descLength;
    w->menu_party_count=(uint8_t)s->menuPartyCount;w->exit_choice=(uint8_t)s->exitChoice;w->exit_slot=(uint8_t)s->exitSlot;
    w->music_enabled=(uint8_t)s->music;w->map_music_slot=(uint8_t)s->musicSlot;w->sound_section=20;w->sound_loop=1;
    w->random_state=c->seed;w->story_progress=13;w->money=3456;w->lcd_background=0;
    w->story_event_id=(uint16_t)c->event_seed;
    for(unsigned i=0;i<3;++i){const CachedParty *e=&s->partyCache[i];w->party_events[i]=(NpeMapPartyEvent){
        .add_member=(uint8_t)e->add,.character_id=(uint8_t)e->character,.next_mode=(uint8_t)e->mode,.next_id=(uint16_t)e->next};}
}
static void check(const NpeWorld *w,const RestartCase *c){const Resident *s=&c->after;const State *v=&c->state;
    assert(w->event_context_id==s->context&&w->mode2_context==s->npcContext&&w->shop_kind==s->kind&&
        w->shop_selected_item==s->selected&&w->shop_first_item==s->first&&w->shop_last_item==s->last&&
        w->shop_row==s->row&&w->shop_ui==s->ui&&w->shop_quantity==s->quantity&&w->shop_choice==s->choice&&
        w->shop_description_start==s->descStart&&w->shop_description_end==s->descEnd&&
        w->system_menu_stage==s->menu&&w->property_page==s->menu&&w->property_member==s->member&&w->equipment_view_slot+1==s->equipmentSlot&&
        w->equipment_description_item==s->gearSelected&&w->item_description_length==s->descLength&&
        w->menu_party_count==s->menuPartyCount&&w->exit_choice==s->exitChoice&&w->exit_slot==s->exitSlot&&
        w->music_enabled==s->music&&w->map_music_slot==s->musicSlot&&w->lcd_background==0&&w->random_state==c->result);
    assert(!w->shop_ready&&!w->shop_list_ready);
    for(unsigned i=0;i<3;++i){const CachedParty *e=&s->partyCache[i];const NpeMapPartyEvent *a=&w->party_events[i];
        assert(a->add_member==e->add&&a->character_id==e->character&&a->next_mode==e->mode&&a->next_id==e->next);}
    assert(w->game_mode==v->mode&&w->story_event_id==v->event&&w->map_number==v->map&&w->story_progress==v->progress&&
        w->view.screen_x==v->sx&&w->view.screen_y==v->sy&&w->view.offset_x==v->vx&&w->view.offset_y==v->vy&&
        w->direction==v->direction&&w->money==v->money&&w->x==v->sx+v->vx&&w->y==v->sy+v->vy);
    for(unsigned i=0;i<3;++i){const Actor *a=&v->actors[i];
        assert(w->party_ids[i]==a->id&&w->party_active[i]==a->active&&w->level[i]==a->level&&
            w->max_hp[i]==a->maxHp&&w->hp[i]==a->hp&&w->max_qi[i]==a->maxQi&&w->qi[i]==a->qi&&
            w->attack[i]==a->attack&&w->defense[i]==a->defense&&w->experience[i]==a->experience&&w->experience_to_next[i]==a->experienceToNext);
        for(unsigned j=0;j<3;++j)assert(w->other_stats[i][j]==a->otherStats[j]);
        for(unsigned j=0;j<6;++j)assert(w->equipped_items[i][j]==a->gear[j]);
        for(unsigned j=0;j<4;++j)assert(w->status_guard[i][j]==a->guards[j]);
        for(unsigned j=0;j<18;++j)assert(w->skill_slots[i][j]==a->skills[j]);
    }
    NpeMap map;assert(!npe_map(w->battle_file,w->map_number-1,&map));
    for(unsigned i=0;i<20&&v->npc[i].id;++i){const uint8_t *n;unsigned x,y;assert(!npe_map_npc(&map,i,&n));npe_world_npc_position(w,n,i,&x,&y);
        unsigned d=w->npc_override_direction[i]!=255?w->npc_override_direction[i]:n[6];
        assert((unsigned)n[0]*256+n[1]==v->npc[i].id&&x==v->npc[i].x&&y==v->npc[i].y&&d==v->npc[i].direction);}
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const RestartCase *c=&restart_cases[case_index];assert(submits<c->commits);
    char path[160];
    if(c->event_seed)snprintf(path,sizeof(path),"tmp/native-restart-%s-%u-event-%u-%u.idx",c->route,c->music,c->event_seed,submits);
    else snprintf(path,sizeof(path),"tmp/native-restart-%s-%u-%u.idx",c->route,c->music,submits);
    FILE *in=fopen(path,"rb");uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(in&&fread(native,1,sizeof(native),in)==sizeof(native)&&fgetc(in)==EOF&&!fclose(in));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Restart %s music%u event%u commit%u C=%u SDL=%u\n",c->route,c->music,c->event_seed,submits,d,sd);
    assert(!d&&!sd);++submits;++total;
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;memset(e,0,sizeof(*e));
    const RestartCase *c=&restart_cases[case_index];
    if(active){assert(submits==c->commits);check(live,c);
        assert(requests==(c->music?2u:0u));if(c->music)assert(sections[0]==0&&!loops[0]&&sections[1]==22&&loops[1]);
        active=0;e->type=SDL_QUIT;return 1;}
    if(!phase++){live->game_mode=7;live->lcd_background=0;e->type=SDL_USEREVENT;return 1;}
    e->type=SDL_KEYDOWN;
    if(!strcmp(c->route,"empty")&&phase<=4){static const SDL_Keycode keys[]={SDLK_DOWN,SDLK_SPACE,SDLK_DOWN};e->key.keysym.sym=keys[phase-2];return 1;}
    configure(live,c);active=1;requests=submits=0;e->key.keysym.sym=SDLK_SPACE;return 1;
}
static void cold(const NpeFile *file){NpeWorld baseline;assert(!npe_world_new_game(file,&baseline));
    assert(baseline.map_music_slot==0&&baseline.random_state==UINT32_C(0x9e3779b9));
    for(unsigned p=0;p<3;++p){NpeWorld w;memset(&w,p==0?0xcc:p==1?0xff:0,sizeof(w));assert(!npe_world_new_game(file,&w));
        assert(w.story_progress==1&&!w.event_context_id&&!w.mode2_context&&!w.event_blank_background&&
            !w.equipment_description_item&&!w.item_description_length&&!w.equipment_description_revision&&
            !w.shop_kind&&!w.shop_ready&&!w.shop_list_ready&&!w.shop_selected_item&&!w.shop_description_start&&
            !w.battle_enemy_shifted_mask&&!w.battle_party_refresh_mask&&!w.battle_scene_refreshed&&!w.battle_names[0][0]&&!w.battle_names[0][1]);
        assert(w.tile_override_count==baseline.tile_override_count);
        assert(!memcmp(w.tile_override_indices,baseline.tile_override_indices,sizeof(w.tile_override_indices))&&
            !memcmp(w.tile_override_tiles,baseline.tile_override_tiles,sizeof(w.tile_override_tiles)));
    }
    unsigned found=0;
    for(unsigned i=0;i<file->offset_table_records[8];++i){NpeMap m;NpeMapTileChange changes[3];unsigned count;
        assert(!npe_map(file,i,&m)&&!npe_map_tile_changes(&m,changes,&count));
        int later=0;for(unsigned j=0;j<count;++j)if(changes[j].progress>1)later=1;
        if(!later)continue;
        NpeWorld w;memset(&w,0xff,sizeof(w));assert(!npe_world_init(file,&w,i+1,1,1));
        for(unsigned j=0;j<count;++j)for(unsigned k=0;k<changes[j].count;++k){unsigned index=changes[j].indices[k];
            unsigned expected=npe_map_tile(&m,index%m.width,index/m.width);
            for(unsigned n=0;n<count;++n)if(changes[n].progress<=1)for(unsigned q=0;q<changes[n].count;++q)
                if(changes[n].indices[q]==index)expected=changes[n].tiles[q];
            assert(npe_world_tile(&w,&m,index%m.width,index/m.width)==expected);}
        found=1;break;
    }
    assert(found);puts("Cold polluted storage: runtime zeroing and actual NPE progress-gated tile loader passed");
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));cold(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    /* Own isolated pref paths: slot1 occupied, slot2 empty like the native fixture. */
    char *paths[2]={game_save_path(&file,0),game_save_path(&file,1)};assert(paths[0]&&paths[1]);remove(paths[1]);
    NpeWorld saved;assert(!npe_world_new_game(&file,&saved)&&!npe_world_save(&file,&saved,paths[0]));
    for(case_index=0;case_index<sizeof(restart_cases)/sizeof(*restart_cases);++case_index){phase=active=submits=requests=0;
        char *args[]={"npe-sdl","--new-game",argv[1],NULL};assert(!run(3,args));}
    assert(!remove(paths[0]));free(paths[0]);free(paths[1]);
    /* A rejected resource constructor must leave the live world and hooks intact. */
    NpeWorld before=saved;NpeCharacter hero;assert(!npe_character(&file,0,&hero));size_t offset=(size_t)(hero.raw-file.bytes)+78;
    uint8_t original=file.bytes[offset];file.bytes[offset]=4;
    assert(npe_world_restart_game(&file,&saved)==-1&&!memcmp(&before,&saved,sizeof(saved)));file.bytes[offset]=original;
    npe_free(&file);assert(total==24);puts("Eight original new/empty routes:24 actual main.c/SDL commits, resident caches/event word, characters, NPCs, RNG and audio commands matched");return 0;
}
