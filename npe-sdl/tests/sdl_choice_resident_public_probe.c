#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned mode,event,context,map,progress,init;} ChoiceState;
typedef struct {unsigned key,commits;ChoiceState after;} ChoicePhase;
typedef struct {const char *name;ChoiceState before;uint8_t records[30];unsigned alias_address;uint8_t alias[6];unsigned phases;ChoicePhase phase[8];} ChoicePublicCase;
#include "choice_resident_public_native.h"
static unsigned active,case_index,submits,total;
#include <SDL.h>
#ifdef main
#undef main
#endif
static void probe_present(SDL_Renderer *);
#define SDL_RenderPresent probe_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
static void read_frame(unsigned before,unsigned index,uint8_t *pixels){
    char path[180];const char *name=choice_public_cases[case_index].name;
    if(before)snprintf(path,sizeof(path),"tmp/native-choice-resident-public-%s-before.idx",name);
    else snprintf(path,sizeof(path),"tmp/native-choice-resident-public-%s-%u.idx",name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"choice %s commit%u C=%u SDL=%u\n",choice_public_cases[case_index].name,submits,d,sd);assert(!d&&!sd);
    char path[180];snprintf(path,sizeof(path),"tmp/c-choice-resident-public-%s-%u.idx",choice_public_cases[case_index].name,submits);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));++submits;++total;
}
static void check(const NpeWorld *w,const ChoiceState *s){
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->event_context_id==s->context&&w->map_number==s->map&&w->story_progress==s->progress&&w->map_init_pending==s->init);
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];NpeWorld baseline,w;NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    assert(!npe_world_new_game(&f,&baseline));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("choice resident",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    for(case_index=0;case_index<10;++case_index){const ChoicePublicCase *c=&choice_public_cases[case_index];const ChoiceState *s=&c->before;
        w=baseline;w.game_mode=17;w.story_event_id=(uint16_t)s->event;w.event_context_id=(uint16_t)s->context;w.map_init_pending=(uint8_t)s->init;
        assert(!npe_world_resources_open(&f,&w)&&!npe_rpg_runtime_write(&f,0x52781c,c->records,30));
        assert(!npe_rpg_runtime_write(&f,0x520000+c->alias_address,c->alias,6));
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);active=0;draw_pixels(renderer,texture,pixels);
        save_map_edges(&f,renderer,pixels);retain_world_frame(&f,&w,renderer,pixels,0);submits=0;active=1;
        for(unsigned p=0;p<c->phases;++p){const ChoicePhase *phase=&c->phase[p];unsigned before=submits;
            if(!p){unsigned levels[3]={0};assert(!draw_world(renderer,texture,&f,&w,0,0,0,0,0,0,levels,0,&font));}
            else{
                SDL_Keycode key=phase->key==1?SDLK_UP:phase->key==2?SDLK_DOWN:phase->key==4?SDLK_LEFT:phase->key==8?SDLK_RIGHT:phase->key==16?SDLK_RETURN:phase->key==32?SDLK_BACKSPACE:SDLK_F4;
                assert(!game_story_choice_input(&f,&w,key,renderer,texture));
                assert(advance_world_events(window,renderer,texture,&f,&font,&w,17,0)>=0);
            }
            assert(submits-before==phase->commits);check(&w,&phase->after);
        }
        active=0;uint8_t records[30],alias[6];
        assert(!npe_rpg_runtime_read(&f,0x52781c,records,30)&&!memcmp(records,c->records,30));
        assert(!npe_rpg_runtime_read(&f,0x520000+c->alias_address,alias,6)&&!memcmp(alias,c->alias,6));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
    }
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&f);
    printf("Choice resident:10 original paths,%u C/SDL commits,word-index aliases and original controls passed\n",total);return 0;
}
