#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"
static SDL_atomic_t compound_ready;
static int compound_wait(SDL_Event *event){SDL_AtomicSet(&compound_ready,1);return SDL_WaitEvent(event);}
#define SDL_WaitEvent compound_wait
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#include "compound_fixture.h"

static unsigned compound_keys_count;
static int compound_keys(void *unused){
    (void)unused;unsigned waited=0;
    while(!SDL_AtomicGet(&compound_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&compound_ready))return -1;
    for(unsigned i=0;i<compound_keys_count;++i){
        SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;
        if(SDL_PushEvent(&event)!=1)return -1;SDL_Delay(30);
    }
    return 0;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile file;NpeFont font={0};char error[128];uint8_t *bytes;size_t size;
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    SDL_Window *window=SDL_CreateWindow("compound probe",0,0,240,120,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,0);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned scenario=0;scenario<sizeof(compound_cases)/sizeof(*compound_cases);++scenario){
        const CompoundCase *c=&compound_cases[scenario];NpeWorld w=compound_fixture(&file,scenario);
        uint8_t saved[42];uint8_t *raw=compound_patch(&file,c,0,saved);compound_check(&w,scenario,0);
        if(!strcmp(c->name,"friend-wrap")||!strcmp(c->name,"friend-qi-wrap")) {
            unsigned ids[18]={c->id};uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],reference[NPE_WIDTH*NPE_HEIGHT];
            assert(!npe_render_native_magic_list_screen(&file,&font,&w,0,ids,1,0,0,pixels));
            char path[128];snprintf(path,sizeof(path),"tmp/native-compound-%s-list0.idx",c->name);
            FILE *input=fopen(path,"rb");assert(input&&fread(reference,1,sizeof(reference),input)==sizeof(reference));
            assert(fgetc(input)==EOF&&!fclose(input));unsigned differences=0;
            for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=reference[i];
            fprintf(stderr,"Compound high QI list %s: %u differences\n",c->name,differences);
            if(differences){FILE *output=fopen("tmp/compound-list-c.idx","wb");assert(output);
                assert(fwrite(pixels,1,sizeof(pixels),output)==sizeof(pixels)&&!fclose(output));}
            assert(!differences);
        }
        for(unsigned round=0;round<c->rounds;++round){
            if(round==2&&c->refresh_changes){uint8_t unused[42];compound_patch(&file,c,1,unused);}
            for(unsigned actor=0;actor<2;++actor){
                if((int16_t)w.hp[actor]<=0)continue;
                assert(w.battle_actor==actor);unsigned use=actor==0&&(round==0||(c->refresh&&round==2));
                compound_keys_count=use&&(!c->side||!c->group)?2:1;
                SDL_AtomicSet(&compound_ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
                SDL_Thread *thread=SDL_CreateThread(compound_keys,"compound keys",NULL);assert(thread);
                if(use&&c->item)assert(show_battle_items(renderer,texture,&file,&font,&w,c->side?1:2)==1);
                else {unsigned spell=use?c->id:42;
                    memset(w.skill_slots[actor],0,sizeof(w.skill_slots[actor]));w.skill_slots[actor][0]=(uint8_t)spell;
                    show_battle_skill(window,renderer,texture,&file,&font,&w);
                }
                int result=-1;SDL_WaitThread(thread,&result);assert(!result);
            }
            compound_check(&w,scenario,round+1);
        }
        memcpy(raw,saved,42);fprintf(stderr,"SDL compound sequence %s passed\n",c->name);
    }
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    printf("SDL compound effects: %u original record phases passed\n",compound_compared);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    npe_font_free(&font);npe_free(&file);return 0;
}
