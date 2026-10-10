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
typedef struct { unsigned id,x,y,direction; } SimpleNpc;
typedef struct {
    unsigned mode,event,context,map,progress,money,init,sx,sy,vx,vy,direction,enabled,slot;
    unsigned level,experience,threshold,attack,defense,other[3];
    uint8_t npcState[250];SimpleNpc npc[20];
} SimpleState;
typedef struct { const char *name;unsigned mode,event;SimpleState before,after; } SimpleCase;
#include "simple_events_native.h"
static unsigned case_index,active,submits,total,choice_active;
static const SimpleCase *current_case(void){return choice_active?&simple_choices[case_index]:&simple_cases[case_index];}
static void probe_present(SDL_Renderer *);
#define SDL_RenderPresent probe_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent

static void read_bytes(const char *suffix,uint8_t *bytes,unsigned count){
    char path[180];snprintf(path,sizeof(path),"tmp/native-simple-events-%s-%s",current_case()->name,suffix);
    FILE *in=fopen(path,"rb");assert(in&&fread(bytes,1,count,in)==count&&fgetc(in)==EOF&&!fclose(in));
}
static void records(const NpeWorld *w,uint8_t *bytes){
    for(unsigned i=0;i<3;++i)assert(!npe_world_map_record_image(w,i,bytes+148*i));
}
static void configure(NpeWorld *w,const NpeWorld *baseline){
    const SimpleState *s=&current_case()->before;*w=*baseline;
    assert(!npe_world_resources_open((NpeFile *)w->battle_file,w));
    uint8_t before[444],current[444];read_bytes("before.records",before,sizeof(before));
    assert(!npe_world_resident_records_write(w,0x5269c6,before,sizeof(before)));
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;
    w->map_init_pending=(uint8_t)s->init;w->money=s->money;w->story_progress=(uint16_t)s->progress;
    w->view.offset_x=s->vx;w->view.offset_y=s->vy;w->x=s->sx+s->vx;w->y=s->sy+s->vy;
    w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;
    memcpy(w->npc_state,s->npcState,sizeof(w->npc_state));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){
        w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;
    }
    npe_world_seed(w,11);records(w,current);assert(!memcmp(current,before,sizeof(before)));
}
static unsigned compare(const NpeWorld *w,int display){
    const SimpleCase *c=current_case();uint8_t expected[444],current[444];
    read_bytes("after.records",expected,sizeof(expected));records(w,current);
    unsigned d=0;for(unsigned i=0;i<444;++i)if(current[i]!=expected[i]){
        if(d<6)fprintf(stderr,"%s record%u original%u C%u\n",c->name,i,expected[i],current[i]);
        ++d;
    }
    for(unsigned i=0;i<250;++i)d+=w->npc_state[i]!=c->after.npcState[i];
    d+=w->game_mode!=1;d+=w->story_event_id!=c->event;
    d+=w->map_init_pending!=(display?0:1);
    assert(w->random_state==11&&w->money==c->before.money);
    fprintf(stderr,"%s %s state differences=%u\n",c->name,display?"SDL":"numeric",d);
    return d;
}
static void probe_present(SDL_Renderer *r){
    SDL_RenderPresent(r);if(!active)return;assert(submits<(choice_active?2u:1u));
    uint8_t expected[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    char suffix[24];snprintf(suffix,sizeof(suffix),"%u.idx",submits);read_bytes(suffix,expected,sizeof(expected));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(expected);++i){unsigned v=expected[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"simple %s C=%u SDL=%u\n",current_case()->name,d,sd);assert(!d&&!sd);
    char path[180];snprintf(path,sizeof(path),"tmp/c-simple-events-%s-%u.idx",current_case()->name,submits);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected)&&!fclose(out));
    ++submits;++total;
}
int main(int argc,char **argv){
    assert(argc==2||argc==3);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeWorld baseline,w;NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error))&&!npe_world_new_game(&file,&baseline));
    unsigned failures=0;
    for(case_index=0;case_index<18;++case_index){configure(&w,&baseline);const SimpleCase *c=current_case();
        if(c->mode==23)assert(npe_world_apply_experience_event(&w)>=0);
        else{
            /* Numeric callers normally reach24 from a resource continuation. */
            const uint8_t choice[6]={24,1,(uint8_t)(c->event>>8),(uint8_t)c->event,0,0};
            assert(!npe_rpg_runtime_write(&file,0x52781c,choice,sizeof(choice)));
            w.game_mode=17;w.story_event_id=1;assert(npe_world_select_choice(&file,&w,0)==1);
        }
        failures+=compare(&w,0)!=0;npe_rpg_runtime_close(&file);
    }
    if(argc==3){printf("Simple numeric:%u of18 paths differ\n",failures);npe_free(&file);return failures?1:0;}
    assert(!failures&&!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("simple events",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    for(case_index=0;case_index<18;++case_index){configure(&w,&baseline);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        read_bytes("before.idx",pixels,sizeof(pixels));active=0;draw_pixels(renderer,texture,pixels);
        save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&w,renderer,pixels,1);submits=0;active=1;
        assert(advance_world_events(window,renderer,texture,&file,&font,&w,w.game_mode,0)>=0&&submits==1&&!compare(&w,1));
        uint8_t bound[444],current[444];assert(!npe_rpg_runtime_read(&file,0x5269c6,bound,sizeof(bound)));records(&w,current);
        assert(!memcmp(bound,current,sizeof(bound))&&!npe_rpg_runtime_failures(&file));
        active=0;npe_rpg_runtime_close(&file);
    }
    choice_active=1;
    for(case_index=0;case_index<3;++case_index){configure(&w,&baseline);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        read_bytes("before.idx",pixels,sizeof(pixels));active=0;draw_pixels(renderer,texture,pixels);
        save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&w,renderer,pixels,1);submits=0;active=1;
        const uint8_t expected_choice[6]={11,24,0,1,0,60};uint8_t choice[6];
        assert(!npe_rpg_runtime_read(&file,0x527822,choice,sizeof(choice))&&!memcmp(choice,expected_choice,sizeof(choice)));
        unsigned levels[3]={0};assert(!draw_world(renderer,texture,&file,&w,0,0,0,0,0,0,levels,0,&font));
        assert(submits==1&&w.game_mode==17&&w.story_event_id==2&&w.event_context_id==0);
        w.defer_story_events=1;assert(!game_story_choice_input(&file,&w,SDLK_RETURN,renderer,texture));
        assert(advance_world_events(window,renderer,texture,&file,&font,&w,17,0)>=0&&submits==2&&!compare(&w,1));
        assert(!npe_rpg_runtime_read(&file,0x527822,choice,sizeof(choice))&&!memcmp(choice,expected_choice,sizeof(choice)));
        assert(!npe_rpg_runtime_failures(&file));active=0;npe_rpg_runtime_close(&file);
    }
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    printf("Simple events:18 original numeric paths,3 actual-NPE choices,%u original C/SDL frames,complete roles/NPC state and map initialization passed\n",total);
    return 0;
}
