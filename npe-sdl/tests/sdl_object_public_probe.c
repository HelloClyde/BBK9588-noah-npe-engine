#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned mode,event,map,progress,direction,sx,sy,vx,vy,quantity,flag;} ObjectState;
typedef struct {
    const char *name;unsigned commits;ObjectState before,after,commit[3];
    uint8_t objects[64],flags[200],npc[280],monsters[56],chase[28],after_objects[64],after_flags[200];
#ifdef NPE_OBJECT_STORY_ALIAS
    unsigned id,item_alias;uint8_t story[90],after_story[90];
#endif
} ObjectPublicCase;
#ifdef NPE_OBJECT_STORY_ALIAS
#include "object_story_alias_native.h"
#else
#include "object_public_native.h"
#endif
static unsigned case_index,phase,inputs,submits,dismissed,finished,total;
static NpeWorld *live;
static NpeFile *fixture_file;
static Uint32 fake_clock;
static uint8_t retained_map[28800];
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 ms){fake_clock+=ms;}
static int wait_input(SDL_Event *,int),poll_input(SDL_Event *);
static void present(SDL_Renderer *),select_sound(NpeSoundPlayer *,NpeWorld *);
static SDL_Window *create_window(const char *name,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;return SDL_CreateWindow(name,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_RenderPresent present
#define SDL_WaitEventTimeout wait_input
#define SDL_PollEvent poll_input
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_CreateWindow create_window
#define npe_sound_select select_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
#undef SDL_WaitEventTimeout
#undef SDL_PollEvent
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_CreateWindow
#undef npe_sound_select
static void check_state(const ObjectState *s){
    assert(live->game_mode==s->mode&&live->story_event_id==s->event&&live->map_number==s->map&&live->story_progress==s->progress);
    assert(live->direction==s->direction&&live->view.screen_x==s->sx&&live->view.screen_y==s->sy&&live->view.offset_x==s->vx&&live->view.offset_y==s->vy);
    assert(live->condition_words[69]==s->quantity);
#ifdef NPE_OBJECT_STORY_ALIAS
    assert(npe_world_object_flag(live,object_public_cases[case_index].id)==(int)s->flag);
#else
    assert(live->object_done[5]==s->flag);
#endif
}
static void check_end(void){
    const ObjectPublicCase *c=&object_public_cases[case_index];uint8_t bytes[200];check_state(&c->after);
    assert(!npe_world_resident_read(live,0x52759e,bytes,64)&&!memcmp(bytes,c->after_objects,64));
    assert(!npe_world_resident_read(live,0x52742c,bytes,200)&&!memcmp(bytes,c->after_flags,200));
#ifdef NPE_OBJECT_RUNTIME_BOUND
    assert(npe_rpg_runtime_owner(live->battle_file)==live);
    assert(npe_rpg_runtime_map_current(live->battle_file,live->map_number)==1);
    if(c->commits){
        /* The original acted paths clear the selected physical cell to53.
           Read the actual heap after pickup and again after notice/repeat. */
        int x=(int)(c->before.vx+c->before.sx)-1;
        int y=(int)(c->before.vy+c->before.sy);
        unsigned slot=0;
        while(slot<8 && (c->objects[slot*8+6]!=x || c->objects[slot*8+7]!=y))++slot;
        assert(slot<8);
        uint8_t width;uint16_t tile;
        assert(!npe_rpg_runtime_read(live->battle_file,0x527a56,&width,1));
        uint16_t index=(uint16_t)(c->after_objects[slot*8+7]*width+c->after_objects[slot*8+6]);
        assert(npe_rpg_runtime_map_tile(live->battle_file,live->map_number,index,&tile)==1 && tile==53);
    }
#endif
#ifdef NPE_OBJECT_STORY_ALIAS
    assert(!npe_world_resident_read(live,0x5274f4,bytes,90)&&!memcmp(bytes,c->after_story,90));
#endif
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){const ObjectPublicCase *c=&object_public_cases[case_index];live=w;NpeWorld previous=*w;
        assert(!npe_world_init(fixture_file,w,80,27,20));
#ifdef NPE_OBJECT_RUNTIME_BOUND
        /* Exercise the actual map heap, not the diagnostic World-only
           fallback. Keep the main file owner and load the selected map. */
        w->battle_file=previous.battle_file;
        assert(!npe_rpg_runtime_map_load(previous.battle_file,w,0));
#endif
        w->battle_animation_observer=previous.battle_animation_observer;w->battle_animation_context=previous.battle_animation_context;
        w->sound_observer=previous.sound_observer;w->sound_context=previous.sound_context;
        const ObjectState *s=&c->before;
        w->view.offset_x=s->vx;w->view.offset_y=s->vy;w->view.screen_x=s->sx;w->view.screen_y=s->sy;
        w->x=(uint16_t)(s->vx+s->sx);w->y=(uint16_t)(s->vy+s->sy);
        w->direction=(uint8_t)(s->direction>3?3:s->direction);w->music_enabled=0;w->map_init_pending=0;w->lcd_background=0;
        w->condition_words[69]=(uint16_t)s->quantity;
        assert(!npe_world_resident_write(w,0x52759e,c->objects,64));
        assert(!npe_world_resident_write(w,0x52742c,c->flags,200));
#ifdef NPE_OBJECT_STORY_ALIAS
        assert(!npe_world_resident_write(w,0x5274f4,c->story,90));
        uint8_t item_alias=(uint8_t)c->item_alias;
        assert(!npe_world_resident_write(w,0x52742a,&item_alias,1));
#endif
        assert(!npe_world_resident_write(w,0x526ca2,c->npc,280));
        assert(!npe_world_resident_write(w,0x52761a,c->monsters,56));
        assert(!npe_world_resident_write(w,0x52783a,c->chase,28));
        NpeMap map;assert(!npe_map(fixture_file,79,&map)&&!npe_world_load_tiles(w,&map));npe_world_seed(w,11);
    }
    npe_sound_select(p,w);SDL_PauseAudioDevice(p->device,1);
}
static void present(SDL_Renderer *r){
    SDL_RenderPresent(r);if(!phase){phase=1;return;}
    const ObjectPublicCase *c=&object_public_cases[case_index];
    if(submits==2){check_end();assert(!memcmp(last_pixel_frame,retained_map,28800));++submits;return;}
    assert(submits<c->commits);check_state(&c->commit[submits]);
#ifdef NPE_OBJECT_STORY_ALIAS
    const char *prefix="object-story-alias";
#else
    const char *prefix="object-public";
#endif
    char path[128];snprintf(path,sizeof(path),"tmp/native-%s-%u-commit-%u.idx",prefix,case_index,submits);
    uint8_t original[28800];FILE *f=fopen(path,"rb");assert(f&&fread(original,1,sizeof(original),f)==sizeof(original)&&fgetc(f)==EOF&&!fclose(f));
    uint32_t rgba[28800];assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<28800;++i){unsigned v=original[i];
        differences+=last_pixel_frame[i]!=v;
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-last_pixel_frame[i]*142/15)<<16)|((200-last_pixel_frame[i]*146/15)<<8)|(161-last_pixel_frame[i]*119/15)));}
    fprintf(stderr,"Original public pickup %s commit%u:pixel differences%u\n",c->name,submits,differences);assert(!differences);
