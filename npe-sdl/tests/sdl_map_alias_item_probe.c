#include <assert.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"
static SDL_atomic_t ready,refusal;
static int variant,selected;
static const char *stems[]={"alias-menu-3-69-group-status","alias-menu-3-69-group-status-positive","alias-menu-3-69-group-status-redirect","alias-menu-3-69-group-status-wrap"};
static unsigned screens;
static int updating;
static int probe_runtime_list(const NpeFile *,const NpeFont *,NpeWorld *,const unsigned *,const unsigned *,unsigned,unsigned,unsigned,int,uint8_t *);
static int probe_list(const NpeFile *,const NpeFont *,const unsigned *,const unsigned *,unsigned,unsigned,uint8_t *);
static int probe_update(const NpeFile *,const NpeFont *,const unsigned *,const unsigned *,unsigned,unsigned,uint8_t *);
static int probe_target(const NpeFile *,const NpeFont *,const NpeWorld *,unsigned,unsigned,unsigned,int,uint8_t *);
static int probe_rejection(const NpeFont *,unsigned,uint8_t *);
static int probe_wait(SDL_Event *event){SDL_AtomicSet(&ready,1);return SDL_WaitEvent(event);}
#define SDL_WaitEvent probe_wait
#define npe_render_map_item_list probe_runtime_list
#define npe_render_item_list_screen probe_list
#define npe_update_item_list_screen probe_update
#define npe_render_map_item_target_view probe_target
#define npe_render_map_item_rejection probe_rejection
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef npe_render_map_item_list
#undef npe_render_item_list_screen
#undef npe_update_item_list_screen
#undef npe_render_map_item_target_view
#undef npe_render_map_item_rejection

