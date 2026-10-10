#include <assert.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
static SDL_atomic_t qi_ready;
static unsigned qi_frame_enabled;
static void qi_sdl_frame(void);
static int qi_wait(SDL_Event *event) {
    if(qi_frame_enabled)qi_sdl_frame();
    SDL_AtomicSet(&qi_ready,1);return SDL_WaitEvent(event);
}
#define SDL_WaitEvent qi_wait
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#include "negative_qi_native.h"

static const QiCase *qi_case;
static unsigned qi_draws, qi_keys_count;
static SDL_Keycode qi_keys[4];
static SDL_Renderer *qi_renderer;
static uint32_t qi_palette(unsigned value) {
    return UINT32_C(0xff000000)|((183-value*142/15)<<16)|
           ((200-value*146/15)<<8)|(161-value*119/15);
}
static void qi_reference(const char *phase,uint8_t *pixels) {
    char path[160];snprintf(path,sizeof(path),"tmp/native-negative-qi-%s-%s.idx",qi_case->name,phase);
    FILE *input=fopen(path,"rb");assert(input);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,input)==NPE_WIDTH*NPE_HEIGHT&&fgetc(input)==EOF&&!fclose(input));
}
static void qi_compare(const char *phase,const uint8_t *pixels) {
    uint8_t reference[NPE_WIDTH*NPE_HEIGHT];qi_reference(phase,reference);unsigned differences=0;
    for(unsigned i=0;i<sizeof(reference);++i)differences+=pixels[i]!=reference[i];
    fprintf(stderr,"Original signed QI %s %s: %u differences\n",qi_case->name,phase,differences);
    if(differences){FILE *out=fopen("tmp/negative-qi-c.idx","wb");assert(out);fwrite(pixels,1,sizeof(reference),out);fclose(out);}
    assert(!differences);
}
static void qi_sdl_frame(void) {
    const char *phases[]={"list","confirm","repeat","cancel"};
    assert(qi_draws<4);uint8_t reference[NPE_WIDTH*NPE_HEIGHT];qi_reference(phases[qi_draws],reference);
    uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(qi_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i)differences+=rgba[i]!=qi_palette(reference[i]);
    fprintf(stderr,"SDL signed QI %s %s: %u differences\n",qi_case->name,phases[qi_draws],differences);
    assert(!differences);++qi_draws;
}
static int qi_send_keys(void *unused) {
    (void)unused;unsigned waited=0;
    while(!SDL_AtomicGet(&qi_ready)&&waited<5000){SDL_Delay(10);waited+=10;}
    if(!SDL_AtomicGet(&qi_ready))return -1;
    for(unsigned i=0;i<qi_keys_count;++i){
        SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=qi_keys[i];
        SDL_AtomicSet(&qi_ready,0);if(SDL_PushEvent(&event)!=1)return -1;
        if(i+1<qi_keys_count){waited=0;while(!SDL_AtomicGet(&qi_ready)&&waited<5000){SDL_Delay(10);waited+=10;}if(!SDL_AtomicGet(&qi_ready))return -1;}
    }
    return 0;
}
static NpeWorld qi_fixture(const NpeFile *file) {
    NpeWorld w;assert(!npe_world_init(file,&w,9,29,13));w.game_mode=3;w.story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,&w)==1);
    for(unsigned i=0;i<qi_case->party;++i){
        w.party_active[i]=1;w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];w.level[i]=99;w.max_hp[i]=1000;w.max_qi[i]=999;
        w.hp[i]=qi_case->before[i].hp;w.qi[i]=qi_case->before[i].qi;
        w.attack[i]=15;w.defense[i]=w.battle_party_defenses[i]=13;w.other_stats[i][1]=(uint16_t)(999-i);
        memset(w.skill_slots[i],0,sizeof(w.skill_slots[i]));w.skill_slots[i][0]=42;
        memcpy(w.battle_party_status[i],qi_case->before[i].status,8);
    }
    for(unsigned i=0;i<3;++i){const QiRecord *r=&qi_case->before[qi_case->party+i];
        w.battle_enemy_hps[i]=r->hp;w.battle_enemy_qis[i]=r->qi;w.battle_enemy_speeds[i]=0;
        memcpy(w.battle_enemy_status[i],r->status,8);
    }
    w.battle_actor=0;return w;
}
static void qi_records(const NpeWorld *w,const QiRecord *records) {
    assert(w->game_mode==3);
    for(unsigned i=0;i<qi_case->party+3;++i){QiRecord r;
        unsigned n=i<qi_case->party?i:i-qi_case->party;
        r.hp=i<qi_case->party?w->hp[n]:w->battle_enemy_hps[n];r.qi=i<qi_case->party?w->qi[n]:w->battle_enemy_qis[n];
        memcpy(r.status,i<qi_case->party?w->battle_party_status[n]:w->battle_enemy_status[n],8);
        if(memcmp(&r,&records[i],sizeof(r)))fprintf(stderr,"QI record %s %u hp %u/%u qi %u/%u\n",qi_case->name,i,r.hp,records[i].hp,r.qi,records[i].qi);
        assert(!memcmp(&r,&records[i],sizeof(r)));
    }
}
int main(int argc,char **argv) {
    assert(argc==2);NpeFile file;NpeFont font={0};char error[128];uint8_t *bytes;size_t size;
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    SDL_Window *window=SDL_CreateWindow("original signed QI",0,0,240,120,SDL_WINDOW_HIDDEN);assert(window);
    qi_renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(qi_renderer);
    SDL_Texture *texture=SDL_CreateTexture(qi_renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned scenario=0;scenario<sizeof(qi_cases)/sizeof(*qi_cases);++scenario){
        qi_case=&qi_cases[scenario];NpeWorld w=qi_fixture(&file),before=w;qi_records(&w,qi_case->before);
        const uint8_t *record;size_t record_size;assert(!npe_record(&file,3,41,&record,&record_size));
        uint8_t *raw=(uint8_t *)record,saved[2]={raw[39],raw[40]};raw[39]=(uint8_t)(qi_case->cost>>8);raw[40]=(uint8_t)qi_case->cost;
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];unsigned ids[18]={42};
        assert(!npe_render_native_magic_list_screen(&file,&font,&w,0,ids,1,0,0,pixels));qi_compare("list",pixels);
        if(qi_case->rejected){
            assert(npe_world_battle_cast_magic(&file,&w,42,0)==-3&&!memcmp(&w,&before,sizeof(w)));
            assert(!npe_render_battle_insufficient_qi(&font,pixels));qi_compare("confirm",pixels);
        }else {
            assert(npe_world_battle_cast_magic(&file,&w,42,0)==NPE_BATTLE_CONTINUE);
            if(qi_case->party==2){assert(w.battle_actor==1);w.qi[0]=0;
                assert(npe_world_battle_cast_magic(&file,&w,42,1)==NPE_BATTLE_CONTINUE);}
            qi_records(&w,qi_case->after);
        }
        // Check native signed HUD characters using an untouched native background.
        const char *hud_phase=qi_case->rejected?"hud":"round";
        uint16_t hud_qi=qi_case->rejected?before.qi[0]:qi_case->after[0].qi;
        qi_reference(hud_phase,pixels);char counter[48];
        snprintf(counter,sizeof(counter),":%d/%d",(int16_t)hud_qi,(int16_t)before.max_qi[0]);
        unsigned right=82+(unsigned)strlen(counter)*6;
        for(unsigned y=104;y<116;++y)memset(pixels+y*NPE_WIDTH+82,0,right-82);
        npe_render_battle_hud_values(before.hp[0],before.max_hp[0],hud_qi,before.max_qi[0],pixels);
        uint8_t reference[NPE_WIDTH*NPE_HEIGHT];qi_reference(hud_phase,reference);
        unsigned differences=0;for(unsigned y=104;y<116;++y)for(unsigned x=82;x<right;++x)differences+=pixels[y*NPE_WIDTH+x]!=reference[y*NPE_WIDTH+x];
        fprintf(stderr,"Original signed QI %s HUD: %u differences\n",qi_case->name,differences);assert(!differences);
        w=before;qi_draws=0;qi_frame_enabled=1;qi_keys_count=qi_case->rejected?4:1;
        qi_keys[0]=qi_keys[1]=SDLK_RETURN;qi_keys[2]=SDLK_ESCAPE;qi_keys[3]=SDLK_F4;
        SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);SDL_AtomicSet(&qi_ready,0);
        SDL_Thread *thread=SDL_CreateThread(qi_send_keys,"signed QI keys",NULL);assert(thread);
        show_battle_skill(window,qi_renderer,texture,&file,&font,&w);int result=-1;SDL_WaitThread(thread,&result);assert(!result);qi_frame_enabled=0;
        if(qi_case->rejected){assert(qi_draws==4&&!memcmp(&w,&before,sizeof(w)));qi_records(&w,qi_case->after);}
        else {assert(qi_draws==1);
            if(qi_case->party==2){assert(w.battle_actor==1);w.qi[0]=0;qi_draws=0;
                // Second spell uses the real SDL list/confirmation; its frame differs from actor0.
                SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;assert(SDL_PushEvent(&event)==1);
                show_battle_skill(window,qi_renderer,texture,&file,&font,&w);}
            qi_records(&w,qi_case->after);
        }
        raw[39]=saved[0];raw[40]=saved[1];
    }
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    puts("Original signed QI/fee gates, unchanged rejected commands, queued charge, C pixels and SDL input passed");
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(qi_renderer);SDL_DestroyWindow(window);SDL_Quit();npe_font_free(&font);npe_free(&file);return 0;
}