#ifdef NPE_OBJECT_RUNTIME_BOUND
    snprintf(path,sizeof(path),"tmp/c-runtime-%s-%u-commit-%u.idx",prefix,case_index,submits);
#else
    snprintf(path,sizeof(path),"tmp/c-%s-%u-commit-%u.idx",prefix,case_index,submits);
#endif
    f=fopen(path,"wb");assert(f&&fwrite(last_pixel_frame,1,28800,f)==28800&&!fclose(f));
    if(!submits)memcpy(retained_map,last_pixel_frame,28800);
    ++submits;++total;
}
static int poll_input(SDL_Event *e){
    memset(e,0,sizeof(*e));if(submits==2&&!dismissed){dismissed=1;e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F4;return 1;}return 0;
}
static int wait_input(SDL_Event *e,int timeout){
    (void)timeout;memset(e,0,sizeof(*e));const ObjectPublicCase *c=&object_public_cases[case_index];
    if(!inputs&&c->before.direction>3){live->direction=(uint8_t)c->before.direction;world_map_frame_state.direction=live->direction;}
    if(inputs){check_end();assert(submits==(c->commits?3u:0u));}
    if(inputs==2){finished=1;e->type=SDL_QUIT;return 1;}
    ++inputs;e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));fixture_file=&file;
    uint64_t fingerprint=npe_game_fingerprint(&file);
    for(case_index=0;case_index<sizeof(object_public_cases)/sizeof(*object_public_cases);++case_index){
        live=NULL;phase=inputs=submits=dismissed=finished=0;fake_clock=100;
        world_map_frame_valid=world_map_saved_edges_valid=0;
        char *args[]={"npe-sdl","--new-game",argv[1],NULL};assert(!npe_app_main(3,args)&&finished);
    }
    assert(npe_game_fingerprint(&file)==fingerprint);npe_free(&file);
    printf("Original public pickup:%u paths,%u captured C/SDL frames,resident flag/object end states and repeat passed; C restoration checked separately\n",
           (unsigned)(sizeof(object_public_cases)/sizeof(*object_public_cases)),total);return 0;
}