static void compare(unsigned key,const uint8_t *pixels){
    static uint8_t seen[20][NPE_WIDTH*NPE_HEIGHT];
    char path[180];snprintf(path,sizeof(path),"tmp/%s-key%u.idx",stems[variant],key);
    FILE *stream=fopen(path,"rb");assert(stream);unsigned differences=0;
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){int c=fgetc(stream);assert(c>=0);differences+=pixels[i]!=(uint8_t)c;}
    assert(fgetc(stream)==EOF&&fclose(stream)==0);
    fprintf(stderr,"Original alias screen %s: %u differences\n",path,differences);
    if(differences){stream=fopen("tmp/alias-actual.idx","wb");assert(stream);assert(fwrite(pixels,1,NPE_WIDTH*NPE_HEIGHT,stream)==NPE_WIDTH*NPE_HEIGHT&&fclose(stream)==0);}
    assert(!differences);
    /* Initial one-item list was already checked in the ordinary-use probe.
       Count distinct pixel arrays, so positive seek failure adds no duplicate. */
    if(key==4)return;
    for(unsigned i=0;i<screens;++i)if(!memcmp(seen[i],pixels,NPE_WIDTH*NPE_HEIGHT))return;
    assert(screens<20);memcpy(seen[screens++],pixels,NPE_WIDTH*NPE_HEIGHT);
}
static int probe_list(const NpeFile *file,const NpeFont *font,const unsigned *ids,const unsigned *quantities,unsigned count,unsigned selection,uint8_t *pixels){
    int result=(updating?npe_update_item_list_screen:npe_render_item_list_screen)(file,font,ids,quantities,count,selection,pixels);assert(!result);
    selected=(int)selection;
    if(count==1){assert(ids[0]==69&&quantities[0]==2);compare(4,pixels);}
    else {
        assert(count==3&&ids[0]==69&&ids[1]==212&&ids[2]==214&&quantities[0]==1&&quantities[2]==44);
        if(variant==2&&quantities[1]==27){assert(!selection);compare(9,pixels);}
        else{assert(quantities[1]==28);compare(selection==0?5:selection==1?6:8,pixels);}
    }
    return result;
}
static int probe_update(const NpeFile *file,const NpeFont *font,const unsigned *ids,const unsigned *quantities,unsigned count,unsigned selection,uint8_t *pixels){
    updating=1;int result=probe_list(file,font,ids,quantities,count,selection,pixels);updating=0;return result;
}
static int probe_runtime_list(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                              const unsigned *ids,const unsigned *quantities,unsigned count,
                              unsigned selection,unsigned previous,int full,uint8_t *pixels){
    int result=npe_render_map_item_list(file,font,world,ids,quantities,count,selection,previous,full,pixels);assert(!result);
    selected=(int)selection;
    if(count==1){assert(ids[0]==69&&quantities[0]==2);compare(4,pixels);}
    else {
        assert(count==3&&ids[0]==69&&ids[1]==212&&ids[2]==214&&quantities[0]==1&&quantities[2]==44);
        if(variant==2&&quantities[1]==27){assert(!selection);compare(9,pixels);}
        else{assert(quantities[1]==28);compare(selection==0?5:selection==1?6:8,pixels);}
    }
    return result;
}
static int probe_target(const NpeFile *file,const NpeFont *font,const NpeWorld *world,unsigned member,unsigned id,unsigned quantity,int full,uint8_t *pixels){
    int result=npe_render_map_item_target_view(file,font,world,member,id,quantity,full,pixels);
    assert(!result&&variant==2&&id==212&&member==0);compare(quantity==28?7:8,pixels);return result;
}
static int probe_rejection(const NpeFont *font,unsigned reason,uint8_t *pixels){
    int result=npe_render_map_item_rejection(font,reason,pixels);assert(!result&&!reason&&variant!=2);
    compare(selected==1?7:9,pixels);SDL_AtomicSet(&refusal,1);return result;
}
static void await_flag(SDL_atomic_t *flag){for(unsigned i=0;!SDL_AtomicGet(flag);++i){assert(i<8000);SDL_Delay(1);}SDL_AtomicSet(flag,0);}
static void send(SDL_Keycode key){await_flag(&ready);SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=key;assert(SDL_PushEvent(&event)==1);}
static int keys(void *unused){
    (void)unused;send(SDLK_RETURN);
    if(variant==3){send(SDLK_F4);return 0;}
    send(SDLK_RIGHT);send(SDLK_RETURN);
    if(variant==2){send(SDLK_RETURN);send(SDLK_F4);send(SDLK_F4);}
    else{
        await_flag(&refusal);SDL_Delay(30);SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_F4;assert(SDL_PushEvent(&event)==1);
        send(SDLK_RIGHT);send(SDLK_RETURN);await_flag(&refusal);send(SDLK_F4);
    }
    return 0;
}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void put32(uint8_t *p,uint32_t value){p[0]=(uint8_t)(value>>24);p[1]=(uint8_t)(value>>16);p[2]=(uint8_t)(value>>8);p[3]=(uint8_t)value;}
static void put16(uint8_t *p,int value){p[0]=(uint8_t)((uint16_t)value>>8);p[1]=(uint8_t)value;}
static NpeWorld fixture(NpeFile *file){
    NpeItem item;assert(npe_item(file,68,&item)==0);uint8_t *raw=(uint8_t *)item.raw;raw[36]=1;
    const unsigned offsets[]={29,31,27,33};const int effects[]={7,-5,11,-13};for(unsigned i=0;i<4;++i)put16(raw+offsets[i],effects[i]);
    NpeWorld world;assert(npe_world_new_game(file,&world)==0);world.game_mode=4;memset(world.condition_words,0,sizeof(world.condition_words));world.condition_words[69]=2;
    for(unsigned i=0;i<3;++i){world.party_ids[i]=1;world.party_characters[i]=world.party_characters[0];memcpy(world.party_names[i],world.party_names[0],12);memcpy(world.battle_names[i],world.party_names[0],12);world.battle_characters[i]=world.party_characters[0];world.party_active[i]=1;world.level[i]=world.level[0];world.experience[i]=world.experience[0];world.experience_to_next[i]=world.experience_to_next[0];
        world.hp[i]=i==0?50:i==1?0:190;world.max_hp[i]=200;world.qi[i]=100;world.max_qi[i]=120;world.attack[i]=15;world.defense[i]=13;
        for(unsigned j=0;j<3;++j)world.other_stats[i][j]=8;for(unsigned j=0;j<4;++j)world.status_guard[i][j]=(uint16_t)(10*i+j);}
    return world;
}
int main(int argc,char **argv){
    assert(argc==2);SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
    NpeFile file;uint8_t *bytes;size_t size;char error[128];assert(read_file(argv[1],&bytes,&size)==0&&npe_parse(&file,bytes,size,error,sizeof(error))==0);
    uint64_t fingerprint=npe_game_fingerprint(&file);uint8_t *original=malloc(size);assert(original);memcpy(original,bytes,size);
    NpeFont font={0};assert(npe_font_load(&font,"npe-sdl/build/assets/gbk12-native.bin")==0&&npe_font_load16(&font,"npe-sdl/build/assets/gbk16-native.bin")==0);
    SDL_Window *window=SDL_CreateWindow("alias",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(variant=0;variant<4;++variant){
        memcpy(bytes,original,size);NpeWorld world=fixture(&file);uint8_t *table=bytes+file.sections[2].offset;
        if(variant==1)put32(table+211*4,(uint32_t)size+100);
        if(variant==2)put32(table+211*4,get32(table+45*4));
        if(variant==3)put32(table+211*4,0u-file.sections[2].offset);
        NpeItem item;assert(npe_item(&file,211,&item)==-1&&npe_native_item(&file,212,&item)==0);
        if(variant<2)assert(item.raw==table+212*4&&item.effect_type==211&&item.group==178&&item.target_side==187);
        else if(variant==2)assert(item.effect_type==1&&item.hp_restore==40&&item.group==0&&item.target_side==1);
        else assert(item.raw==bytes&&item.effect_type==204&&item.group==6&&item.target_side==131);
        assert(npe_native_item(&file,214,&item)==0&&item.raw==table+214*4&&item.effect_type==203&&item.group==249&&item.target_side==43);
        NpeIndexedBitmap icon;assert(npe_native_item_bitmap(&file,214,&icon)==0);fprintf(stderr,"Alias214 image: %ux%u at %u,%u\n",icon.width,icon.height,icon.x,icon.y);
        SDL_AtomicSet(&ready,0);SDL_AtomicSet(&refusal,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        SDL_Thread *thread=SDL_CreateThread(keys,"alias-keys",NULL);assert(thread);assert(show_use_item(window,renderer,texture,&file,&font,&world,0)==0);SDL_WaitThread(thread,NULL);
        assert(world.game_mode==1&&world.condition_words[69]==1&&world.condition_words[212]==(variant==2?27:28)&&world.condition_words[213]==65516&&world.condition_words[214]==44);
        assert(world.hp[0]==(variant==2?90:50)&&world.hp[1]==0&&world.hp[2]==190);
        if(variant==3){NpeWorld before=world;NpeItem cached;assert(!npe_world_item(&file,&before,212,&cached));assert(npe_world_use_item(&file,&world,212,0)==0&&!memcmp(&world,&before,sizeof(world)));}
        for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)assert(world.status_guard[i][j]==10*i+j);
        fprintf(stderr,"SDL alias variant %d: %s passed\n",variant,variant==3?"wrapped offset fields and ASCII list scan; binary description not covered":"signed scan, selection, quantity, rejection/use and map return");
    }
    memcpy(bytes,original,size);assert(npe_game_fingerprint(&file)==fingerprint);free(original);
    fprintf(stderr,"Alias screens: %u distinct new original frames; three complete SDL use/rejection flows and one wrapped list scan passed\n",screens);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_font_free(&font);npe_free(&file);SDL_Quit();return 0;
}
