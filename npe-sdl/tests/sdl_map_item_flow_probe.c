#include <assert.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"
#include "map_use_native.h"
static SDL_atomic_t ready;
static unsigned calls, dialogs, frozen_pages;
static int checking_freeze;
static SDL_atomic_t prompt_visible;
static const char *prompt_reference;
static Uint32 prompt_started;
static int probe_rejection(const NpeFont *,unsigned,uint8_t *);
static int probe_wait(SDL_Event *event){SDL_AtomicSet(&ready,1);return SDL_WaitEvent(event);}
static int probe_use(const NpeFile *,NpeWorld *,unsigned,unsigned);
static int probe_page(const NpeFile *,const NpeFont *,const NpeWorld *,unsigned,unsigned,unsigned,int,uint8_t *);
static int probe_dialog(Uint32 flags,const char *title,const char *message,SDL_Window *window){
    (void)flags;(void)title;(void)message;(void)window;++dialogs;return 0;
}
#define SDL_WaitEvent probe_wait
#define SDL_ShowSimpleMessageBox probe_dialog
#define npe_world_use_item probe_use
#define npe_render_map_item_target_view probe_page
#define npe_render_map_item_rejection probe_rejection
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef SDL_ShowSimpleMessageBox
#undef npe_world_use_item
#undef npe_render_map_item_target_view
#undef npe_render_map_item_rejection

