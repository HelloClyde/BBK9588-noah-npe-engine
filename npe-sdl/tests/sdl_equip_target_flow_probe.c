#include <assert.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"
static SDL_atomic_t ready;
static unsigned equip_calls, page_calls, dialogs, checking_depletion, depleted_frames;
static unsigned checking_quantity, quantity_lists;
static int probe_runtime_page(const NpeFile *,const NpeFont *,NpeWorld *,unsigned,unsigned,unsigned,int,uint8_t *);
static int probe_runtime_list(const NpeFile *,const NpeFont *,NpeWorld *,const unsigned *,const unsigned *,unsigned,unsigned,unsigned,int,uint8_t *);
static void compare(const char *,const uint8_t *);
static int probe_wait(SDL_Event *event) {SDL_AtomicSet(&ready,1);return SDL_WaitEvent(event);}
static int probe_equip(const NpeFile *,NpeWorld *,unsigned,unsigned);
static int probe_page(const NpeFile *,const NpeFont *,const NpeWorld *,unsigned,unsigned,unsigned,uint8_t *);
static int probe_list(const NpeFile *,const NpeFont *,const unsigned *,const unsigned *,unsigned,unsigned,uint8_t *);
static int probe_dialog(Uint32 flags,const char *title,const char *message,SDL_Window *window) {
    (void)flags;(void)title;(void)message;(void)window;++dialogs;return 0;
}
#define SDL_WaitEvent probe_wait
#define SDL_ShowSimpleMessageBox probe_dialog
#define npe_world_equip_item probe_equip
#define npe_render_native_equip_target_screen probe_page
#define npe_render_item_list_screen probe_list
#define npe_render_equip_target_screen probe_runtime_page
#define npe_render_map_item_list probe_runtime_list
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef SDL_ShowSimpleMessageBox
#undef npe_world_equip_item
#undef npe_render_native_equip_target_screen
#undef npe_render_item_list_screen
#undef npe_render_equip_target_screen
#undef npe_render_map_item_list

