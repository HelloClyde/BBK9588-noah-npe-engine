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

typedef struct { unsigned id,x,y,direction; } HealingNpc;
typedef struct {
    unsigned mode,event,context,map,progress,money,init,sx,sy,vx,vy,direction,enabled,slot;
    unsigned maxHp[3],hp[3],maxQi[3],qi[3];
    HealingNpc npc[20];
} HealingState;
typedef struct { const char *name;unsigned fee,commits;HealingState before,after; } HealingCase;
typedef struct { unsigned key,commits;HealingState after; } HealingPhase;
typedef struct { HealingCase service;unsigned phases;HealingPhase phase[4];uint8_t table[6]; } HealingChoiceCase;
#include "healing_service_native.h"
static unsigned case_index,submits,total,active,timeout_requested,quit_requested;
static Uint32 fake_clock;
static const HealingCase *expected_case;
static unsigned choice_active;
static NpeWorld *live;
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *e){
    if(!active||timeout_requested)return 0;
    memset(e,0,sizeof(*e));
    e->type=quit_requested?SDL_QUIT:SDL_KEYDOWN;e->key.keysym.sym=SDLK_F4;
    return 1;
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

static void read_frame(int before,unsigned index,uint8_t *pixels){
    char path[180];const char *name=expected_case->name;
    const char *kind=choice_active?"choice-short":"service";
    if(before)snprintf(path,sizeof(path),"tmp/native-healing-%s-%s-before.idx",kind,name);
    else snprintf(path,sizeof(path),"tmp/native-healing-%s-%s-%u.idx",kind,name,index);
    FILE *in=fopen(path,"rb");
    assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void check_roles(const NpeWorld *w,const HealingState *s){
    for(unsigned i=0;i<3;++i)
        assert(w->max_hp[i]==s->maxHp[i]&&w->hp[i]==s->hp[i]&&w->max_qi[i]==s->maxQi[i]&&w->qi[i]==s->qi[i]);
}
static void probe_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!active)return;
    const HealingCase *c=expected_case;assert(submits<c->commits);
    uint8_t original[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,original);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(original);++i){unsigned v=original[i];
        d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));
    }
    fprintf(stderr,"healing %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);assert(!d&&!sd);
    if(choice_active && live->game_mode==17){
        check_roles(live,&c->before);assert(live->money==c->before.money&&live->story_event_id==1&&!live->map_init_pending);
    }else{
        check_roles(live,&c->after);assert(live->money==c->after.money&&live->story_event_id==c->after.event);
        if(submits+1<c->commits)assert(live->game_mode==22&&live->map_init_pending==c->before.init);
        else assert(live->game_mode==1&&live->map_init_pending==0);
    }
    char path[180];snprintf(path,sizeof(path),"tmp/c-healing-%s-%s-%u.idx",choice_active?"choice-short":"service",c->name,submits);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(original),out)==sizeof(original)&&!fclose(out));
    ++submits;++total;
}
static void configure(NpeWorld *w,const NpeWorld *baseline,const HealingCase *c){
    const HealingState *s=&c->before;*w=*baseline;
    if(s->map!=baseline->map_number)assert(!npe_world_init(baseline->battle_file,w,s->map,s->sx+s->vx,s->sy+s->vy));
    assert(!npe_world_resources_open((NpeFile *)w->battle_file,w));
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;
    w->money=s->money;w->story_progress=(uint16_t)s->progress;w->map_init_pending=(uint8_t)s->init;
    w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
    w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=s->direction;
    w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;
    for(unsigned i=0;i<3;++i){w->max_hp[i]=(uint16_t)s->maxHp[i];w->hp[i]=(uint16_t)s->hp[i];
        w->max_qi[i]=(uint16_t)s->maxQi[i];w->qi[i]=(uint16_t)s->qi[i];}
    for(unsigned i=0;i<20&&s->npc[i].id;++i){
        w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;
    }
    npe_world_seed(w,11);
}
static void before_frame(SDL_Renderer *renderer,SDL_Texture *texture,const NpeFile *file,NpeWorld *w){
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);active=0;
    draw_pixels(renderer,texture,pixels);save_map_edges(file,renderer,pixels);retain_world_frame(file,w,renderer,pixels,1);
}
static void check_after(const NpeFile *file,const NpeWorld *w,const HealingCase *c){
    assert(w->money==c->after.money&&w->game_mode==c->after.mode&&w->story_event_id==c->after.event&&
           w->map_init_pending==c->after.init&&w->random_state==11);
    check_roles(w,&c->after);
    uint8_t cash[4];assert(!npe_rpg_runtime_read(file,0x52799e,cash,4));
    assert((((uint32_t)cash[0]<<24)|((uint32_t)cash[1]<<16)|((uint32_t)cash[2]<<8)|cash[3])==w->money);
    for(unsigned i=0;i<3;++i){uint8_t record[8];assert(!npe_rpg_runtime_read(file,0x526a14+i*148,record,8));
        assert(((unsigned)record[0]*256+record[1])==w->max_hp[i]&&((unsigned)record[2]*256+record[3])==w->hp[i]&&
               ((unsigned)record[4]*256+record[5])==w->max_qi[i]&&((unsigned)record[6]*256+record[7])==w->qi[i]);}
    assert(!npe_rpg_runtime_failures(file));
}
int main(int argc,char **argv){
    NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld baseline,w;
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("healing service",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!npe_world_new_game(&file,&baseline));live=&w;
    for(case_index=0;case_index<sizeof(healing_cases)/sizeof(healing_cases[0]);++case_index){
        const HealingCase *c=&healing_cases[case_index];expected_case=c;configure(&w,&baseline,c);before_frame(renderer,texture,&file,&w);
        submits=0;active=1;fake_clock=0;
        assert(advance_world_events(window,renderer,texture,&file,&font,&w,22,0)>=0);
        assert(fake_clock==0&&submits==c->commits);check_after(&file,&w,c);
        active=0;npe_rpg_runtime_close(&file);
    }
    /* Numeric entry preserves the full fee and delays initialization until finish. */
    for(case_index=0;case_index<9;++case_index){const HealingCase *c=&healing_cases[case_index];
        configure(&w,&baseline,c);unsigned healed=c->commits==4;
        assert(npe_world_healing_effect(&w)==(int)healed&&w.game_mode==22&&w.story_event_id==c->fee&&w.map_init_pending==c->before.init);
        check_roles(&w,&c->after);assert(w.money==c->after.money);
        assert(npe_world_finish_healing_event(&w)==1&&w.game_mode==1&&w.map_init_pending==1&&w.story_event_id==c->fee);
        NpeWorld before=w;assert(npe_world_healing_effect(&w)<0&&npe_world_finish_healing_event(&w)<0&&!memcmp(&before,&w,sizeof(w)));
        npe_rpg_runtime_close(&file);
    }
    /* No input consumes2000ms, including Uint32 wrap; a key consumes none. */
    timeout_requested=1;
    for(case_index=0;case_index<2;++case_index){const HealingCase *c=&healing_cases[case_index];expected_case=c;
        configure(&w,&baseline,c);before_frame(renderer,texture,&file,&w);submits=0;active=1;fake_clock=UINT32_MAX-1000;
        Uint32 start=fake_clock;assert(advance_world_events(window,renderer,texture,&file,&font,&w,22,0)>=0);
        assert((Uint32)(fake_clock-start)==2000&&submits==c->commits);check_after(&file,&w,c);
        active=0;npe_rpg_runtime_close(&file);
    }
    timeout_requested=0;quit_requested=1;
    for(case_index=0;case_index<2;++case_index){const HealingCase *c=&healing_cases[case_index];expected_case=c;
        configure(&w,&baseline,c);before_frame(renderer,texture,&file,&w);submits=0;active=1;
        assert(show_healing_event(renderer,texture,&file,&font,&w)==-2&&submits==(c->commits==4?2u:1u)&&
               w.game_mode==22&&w.story_event_id==c->fee&&w.map_init_pending==c->before.init&&w.money==c->after.money);
        check_roles(&w,&c->after);active=0;npe_rpg_runtime_close(&file);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    }
    quit_requested=0;choice_active=1;
    for(case_index=0;case_index<3;++case_index){const HealingChoiceCase *choice=&healing_choices[case_index];
        const HealingCase *c=&choice->service;expected_case=c;configure(&w,&baseline,c);before_frame(renderer,texture,&file,&w);
        uint8_t table[6];assert(!npe_rpg_runtime_read(&file,0x52781c,table,6)&&!memcmp(table,choice->table,6));
        submits=0;active=1;fake_clock=0;unsigned levels[3]={0};
        assert(!draw_world(renderer,texture,&file,&w,0,0,0,0,0,0,levels,0,&font));assert(submits==1&&w.game_mode==17);
        if(case_index<2){assert(!game_story_choice_input(&file,&w,SDLK_LEFT,renderer,texture));assert(submits==2&&w.event_context_id==1);}
        w.defer_story_events=1;assert(!game_story_choice_input(&file,&w,SDLK_RETURN,renderer,texture));
        assert(w.money==c->before.money);
        if(case_index<2)assert(w.game_mode==22&&w.story_event_id==10);
        assert(advance_world_events(window,renderer,texture,&file,&font,&w,17,0)>=0);
        assert(submits==c->commits&&fake_clock==0);check_after(&file,&w,c);
        assert(!npe_rpg_runtime_read(&file,0x52781c,table,6)&&!memcmp(table,choice->table,6));
        active=0;npe_rpg_runtime_close(&file);
        /* The same real resource choice works for the non-display numeric caller. */
        if(case_index<2){configure(&w,&baseline,c);w.defer_story_events=0;
            assert(npe_world_select_choice(&file,&w,0)==1&&w.game_mode==1&&w.map_init_pending==1&&w.story_event_id==10&&w.money==c->after.money);
            check_roles(&w,&c->after);npe_rpg_runtime_close(&file);}
    }
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    printf("Healing service:9 original paths,3 actual-NPE choice chains,46 original frames/%u C/SDL commits,timeout/wrap/key/quit and numeric dispatch passed\n",total);return 0;
}
