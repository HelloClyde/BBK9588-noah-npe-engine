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
typedef struct {const char *name;unsigned map,event,commits;MenuState before,after;
#ifdef NPE_PARTY_TELEPORT_RAM
    uint8_t before_cache[38],after_cache[38];
#endif
} PartyCase;
#ifdef NPE_PARTY_TELEPORT_RAM
#include "party_resident_native.h"
#define PARTY_PREFIX "tmp/native-party-resident-"
#else
#include "party_service_native.h"
#define PARTY_PREFIX "tmp/native-party-service-"
#endif
static unsigned case_index,submits,total,active,poll_toggle,quit_requested;
static Uint32 fake_clock;
static NpeWorld *live;
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *e){
    if(!active)return 0;
    memset(e,0,sizeof(*e));e->type=quit_requested?SDL_QUIT:SDL_KEYDOWN;e->key.keysym.sym=SDLK_F4;return 1;
}
static void probe_present(SDL_Renderer *);
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
    if(before)snprintf(path,sizeof(path),PARTY_PREFIX "%s-before.idx",party_cases[case_index].name);
    else snprintf(path,sizeof(path),PARTY_PREFIX "%s-%u.idx",party_cases[case_index].name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const PartyCase *c=&party_cases[case_index];assert(submits<c->commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"party service %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);
    if(d){FILE *out=fopen("tmp/party-service-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);
    if(submits<c->commits-1){
        unsigned event_id=c->before.event;
#ifndef NPE_PARTY_TELEPORT_RAM
        if(c->map==120){
            unsigned first_noop=0;
            for(unsigned i=0;i<3;++i)first_noop|=c->before.actors[i].id==2&&c->before.actors[i].active==1;
            if(first_noop||submits>=2)event_id=2;
        }
#endif
        assert(live->game_mode==10&&live->story_event_id==event_id);
    }
    ++submits;++total;
}
static void check_actors(const NpeWorld *w,const MenuState *s){
    for(unsigned i=0;i<3;++i){const MenuActor *a=&s->actors[i];
        assert(w->party_ids[i]==a->id && w->party_active[i]==a->active && w->level[i]==a->level &&
            w->max_hp[i]==a->maxHp && w->hp[i]==a->hp && w->max_qi[i]==a->maxQi && w->qi[i]==a->qi &&
            w->attack[i]==a->attack && w->defense[i]==a->defense && w->experience[i]==a->experience && w->experience_to_next[i]==a->experienceToNext);
        for(unsigned j=0;j<3;++j)assert(w->other_stats[i][j]==a->otherStats[j]);
        for(unsigned j=0;j<6;++j)assert(w->equipped_items[i][j]==a->gear[j]);
        for(unsigned j=0;j<4;++j)assert(w->status_guard[i][j]==a->guards[j]);
        for(unsigned j=0;j<18;++j)assert(w->skill_slots[i][j]==a->skills[j]);
    }
}
static void configure(NpeWorld *w,const NpeWorld *baseline,const MenuState *s){
    *w=*baseline;
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
    w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;w->lcd_background=0;npe_world_seed(w,11);
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint8_t)s->npc[i].x;w->npc_override_y[i]=(uint8_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
    check_actors(w,s);
}
int main(int argc,char **argv){NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld baseline,w;
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("party service",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!npe_world_new_game(&file,&baseline));live=&w;
#ifdef NPE_PARTY_TELEPORT_RAM
    uint64_t fingerprint=npe_game_fingerprint(&file);
#else
    NpeFile diagnostic;
    uint8_t *diagnostic_bytes=malloc(size);assert(diagnostic_bytes);memcpy(diagnostic_bytes,bytes,size);
    assert(!npe_parse(&diagnostic,diagnostic_bytes,size,error,sizeof(error)));
    NpeMap cached_map;NpeMapPartyEvent cached_event;
    assert(!npe_map(&diagnostic,0,&cached_map)&&npe_map_party_event(&cached_map,1,&cached_event)==1);
    size_t cached_offset=(size_t)(cached_event.raw-diagnostic_bytes);
#endif
    for(case_index=0;case_index<sizeof(party_cases)/sizeof(*party_cases);++case_index){const PartyCase *c=&party_cases[case_index];
#ifndef NPE_PARTY_TELEPORT_RAM
        NpeMap map;NpeMapPartyEvent event;
        assert(!npe_map(&file,c->map-1,&map)&&npe_map_party_event(&map,c->event,&event)==1);
#endif
        configure(&w,&baseline,&c->before);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);
#ifdef NPE_PARTY_TELEPORT_RAM
        w.battle_file=&file;assert(!npe_world_resources_open(&file,&w));
        assert(!npe_rpg_runtime_write(&file,0x52788e,c->before_cache,38));
        NpeFile *dispatch_file=&file;
#else
        /* Match the original isolated cache input: transfer its unmodified
           NPE event record into map1's group6 in a memory-only file copy.
           The production dispatcher/parser/renderers then run normally. */
        memcpy(diagnostic_bytes+cached_offset,event.raw,7);w.battle_file=&diagnostic;
        if(event.next_mode==10){NpeMapPartyEvent second,cached_second;
            assert(npe_map_party_event(&map,event.next_id,&second)==1&&npe_map_party_event(&cached_map,2,&cached_second)==1);
            memcpy((uint8_t *)cached_second.raw,second.raw,7);
        }
        assert(!npe_world_load_party_events(&w,&cached_map));
        NpeFile *dispatch_file=&diagnostic;
#endif
        active=0;draw_pixels(renderer,texture,pixels);save_map_edges(dispatch_file,renderer,pixels);retain_world_frame(dispatch_file,&w,renderer,pixels,1);
        submits=poll_toggle=0;active=1;fake_clock=case_index==1?UINT32_MAX-1000:0;Uint32 start=fake_clock;
        advance_world_events(window,renderer,texture,dispatch_file,&font,&w,10,0);
        assert(fake_clock==start); /* Injected F4 dismisses each notice. */
        assert(w.game_mode==c->after.mode&&w.story_event_id==c->after.event&&w.event_context_id==c->after.context&&w.menu_party_count==c->after.menuPartyCount);
        check_actors(&w,&c->after);
        assert(submits==c->commits&&w.random_state==11);
        NpeMap current;assert(!npe_map(&file,w.map_number-1,&current));
        for(unsigned i=0;i<20&&c->after.npc[i].id;++i){const uint8_t *npc;unsigned x,y;assert(!npe_map_npc(&current,i,&npc));
            npe_world_npc_position(&w,npc,i,&x,&y);
            unsigned direction=w.npc_override_direction[i]!=255?w.npc_override_direction[i]:npc[6];
            assert(x==c->after.npc[i].x&&y==c->after.npc[i].y&&direction==c->after.npc[i].direction);}
        fprintf(stderr,"party service %s: original records,126 fields,notice wait and map return matched\n",c->name);
#ifdef NPE_PARTY_TELEPORT_RAM
        uint8_t actual[38];assert(!npe_rpg_runtime_read(&file,0x52788e,actual,38));assert(!memcmp(actual,c->after_cache,38));
        assert(npe_game_fingerprint(&file)==fingerprint&&!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
#endif
    }
    // Quit during the notice leaves its followup pending.
    case_index=0;configure(&w,&baseline,&party_cases[0].before);NpeMap map;NpeMapPartyEvent event;
    assert(!npe_map(&file,party_cases[0].map-1,&map)&&npe_map_party_event(&map,party_cases[0].event,&event)==1);
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);active=0;draw_pixels(renderer,texture,pixels);submits=poll_toggle=0;active=quit_requested=1;
    assert(show_party_change(renderer,texture,&file,&font,&w,&event)==-2&&submits==1&&w.game_mode==10&&w.story_event_id==1);
    active=0;
    for(unsigned i=0;i<4;++i){const unsigned ids[]={0,4,255,256};w=baseline;w.game_mode=10;w.story_event_id=(uint16_t)ids[i];
        assert(npe_world_apply_party_event(&file,&w)==0&&w.game_mode==0);}
    w=baseline;w.game_mode=10;w.story_event_id=257;assert(npe_world_apply_party_event(&file,&w)==1&&w.party_active[1]==1&&w.game_mode==3);
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
#ifndef NPE_PARTY_TELEPORT_RAM
    npe_free(&diagnostic);
#endif
    npe_free(&file);
#ifdef NPE_PARTY_TELEPORT_RAM
    printf("Party resident:50 original cases,%u C/SDL dispatcher commits,38 cache bytes,126 actor fields,no NPE edits and retained third slot passed\n",total-1);
#else
    printf("Party service:61 original cases,137 C/SDL commits,record fields,strict flags,notice key/quit and event range passed\n");return 0;
#endif
    return 0;
}
