#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"
static SDL_atomic_t status_item_ready;
static unsigned status_item_magic_gate,status_item_magic_draws,status_item_magic_variant;
static int status_item_magic_list(const NpeFile *,const NpeFont *,const NpeWorld *,unsigned,
                                  const unsigned *,unsigned,unsigned,unsigned,uint8_t *);
static int status_item_wait(SDL_Event *event){SDL_AtomicSet(&status_item_ready,1);return SDL_WaitEvent(event);}
#define SDL_WaitEvent status_item_wait
#define npe_render_native_magic_list_screen status_item_magic_list
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef npe_render_native_magic_list_screen
#include "status_items_fixture.h"
static unsigned status_item_key_count,status_item_move;
static SDL_Keycode status_item_move_key;
static int status_item_magic_list(const NpeFile *file,const NpeFont *font,const NpeWorld *w,unsigned member,
                                  const unsigned *ids,unsigned count,unsigned first,unsigned selection,uint8_t *pixels){
    int result=npe_render_native_magic_list_screen(file,font,w,member,ids,count,first,selection,pixels);
    if(status_item_magic_gate){
        ++status_item_magic_draws;assert(!result);
        const char *names[]={"negative","zero","positive"};char path[128];
        snprintf(path,sizeof(path),"tmp/native-magic-seal-%s-after.idx",names[status_item_magic_variant]);
        uint8_t reference[NPE_WIDTH*NPE_HEIGHT];FILE *input=fopen(path,"rb");
        assert(input&&fread(reference,1,sizeof(reference),input)==sizeof(reference)&&fgetc(input)==EOF&&!fclose(input));
        unsigned differences=0;for(unsigned i=0;i<sizeof(reference);++i)differences+=pixels[i]!=reference[i];
        fprintf(stderr,"Original magic seal %s list: %u differences\n",names[status_item_magic_variant],differences);
        if(differences){FILE *output=fopen("tmp/magic-seal-list-c.idx","wb");assert(output);
            assert(fwrite(pixels,1,sizeof(reference),output)==sizeof(reference)&&!fclose(output));}
        assert(!differences);
    }
    return result;
}
static int status_item_keys(void *unused){
    (void)unused;unsigned waited=0;
    while(!SDL_AtomicGet(&status_item_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&status_item_ready))return -1;
    for(unsigned i=0;i<status_item_key_count;++i){
        SDL_Event event={0};event.type=SDL_KEYDOWN;
        event.key.keysym.sym=status_item_magic_gate?SDLK_F4:
            status_item_move&&i>=1&&i<=status_item_move?status_item_move_key:SDLK_RETURN;
        if(SDL_PushEvent(&event)!=1)return -1;SDL_Delay(30);
    }
    return 0;
}
static void status_item_magic_gates(const NpeFile *file,const NpeFont *font,SDL_Window *window,SDL_Renderer *renderer,SDL_Texture *texture){
    const int16_t values[]={-5,0,5};
    for(status_item_magic_variant=0;status_item_magic_variant<3;++status_item_magic_variant){
        NpeWorld w;assert(!npe_world_init(file,&w,9,29,13));w.game_mode=3;w.story_event_id=1;
        assert(npe_world_prepare_scripted_battle(file,&w)==1);
        w.hp[0]=500;w.max_hp[0]=1000;w.qi[0]=500;w.max_qi[0]=999;w.level[0]=99;
        memset(w.skill_slots[0],0,sizeof(w.skill_slots[0]));w.skill_slots[0][0]=42;
        w.battle_party_status[0][3]=values[status_item_magic_variant];NpeWorld before=w;
        status_item_magic_gate=1;status_item_magic_draws=0;SDL_AtomicSet(&status_item_ready,0);
        SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        if(!status_item_magic_variant){
            assert(npe_world_battle_cast_magic(file,&w,42,0)==-5&&!memcmp(&w,&before,sizeof(w)));
            show_battle_skill(window,renderer,texture,file,font,&w);
            assert(!SDL_AtomicGet(&status_item_ready)&&!status_item_magic_draws);
        }else{
            status_item_key_count=1;SDL_Thread *thread=SDL_CreateThread(status_item_keys,"seal gate keys",NULL);assert(thread);
            show_battle_skill(window,renderer,texture,file,font,&w);int result=-1;SDL_WaitThread(thread,&result);
            assert(!result&&status_item_magic_draws==1);
        }
        assert(!memcmp(&w,&before,sizeof(w)));status_item_magic_gate=0;
    }
    puts("Original fourth-state magic entry: negative no-op, zero/positive list and no command/QI consumption passed");
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile file;NpeFont font={0};char error[128];uint8_t *bytes;size_t size;
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    SDL_Window *window=SDL_CreateWindow("status item probe",0,0,240,120,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,0);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned scenario=0;scenario<sizeof(status_item_cases)/sizeof(*status_item_cases);++scenario){
        const StatusItemCase *c=&status_item_cases[scenario];NpeWorld w=status_item_fixture(&file,scenario);
        uint8_t saved[42];uint8_t *raw=status_item_patch(&file,c,saved);status_item_check(&w,scenario,0);
        for(unsigned round=0;round<c->rounds;++round){
            for(unsigned actor=0;actor<c->party;++actor){
                if((int16_t)w.hp[actor]<=0||w.battle_party_status[actor][1]<0||w.battle_party_status[actor][2]<0)continue;assert(w.battle_actor==actor);
                unsigned use=actor==0&&(round==0||c->refresh);
                if(!use&&c->defend){status_item_numeric_action(&file,&w,c,0);continue;}
                status_item_move=use&&!c->group?c->target:0;
                status_item_move_key=c->side?SDLK_UP:SDLK_DOWN;
                status_item_key_count=use&&(!c->side||!c->group)?2:1;
                status_item_key_count+=status_item_move;
                SDL_AtomicSet(&status_item_ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
                SDL_Thread *thread=SDL_CreateThread(status_item_keys,"status item keys",NULL);assert(thread);
                if(use)assert(show_battle_items(renderer,texture,&file,&font,&w,c->side?1:2)==1);
                else {memset(w.skill_slots[actor],0,sizeof(w.skill_slots[actor]));w.skill_slots[actor][0]=42;
                    show_battle_skill(window,renderer,texture,&file,&font,&w);}
                int result=-1;SDL_WaitThread(thread,&result);assert(!result);
            }
            status_item_check(&w,scenario,round+1);
        }
        memcpy(raw,saved,42);fprintf(stderr,"SDL status item %s passed\n",c->name);
    }
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    status_item_magic_gates(&file,&font,window,renderer,texture);
    printf("SDL status items: %u original record phases passed\n",status_items_compared);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    npe_font_free(&font);npe_free(&file);return 0;
}
