#include <assert.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"
static SDL_atomic_t ready;
static unsigned calls, caster_after, target_after, skill_after;
static int probe_wait(SDL_Event *event) {SDL_AtomicSet(&ready,1);return SDL_WaitEvent(event);}
static int probe_cast(const NpeFile *,NpeWorld *,unsigned,unsigned,unsigned);
#define SDL_WaitEvent probe_wait
#define npe_world_menu_cast_skill probe_cast
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef npe_world_menu_cast_skill
static int probe_cast(const NpeFile *f,NpeWorld *w,unsigned caster,unsigned target,unsigned skill) {
    ++calls;caster_after=caster;target_after=target;skill_after=skill;
    return npe_world_menu_cast_skill(f,w,caster,target,skill);
}
struct Keys {SDL_Keycode key[14];unsigned count;};
static int send_keys(void *data) {
    struct Keys *keys=data;
    for(unsigned i=0;i<keys->count;++i) {
        for(unsigned elapsed=0;!SDL_AtomicGet(&ready);elapsed+=1){assert(elapsed<8000);SDL_Delay(1);}
        SDL_AtomicSet(&ready,0);SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=keys->key[i];assert(SDL_PushEvent(&event)==1);
    }
    return 0;
}
int main(int argc,char **argv) {
    assert(argc==2);SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
    NpeFile file;uint8_t *bytes;size_t size;char error[128];assert(read_file(argv[1],&bytes,&size)==0);
    assert(npe_parse(&file,bytes,size,error,sizeof(error))==0);
    NpeFont font={0};assert(npe_font_load(&font,"npe-sdl/build/assets/gbk12-native.bin")==0);assert(npe_font_load16(&font,"npe-sdl/build/assets/gbk16-native.bin")==0);
    SDL_Window *window=SDL_CreateWindow("map-magic",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned scenario=0;scenario<9;++scenario) {
        NpeWorld world;assert(npe_world_new_game(&file,&world)==0);world.game_mode=6;
        for(unsigned i=0;i<3;++i){world.level[i]=99;world.hp[i]=50;world.max_hp[i]=200;world.qi[i]=100;world.max_qi[i]=200;world.party_ids[i]=1;world.party_characters[i]=world.party_characters[0];memcpy(world.party_names[i],world.party_names[0],12);memcpy(world.battle_names[i],world.party_names[0],12);world.battle_characters[i]=world.party_characters[0];memset(world.skill_slots[i],0,18);world.skill_slots[i][0]=6;}
        if(scenario==1||scenario==2||scenario==4){world.party_active[1]=world.party_active[2]=1;world.hp[1]=0;world.hp[2]=190;}
        if(scenario==1)world.skill_slots[0][0]=9;
        if(scenario==3){world.skill_slots[0][0]=1;world.hp[0]=0;}
        if(scenario==5){world.skill_slots[0][0]=4;world.skill_slots[0][1]=6;}
        if(scenario>=7){world.skill_slots[0][0]=40;for(unsigned s=0;s<4;++s)world.status_guard[0][s]=(uint16_t)s;}
        if(scenario==8){world.party_active[1]=1;world.hp[1]=0;for(unsigned s=0;s<4;++s)world.status_guard[1][s]=(uint16_t)(10+s);}
        struct Keys keys={{SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_F4,SDLK_F4,SDLK_F4},6};
        if(scenario==1){keys.key[2]=SDLK_F4;keys.key[3]=SDLK_F4;keys.count=4;}
        if(scenario==2||scenario==8){keys.key[2]=SDLK_DOWN;keys.key[3]=SDLK_RETURN;keys.key[4]=keys.key[5]=keys.key[6]=SDLK_F4;keys.count=7;}
        if(scenario==4){SDL_Keycode k[]={SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,SDLK_RETURN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,SDLK_F4,SDLK_F4,SDLK_F4};memcpy(keys.key,k,sizeof(k));keys.count=10;}
        if(scenario==5){SDL_Keycode k[]={SDLK_RETURN,SDLK_RIGHT,SDLK_RETURN,SDLK_RETURN,SDLK_F4,SDLK_F4,SDLK_F4};memcpy(keys.key,k,sizeof(k));keys.count=7;}
        if(scenario==6){keys.key[2]=SDLK_F4;keys.count=5;}
        /* The original F4 exits the whole map menu in one press. */
        for(unsigned i=0;i<keys.count;++i)if(keys.key[i]==SDLK_F4){keys.count=i+1;break;}
        SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);SDL_AtomicSet(&ready,0);calls=0;
        SDL_Thread *thread=SDL_CreateThread(send_keys,"map-magic-keys",&keys);assert(thread);
        assert(!npe_world_resources_open(&file,&world));
        show_magic_list(window,renderer,texture,&file,&font,&world);SDL_WaitThread(thread,NULL);
        assert(calls==(scenario==6?0u:1u));
        if(scenario!=6){assert(caster_after==(scenario==4?2u:0u));assert(target_after==(scenario==4?2u:(scenario==2||scenario==8)?1u:0u));assert(skill_after==(scenario>=7?40u:scenario==1?9u:scenario==3?1u:6u));}
        if(scenario==0||scenario==5){assert(world.hp[0]==150&&world.qi[0]==89);}
        if(scenario==1){assert(world.hp[0]==200&&world.hp[1]==200&&world.hp[2]==200&&world.qi[0]==80);}
        if(scenario==2){assert(world.hp[1]==100&&world.hp[0]==50&&world.qi[0]==89);}
        if(scenario==3){assert(world.hp[0]==0&&world.qi[0]==20);}
        if(scenario==4){assert(world.hp[2]==200&&world.qi[2]==89&&world.qi[0]==100);}
        if(scenario==6){assert(world.hp[0]==50&&world.qi[0]==100);}
        if(scenario==7){assert(world.hp[0]==50&&world.qi[0]==84);for(unsigned s=0;s<4;++s)assert(world.status_guard[0][s]==s+3);}
        if(scenario==8){assert(world.hp[1]==0&&world.qi[0]==84);for(unsigned s=0;s<4;++s){assert(world.status_guard[1][s]==s+13);assert(world.status_guard[0][s]==s);}}
        fprintf(stderr,"SDL map magic scenario %u passed\n",scenario);
        npe_rpg_runtime_close(&file);
    }
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_font_free(&font);npe_free(&file);SDL_Quit();return 0;
}