static unsigned screens;
static void compare(const char *path,const uint8_t *pixels){
    static char seen[80][160];
    FILE *stream=fopen(path,"rb");assert(stream);unsigned differences=0;
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){int c=fgetc(stream);assert(c>=0);differences+=pixels[i]!=(uint8_t)c;}
    assert(fgetc(stream)==EOF&&fclose(stream)==0);
    fprintf(stderr,"Original map-use screen %s: %u differences\n",path,differences);
    if(differences){stream=fopen("tmp/map-use-actual.idx","wb");assert(stream);assert(fwrite(pixels,1,NPE_WIDTH*NPE_HEIGHT,stream)==NPE_WIDTH*NPE_HEIGHT&&fclose(stream)==0);}
    assert(!differences);
    for(unsigned i=0;i<screens;++i)if(!strcmp(path,seen[i]))return;
    assert(screens<80);snprintf(seen[screens++],160,"%s",path);
}
static int probe_rejection(const NpeFont *font,unsigned rejection,uint8_t *pixels){
    int result=npe_render_map_item_rejection(font,rejection,pixels);
    if(prompt_reference){assert(!result);compare(prompt_reference,pixels);prompt_started=SDL_GetTicks();SDL_AtomicSet(&prompt_visible,1);}
    return result;
}
static int probe_use(const NpeFile *file,NpeWorld *world,unsigned id,unsigned member){
    ++calls;return npe_world_use_item(file,world,id,member);
}
static int probe_page(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                      unsigned member,unsigned id,unsigned quantity,int full,uint8_t *pixels){
    int result=npe_render_map_item_target_view(file,font,world,member,id,quantity,full,pixels);
    if(checking_freeze&&!quantity){assert(member==2);++frozen_pages;}
    return result;
}
struct Keys {SDL_Keycode key[24];unsigned count;};
static int send_keys(void *data){
    const struct Keys *keys=data;
    for(unsigned i=0;i<keys->count;++i){
        for(unsigned elapsed=0;!SDL_AtomicGet(&ready);++elapsed){assert(elapsed<8000);SDL_Delay(1);}
        SDL_AtomicSet(&ready,0);SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=keys->key[i];
        assert(SDL_PushEvent(&event)==1);
    }
    return 0;
}
static void push_key(SDL_Keycode key){SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=key;assert(SDL_PushEvent(&event)==1);}
static void await_flag(SDL_atomic_t *flag){
    for(unsigned elapsed=0;!SDL_AtomicGet(flag);++elapsed){assert(elapsed<8000);SDL_Delay(1);}
}
static int send_rejection_keys(void *unused){
    (void)unused;await_flag(&ready);SDL_AtomicSet(&ready,0);push_key(SDLK_RETURN);
    await_flag(&prompt_visible);SDL_Delay(25);
    /* The dismissing key ends the prompt and is consumed by it. */
    push_key(SDLK_RETURN);
    await_flag(&ready);assert(SDL_GetTicks()-prompt_started<2000);
    push_key(SDLK_F4);return 0;
}
static void put(uint8_t *raw,unsigned offset,int value){raw[offset]=(uint8_t)((uint16_t)value>>8);raw[offset+1]=(uint8_t)value;}
static NpeWorld fixture(NpeFile *file,const MapUseNative *sample){
    NpeItem item;assert(npe_item(file,sample->id-1,&item)==0);uint8_t *raw=(uint8_t *)item.raw;
    const char *variant=sample->variant;
    if(!strcmp(variant,"wrap")||!strcmp(variant,"negative")){
        put(raw,13,!strcmp(variant,"wrap")?100:-100);put(raw,15,!strcmp(variant,"wrap")?20:-200);
        for(unsigned o=17;o<=25;o+=2)put(raw,o,77);
    }
    if(strstr(variant,"status")){
        const unsigned offsets[]={29,31,27,33};const int effects[]={7,-5,11,-13};
        for(unsigned s=0;s<4;++s)put(raw,offsets[s],effects[s]);
    }
    if(!strncmp(variant,"group",5))raw[36]=1;
    if(!strcmp(variant,"group")||!strcmp(variant,"group-attributes"))put(raw,15,60);
    if(!strcmp(variant,"group-attributes"))for(unsigned o=17;o<=25;o+=2)put(raw,o,77);
    if(!strcmp(variant,"enemy"))raw[37]=0;
    if(!strcmp(variant,"type16"))raw[12]=16;
    NpeWorld world;assert(npe_world_new_game(file,&world)==0);world.game_mode=4;
    memset(world.condition_words,0,sizeof(world.condition_words));world.condition_words[sample->id]=!strcmp(variant,"quantity")?100:2;
    for(unsigned m=0;m<3;++m){
        world.party_active[m]=m<sample->count;world.party_ids[m]=1;world.party_characters[m]=world.party_characters[0];memcpy(world.party_names[m],world.party_names[0],12);memcpy(world.battle_names[m],world.party_names[0],12);world.battle_characters[m]=world.party_characters[0];
        if(!strcmp(variant,"group-status-inactive")&&m==1)world.party_active[m]=0;
        world.level[m]=world.level[0];world.experience[m]=world.experience[0];world.experience_to_next[m]=world.experience_to_next[0];
        const uint16_t *p=sample->initial[m];
        world.hp[m]=p[0];world.qi[m]=p[1];world.attack[m]=p[2];world.defense[m]=p[3];
        memcpy(world.other_stats[m],p+4,sizeof(world.other_stats[m]));memcpy(world.status_guard[m],p+7,sizeof(world.status_guard[m]));
        world.max_hp[m]=!strcmp(variant,"wrap")?32767:200;world.max_qi[m]=!strcmp(variant,"wrap")?32767:120;
    }
    return world;
}
static void check_state(const NpeWorld *world,const MapUseNative *sample,const MapUseSnapshot *expected){
    assert(world->condition_words[sample->id]==expected->inventory);
    for(unsigned m=0;m<sample->count;++m){
        uint16_t actual[]={world->hp[m],world->qi[m],world->attack[m],world->defense[m],
            world->other_stats[m][0],world->other_stats[m][1],world->other_stats[m][2],
            world->status_guard[m][0],world->status_guard[m][1],world->status_guard[m][2],world->status_guard[m][3]};
        assert(memcmp(actual,expected->party[m],sizeof(actual))==0);
    }
    assert(memcmp(world->condition_words+212,expected->aliases,sizeof(expected->aliases))==0);
}
static void page_compare(const NpeFile *file,const NpeFont *font,const NpeWorld *world,const MapUseNative *sample,unsigned member,unsigned key){
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];char path[160];
    assert(npe_render_map_item_target_screen(file,font,world,member,sample->id,world->condition_words[sample->id],pixels)==0);
    snprintf(path,sizeof(path),"tmp/%s-key%u.idx",sample->stem,key);compare(path,pixels);
}
static void run_flow(SDL_Window *window,SDL_Renderer *renderer,SDL_Texture *texture,NpeFile *file,const NpeFont *font,NpeWorld *world,struct Keys keys){
    calls=dialogs=0;SDL_AtomicSet(&ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    SDL_Thread *thread=SDL_CreateThread(send_keys,"map-item-keys",&keys);assert(thread);
    assert(show_use_item(window,renderer,texture,file,font,world,0)==0);SDL_WaitThread(thread,NULL);
    assert(world->game_mode==1&&!dialogs);
}
int main(int argc,char **argv){
    assert(argc==2);SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
    NpeFile file;uint8_t *bytes;size_t size;char error[128];assert(read_file(argv[1],&bytes,&size)==0);
    assert(npe_parse(&file,bytes,size,error,sizeof(error))==0);uint64_t fingerprint=npe_game_fingerprint(&file);
    NpeFont font={0};assert(npe_font_load(&font,"npe-sdl/build/assets/gbk12-native.bin")==0);assert(npe_font_load16(&font,"npe-sdl/build/assets/gbk16-native.bin")==0);
    SDL_Window *window=SDL_CreateWindow("map-items",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned index=0;index<sizeof(map_use_native)/sizeof(map_use_native[0]);++index){
        const MapUseNative *sample=&map_use_native[index];NpeItem item;assert(npe_item(&file,sample->id-1,&item)==0);
        uint8_t backup[40];memcpy(backup,item.raw,sizeof(backup));NpeWorld world=fixture(&file,sample);
        assert(npe_item(&file,sample->id-1,&item)==0);int target_page=!item.group&&item.effect_type<24&&item.target_side==1;
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];char path[160];unsigned ids[]={sample->id},quantities[]={world.condition_words[sample->id]};
        assert(npe_render_item_list_screen(&file,&font,ids,quantities,1,0,pixels)==0);
        snprintf(path,sizeof(path),"tmp/%s-key4.idx",sample->stem);compare(path,pixels);
        if(target_page)page_compare(&file,&font,&world,sample,0,5);
        for(unsigned phase=0;phase<sample->phase_count;++phase){
            const MapUseSnapshot *expected=&sample->phase[phase];
            assert(npe_world_use_item(&file,&world,sample->id,expected->member)==1);check_state(&world,sample,expected);
            if(target_page)page_compare(&file,&font,&world,sample,expected->member,expected->key);
            /* Save the map-mode copy, including low words and aliased inventory. */
            if(strcmp(sample->variant,"group-status-inactive")) {
                /* Native visual fixtures clone the hero record. The C
                   codec requires distinct IDs and a compact active party. */
                NpeWorld saving=world,restored;saving.game_mode=1;
                for(unsigned m=0;m<3;++m)saving.party_ids[m]=(uint8_t)(m+1);
                const char *save_path="tmp/map-use-fixture.npes";
                assert(npe_world_save(&file,&saving,save_path)==0&&npe_world_load(&file,&restored,save_path)==0);
                check_state(&restored,sample,expected);assert(remove(save_path)==0);
            }
        }
        if(!sample->phase_count){
            NpeWorld unchanged=world;NpeItem cached;
            assert(!npe_world_item(&file,&unchanged,sample->id,&cached));
            int result=npe_world_use_item(&file,&world,sample->id,0);
            assert(result==(!strcmp(sample->variant,"enemy")?-2:0));assert(memcmp(&world,&unchanged,sizeof(world))==0);
            assert(npe_render_map_item_rejection(&font,!strcmp(sample->variant,"enemy"),pixels)==0);
            snprintf(path,sizeof(path),"tmp/%s-key5.idx",sample->stem);compare(path,pixels);
        }
        fprintf(stderr,"Original map-use %s: %u phases, full party passed%s\n",sample->stem,sample->phase_count,
            !strcmp(sample->variant,"group-status-inactive")?" (noncompact fixture; save not applicable)":sample->phase_count?" with map saves":" (rejected unchanged)");
        memcpy((uint8_t *)item.raw,backup,sizeof(backup));assert(npe_game_fingerprint(&file)==fingerprint);
    }
    for(unsigned index=0;index<5;++index){
        const MapUseNative *sample=&map_use_native[index];NpeWorld world=fixture(&file,sample);
        struct Keys keys;
        if(index==2)keys=(struct Keys){{SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_F4},6};
        else if(index<2)keys=(struct Keys){{SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_UP,SDLK_RETURN,SDLK_F4,SDLK_F4},9};
        else keys=(struct Keys){{SDLK_RETURN,SDLK_UP,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_RETURN,SDLK_F4,SDLK_F4},8};
        checking_freeze=index<2;frozen_pages=0;run_flow(window,renderer,texture,&file,&font,&world,keys);checking_freeze=0;
        /* The final use redraws quantity zero once. Frozen direction and
           confirm inputs retain that page without calling its renderer. */
        assert(calls==2);check_state(&world,sample,&sample->phase[1]);if(index<2)assert(frozen_pages==1);
        fprintf(stderr,"SDL original map-use %s: selection, repeat, depletion and map return passed\n",sample->stem);
    }
    const unsigned more[]={5,6,7,8,9,12,14};
    for(unsigned i=0;i<sizeof(more)/sizeof(more[0]);++i){
        const MapUseNative *sample=&map_use_native[more[i]];NpeItem item;assert(npe_item(&file,sample->id-1,&item)==0);
        uint8_t backup[40];memcpy(backup,item.raw,sizeof(backup));NpeWorld world=fixture(&file,sample);
        assert(npe_item(&file,sample->id-1,&item)==0);
        struct Keys keys=item.group?(struct Keys){{SDLK_RETURN,SDLK_RETURN,SDLK_F4},3}:
            (struct Keys){{SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_UP,SDLK_RETURN,SDLK_F4,SDLK_F4},7};
        /* Quantity100 retains98; avoid a third use in that fixture. */
        if(!strcmp(sample->variant,"quantity"))keys=(struct Keys){{SDLK_RETURN,SDLK_RETURN,SDLK_RETURN,SDLK_F4,SDLK_F4},5};
        run_flow(window,renderer,texture,&file,&font,&world,keys);assert(calls==2);check_state(&world,sample,&sample->phase[1]);
        memcpy((uint8_t *)item.raw,backup,sizeof(backup));assert(npe_game_fingerprint(&file)==fingerprint);
        fprintf(stderr,"SDL original map-use %s: repeated effect and return passed\n",sample->stem);
    }
    const unsigned rejected[]={13,15,16};
    for(unsigned i=0;i<3;++i){
        const MapUseNative *sample=&map_use_native[rejected[i]];NpeItem item;assert(npe_item(&file,sample->id-1,&item)==0);
        uint8_t backup[40];memcpy(backup,item.raw,sizeof(backup));NpeWorld world=fixture(&file,sample);
        char path[160];snprintf(path,sizeof(path),"tmp/%s-key5.idx",sample->stem);prompt_reference=path;
        SDL_AtomicSet(&ready,0);SDL_AtomicSet(&prompt_visible,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);calls=dialogs=0;
        SDL_Thread *thread=SDL_CreateThread(send_rejection_keys,"map-item-rejection",NULL);assert(thread);
        show_use_item(window,renderer,texture,&file,&font,&world,0);SDL_WaitThread(thread,NULL);prompt_reference=NULL;
        assert(world.game_mode==1&&world.condition_words[sample->id]==2&&!calls&&!dialogs);
        memcpy((uint8_t *)item.raw,backup,sizeof(backup));assert(npe_game_fingerprint(&file)==fingerprint);
        fprintf(stderr,"SDL original map-use %s: refusal and consumed dismissing input passed\n",sample->stem);
    }
    fprintf(stderr,"Map-use native screen comparisons: %u\n",screens);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_font_free(&font);npe_free(&file);SDL_Quit();return 0;
}
