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
typedef struct {unsigned id,x,y,direction;} LeaderNpc;
typedef struct {unsigned mode,event,context,map,progress,init,sx,sy,vx,vy,direction,menu_count,steps;
    uint8_t actors[3][148];LeaderNpc npc[20];uint16_t inventory[255];uint8_t objects[16];} LeaderState;
typedef struct {unsigned key,commits;LeaderState after;} LeaderPhase;
typedef struct {const char *name;unsigned commits,phases,original_input;LeaderState before;LeaderPhase phase[3];} LeaderCase;
#include "leader_actions_native.h"
static unsigned case_index,phase_index,submits,total,initial_frames,started,finished;
static Uint32 fake_clock;
static NpeWorld *live;
static int wait_input(SDL_Event *,int);
static void present(SDL_Renderer *);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 ms){fake_clock+=ms;}
static SDL_Window *create_window(const char *name,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;return SDL_CreateWindow(name,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_WaitEventTimeout wait_input
#define SDL_RenderPresent present
#define npe_sound_select select_sound
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_CreateWindow create_window
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef npe_sound_select
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_CreateWindow
static unsigned native_word(const uint8_t *r,unsigned o){return (unsigned)r[o]*256+r[o+1];}
static void check_state(const NpeWorld *w,const LeaderState *s){
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->event_context_id==s->context&&w->map_number==s->map);
    assert(w->story_progress==s->progress&&w->map_init_pending==s->init&&w->direction==s->direction&&w->menu_party_count==s->menu_count&&w->encounter_steps==s->steps);
    if(w->view.screen_x!=s->sx||w->view.screen_y!=s->sy||w->view.offset_x!=s->vx||w->view.offset_y!=s->vy)
        fprintf(stderr,"View phase%u actual%u,%u,%u,%u expected%u,%u,%u,%u\n",phase_index,w->view.screen_x,w->view.screen_y,w->view.offset_x,w->view.offset_y,s->sx,s->sy,s->vx,s->vy);
    assert(w->view.screen_x==s->sx&&w->view.screen_y==s->sy&&w->view.offset_x==s->vx&&w->view.offset_y==s->vy);
    assert(w->x==s->sx+s->vx&&w->y==s->sy+s->vy&&w->random_state==11);
    for(unsigned i=0;i<3;++i){const uint8_t *r=s->actors[i];
        assert(w->party_ids[i]==r[12]&&w->party_active[i]==r[43]&&(i?w->companion_directions[i-1]:w->direction)==r[14]);
        assert(w->level[i]==r[24]&&w->hp[i]==native_word(r,80)&&w->qi[i]==native_word(r,84));
        if(memcmp(w->skill_slots[i],r+25,18)||memcmp(w->equipped_items[i],r+18,6))
            fprintf(stderr,"Actor%u phase%u skills first%u/%u gear first%u/%u\n",i,phase_index,w->skill_slots[i][0],r[25],w->equipped_items[i][0],r[18]);
        assert(!memcmp(w->skill_slots[i],r+25,18)&&!memcmp(w->equipped_items[i],r+18,6));}
    assert(!memcmp(w->condition_words,s->inventory,sizeof(s->inventory))&&!memcmp(w->object_done,s->objects,sizeof(s->objects)));
    for(unsigned i=0;i<20;++i){assert(w->npc_override_x[i]==s->npc[i].x&&w->npc_override_y[i]==s->npc[i].y&&w->npc_override_direction[i]==s->npc[i].direction);}
}
static void configure(NpeWorld *w,const LeaderState *s){
    w->game_mode=s->mode;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;w->story_progress=(uint16_t)s->progress;
    w->map_init_pending=(uint8_t)s->init;w->direction=4;w->menu_party_count=(uint8_t)s->menu_count;w->encounter_steps=(uint16_t)s->steps;
    w->view=(NpeViewport){s->sx,s->sy,s->vx,s->vy};w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->music_enabled=0;w->lcd_background=0;npe_world_seed(w,11);
    for(unsigned i=0;i<3;++i){const uint8_t *r=s->actors[i];w->party_ids[i]=r[12];w->party_active[i]=r[43];w->level[i]=r[24];
        w->hp[i]=(uint16_t)native_word(r,80);w->max_hp[i]=(uint16_t)native_word(r,78);w->qi[i]=(uint16_t)native_word(r,84);w->max_qi[i]=(uint16_t)native_word(r,82);
        w->attack[i]=(uint16_t)native_word(r,92);w->defense[i]=(uint16_t)native_word(r,94);w->damage_floor_stat[i]=r[24];
        w->experience[i]=(uint16_t)native_word(r,76);w->experience_to_next[i]=(uint16_t)native_word(r,74);
        memcpy(w->skill_slots[i],r+25,18);memcpy(w->equipped_items[i],r+18,6);
        for(unsigned j=0;j<3;++j)w->other_stats[i][j]=(uint16_t)native_word(r,86+j*2);
        for(unsigned j=0;j<4;++j)w->status_guard[i][j]=(uint16_t)native_word(r,62+j*2);
        if(i)w->companion_directions[i-1]=r[14];}
    memcpy(w->condition_words,s->inventory,sizeof(s->inventory));memcpy(w->object_done,s->objects,sizeof(s->objects));
    for(unsigned i=0;i<20;++i){w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;w->npc_override_direction[i]=(uint8_t)s->npc[i].direction;}
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){if(!live){live=w;configure(w,&leader_cases[case_index].before);}npe_sound_select(p,w);SDL_PauseAudioDevice(p->device,1);}
static void frame(SDL_Renderer *r,int before,unsigned index){char path[180];
    if(before)snprintf(path,sizeof(path),"tmp/native-leader-actions-%s-before.idx",leader_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-leader-actions-%s-%u.idx",leader_cases[case_index].name,index);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *in=fopen(path,"rb");
    assert(in&&fread(native,1,sizeof(native),in)==sizeof(native)&&fgetc(in)==EOF&&!fclose(in));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Leader action %s %s%u C=%u SDL=%u\n",leader_cases[case_index].name,before?"initial":"commit",index,d,sd);assert(!d&&!sd);
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!started){frame(r,1,0);++initial_frames;return;}
    assert(submits<leader_cases[case_index].commits);frame(r,0,submits++);++total;}
static SDL_Keycode keycode(unsigned key){return key==16?SDLK_SPACE:key==256?SDLK_RETURN:key==64?SDLK_F4:key==4?SDLK_LEFT:SDLK_DOWN;}
static int wait_input(SDL_Event *e,int timeout){memset(e,0,sizeof(*e));
    /* The walking commit waits one nominal64-Hz tick before each frame.
       Those inner waits do not deliver the next main-loop input. */
    if(started&&timeout>0&&timeout<=16){fake_clock+=(Uint32)timeout;return 0;}
    const LeaderCase *c=&leader_cases[case_index];
    if(!started){assert(live&&initial_frames==1);live->direction=c->before.direction;world_map_frame_state.direction=live->direction;
        started=1;check_state(live,&c->before);}
    else {const LeaderPhase *p=&c->phase[phase_index-1];check_state(live,&p->after);
        unsigned expected=0;for(unsigned i=0;i<phase_index;++i)expected+=c->phase[i].commits;assert(submits==expected);}
    if(phase_index==c->phases){finished=1;e->type=SDL_QUIT;return 1;}
    e->type=SDL_KEYDOWN;e->key.keysym.sym=keycode(c->phase[phase_index++].key);return 1;
}
static void numeric_checks(const NpeFile *file){
    NpeWorld w;assert(!npe_world_new_game(file,&w));
    for(unsigned direction=4;direction<256;++direction){w.direction=direction;NpeWorld before=w;unsigned npc=999;NpeMapObject object;
        memset(&object,0xa5,sizeof(object));NpeMapObject old_object=object;
        assert(!npe_world_facing_npc(file,&w,&npc)&&npc==999&&!npe_world_begin_talk(file,&w));
        assert(!npe_world_object_action(file,&w,&object)&&!memcmp(&old_object,&object,sizeof(object))&&!memcmp(&before,&w,sizeof(w)));}
    NpeMap map;NpeMapObject object;assert(!npe_map(file,79,&map)&&!npe_map_object_info(&map,0,&object));
    assert(object.id==6&&object.item_index==69&&object.x==26&&object.y==20);
    assert(!npe_world_init(file,&w,80,26,22));w.direction=2;
    assert(w.x==26&&w.y==22&&w.view.screen_x+w.view.offset_x==26&&w.view.screen_y+w.view.offset_y==21);
    unsigned count=w.condition_words[69];assert(npe_world_object_action(file,&w,&object)==1&&object.id==6&&w.condition_words[69]==count+1&&w.object_done[5]);
    assert(!npe_world_object_action(file,&w,&object));
    puts("Source-backed full-byte no-target and actual map80 object6 viewport-coordinate numeric checks passed");
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    numeric_checks(&file);npe_free(&file);assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<7;++case_index){phase_index=submits=initial_frames=started=finished=0;fake_clock=100;live=NULL;
        char *args[]={"npe-sdl","--new-game",argv[1],NULL};assert(!run(3,args)&&finished&&submits==leader_cases[case_index].commits);
        printf("Leader case %s:%s,actual main completed,commits%u\n",leader_cases[case_index].name,leader_cases[case_index].original_input?"original public input":"source-backed Enter",submits);}
    assert(total==11);puts("Leader actions:five original input paths/11 C-SDL commits,seven initial frames,two source-backed Enter loops,no-target state and close passed");return 0;
}