static int probe_equip(const NpeFile *file,NpeWorld *world,unsigned id,unsigned member) {
    ++equip_calls;return npe_world_equip_item(file,world,id,member);
}
static int probe_page(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                      unsigned member,unsigned id,unsigned quantity,uint8_t *pixels) {
    ++page_calls;int result=npe_render_native_equip_target_screen(file,font,world,member,id,quantity,pixels);
    if(!result&&checking_depletion&&!quantity&&id==85){assert(member==1);compare("tmp/equip-menu-3-deplete-key12.idx",pixels);++depleted_frames;}
    if(!result&&checking_quantity&&id==85&&quantity==99)compare("tmp/equip-menu-1-quantity-key7.idx",pixels);
    return result;
}
static int probe_list(const NpeFile *file,const NpeFont *font,const unsigned *ids,
                      const unsigned *quantities,unsigned count,unsigned selection,uint8_t *pixels) {
    int result=npe_render_item_list_screen(file,font,ids,quantities,count,selection,pixels);
    if(!result&&checking_quantity){
        assert(count==1&&ids[0]==85&&(quantities[0]==100||quantities[0]==99));
        compare(quantities[0]==100?"tmp/equip-menu-1-quantity-key5.idx":"tmp/equip-menu-1-quantity-key8.idx",pixels);
        ++quantity_lists;
    }
    return result;
}
struct Keys {SDL_Keycode key[32];unsigned count;};
static int probe_runtime_page(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                              unsigned member,unsigned id,unsigned quantity,int full,uint8_t *pixels){
    ++page_calls;int result=npe_render_equip_target_screen(file,font,world,member,id,quantity,full,pixels);
    if(!result&&checking_depletion&&!quantity&&id==85){assert(member==1);compare("tmp/equip-menu-3-deplete-key12.idx",pixels);++depleted_frames;}
    if(!result&&checking_quantity&&id==85&&quantity==99)compare("tmp/equip-menu-1-quantity-key7.idx",pixels);
    return result;
}
static int probe_runtime_list(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                              const unsigned *ids,const unsigned *quantities,unsigned count,
                              unsigned selection,unsigned previous,int full,uint8_t *pixels){
    int result=npe_render_map_item_list(file,font,world,ids,quantities,count,selection,previous,full,pixels);
    if(!result&&checking_quantity){
        assert(count==1&&ids[0]==85&&(quantities[0]==100||quantities[0]==99));
        compare(quantities[0]==100?"tmp/equip-menu-1-quantity-key5.idx":"tmp/equip-menu-1-quantity-key8.idx",pixels);++quantity_lists;
    }
    return result;
}
static int send_keys(void *data) {
    struct Keys *keys=data;
    for (unsigned i=0;i<keys->count;++i) {
        for (unsigned elapsed=0;!SDL_AtomicGet(&ready);++elapsed) {assert(elapsed<8000);SDL_Delay(1);}
        SDL_AtomicSet(&ready,0);SDL_Event event={0};event.type=SDL_KEYDOWN;
        event.key.keysym.sym=keys->key[i];assert(SDL_PushEvent(&event)==1);
    }
    return 0;
}
static void compare(const char *path,const uint8_t *pixels) {
    FILE *stream=fopen(path,"rb");assert(stream);unsigned differences=0;
    for (unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i) {int c=fgetc(stream);assert(c>=0);differences+=pixels[i]!=(uint8_t)c;}
    assert(fgetc(stream)==EOF&&fclose(stream)==0);
    fprintf(stderr,"Original equip page %s: %u differences\n",path,differences);
    if(differences){stream=fopen("tmp/equip-target-actual.idx","wb");assert(stream);assert(fwrite(pixels,1,NPE_WIDTH*NPE_HEIGHT,stream)==NPE_WIDTH*NPE_HEIGHT&&fclose(stream)==0);}
    assert(!differences);
}
static NpeWorld fixture(const NpeFile *file,unsigned count) {
    NpeWorld world;assert(npe_world_new_game(file,&world)==0);world.game_mode=6;
    memset(world.condition_words,0,sizeof(world.condition_words));world.condition_words[85]=2;world.condition_words[91]=1;
    for(unsigned m=0;m<3;++m) {
        world.party_active[m]=m<count;world.party_ids[m]=1;world.party_characters[m]=world.party_characters[0];memcpy(world.party_names[m],world.party_names[0],12);memcpy(world.battle_names[m],world.party_names[0],12);world.battle_characters[m]=world.party_characters[0];
        world.attack[m]=world.attack[0];world.defense[m]=world.defense[0];
        memcpy(world.other_stats[m],world.other_stats[0],sizeof(world.other_stats[0]));
    }
    return world;
}
int main(int argc,char **argv) {
    assert(argc==2);SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
    NpeFile file;uint8_t *bytes;size_t size;char error[128];assert(read_file(argv[1],&bytes,&size)==0);
    assert(npe_parse(&file,bytes,size,error,sizeof(error))==0);
    NpeFont font={0};assert(npe_font_load(&font,"npe-sdl/build/assets/gbk12-native.bin")==0);assert(npe_font_load16(&font,"npe-sdl/build/assets/gbk16-native.bin")==0);
    SDL_Window *window=SDL_CreateWindow("equip-menu",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned count=1;count<=3;++count) {
        NpeWorld world=fixture(&file,count);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        assert(npe_render_native_equip_target_screen(&file,&font,&world,0,85,2,pixels)==0);
        char path[128];snprintf(path,sizeof(path),"tmp/equip-menu-%u-key6.idx",count);compare(path,pixels);
        /* Match original key6..key21: upward wrap, four downward moves,
           repeated equip, ignored horizontal arrows, return, replace. */
        struct Keys keys={{SDLK_RETURN,SDLK_UP,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,
                           SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_F4,SDLK_RIGHT,
                           SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_F4,SDLK_F4},16};
        equip_calls=page_calls=dialogs=0;SDL_AtomicSet(&ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        SDL_Thread *thread=SDL_CreateThread(send_keys,"equip-keys",&keys);assert(thread);
        show_native_equip_choice(window,renderer,texture,&file,&font,&world);SDL_WaitThread(thread,NULL);
        assert(world.game_mode==1&&equip_calls==4&&page_calls>=(count==1?4:10)&&!dialogs);
        unsigned first=count==1?0:count==2?1:0,second=count==1?0:1;
        for(unsigned m=0;m<count;++m)assert(world.equipped_items[m][5]==(m==second?91:m==first?85:0));
        assert(world.condition_words[85]==(first==second?2:1)&&!world.condition_words[91]);
        assert(npe_render_native_equip_target_screen(&file,&font,&world,second,91,0,pixels)==0);
        snprintf(path,sizeof(path),"tmp/equip-menu-%u-key19.idx",count);compare(path,pixels);
        const unsigned ids[]={85,91},quantities[]={1,1};
        assert(npe_render_item_list_screen(&file,&font,ids,quantities,2,1,pixels)==0);
        snprintf(path,sizeof(path),"tmp/equip-menu-%u-key16.idx",count);compare(path,pixels);
        fprintf(stderr,"SDL original equip %u members: wrap, repeat, replacement and return passed\n",count);
    }
    {
        NpeWorld world=fixture(&file,3);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        struct Keys keys={{SDLK_RETURN,SDLK_LEFT,SDLK_RIGHT,SDLK_UP,SDLK_RETURN,
                           SDLK_UP,SDLK_RETURN,SDLK_UP,SDLK_RETURN,SDLK_F4,
                           SDLK_RETURN,SDLK_RETURN,SDLK_F4,SDLK_F4},14};
        checking_depletion=1;equip_calls=page_calls=dialogs=depleted_frames=0;
        SDL_AtomicSet(&ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        SDL_Thread *thread=SDL_CreateThread(send_keys,"equip-deplete",&keys);assert(thread);
        show_native_equip_choice(window,renderer,texture,&file,&font,&world);SDL_WaitThread(thread,NULL);
        checking_depletion=0;
        assert(world.game_mode==1&&equip_calls==3&&!dialogs&&depleted_frames==1);
        assert(world.equipped_items[0][5]==91&&world.equipped_items[1][5]==85&&world.equipped_items[2][5]==85);
        assert(!world.condition_words[85]&&!world.condition_words[91]);
        assert(npe_render_native_equip_target_screen(&file,&font,&world,0,91,0,pixels)==0);
        compare("tmp/equip-menu-3-deplete-key17.idx",pixels);
        assert(npe_render_empty_inventory_screen(&font,pixels)==0);compare("tmp/equip-menu-3-deplete-key18.idx",pixels);
        fprintf(stderr,"SDL original depletion: horizontal keys, zero guard, list rebuild and map return passed\n");
    }
    const char *variants[]={"positive","negative","p2","p3"};
    for(unsigned v=0;v<4;++v) {
        NpeWorld world=fixture(&file,1);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        for(unsigned s=0;s<4;++s)world.status_guard[0][s]=v==0?5:v==1?(uint16_t)-5:s==v?5:0;
        assert(npe_render_native_equip_target_screen(&file,&font,&world,0,85,2,pixels)==0);
        char path[128];snprintf(path,sizeof(path),"tmp/equip-menu-1-%s-key6.idx",variants[v]);compare(path,pixels);
    }
    {
        NpeWorld world=fixture(&file,1);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        world.condition_words[85]=100;world.condition_words[91]=UINT16_MAX;
        const unsigned ids[]={85},quantities[]={100};
        assert(npe_render_item_list_screen(&file,&font,ids,quantities,1,0,pixels)==0);
        compare("tmp/equip-menu-1-quantity-key5.idx",pixels);
        assert(npe_render_native_equip_target_screen(&file,&font,&world,0,85,100,pixels)==0);
        compare("tmp/equip-menu-1-quantity-key6.idx",pixels);
        struct Keys keys={{SDLK_RETURN,SDLK_RETURN,SDLK_F4,SDLK_F4},4};
        checking_quantity=1;quantity_lists=0;
        equip_calls=page_calls=dialogs=0;SDL_AtomicSet(&ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        SDL_Thread *thread=SDL_CreateThread(send_keys,"equip-quantity",&keys);assert(thread);
        show_native_equip_choice(window,renderer,texture,&file,&font,&world);SDL_WaitThread(thread,NULL);
        checking_quantity=0;
        assert(world.game_mode==1&&equip_calls==1&&!dialogs);
        assert(quantity_lists==2);
        assert(world.equipped_items[0][5]==85&&world.condition_words[85]==99&&world.condition_words[91]==UINT16_MAX);
        assert(npe_world_equip_item(&file,&world,91,0)==-1);
        fprintf(stderr,"SDL original equipment quantity100 and signed-negative inventory guard passed\n");
    }
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_font_free(&font);npe_free(&file);SDL_Quit();return 0;
}
