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
typedef struct {unsigned id,x,y,direction;} MoneyNpc;
typedef struct {unsigned mode,event,context,map,progress,money,init,sx,sy,vx,vy,direction,enabled,slot;MoneyNpc npc[20];} MoneyState;
typedef struct {const char *name;unsigned amount,adding,commits;MoneyState before,after;} MoneyCase;
#include "money_service_native.h"
typedef struct {unsigned amount,next,adding,mode;} MoneyCache;
typedef struct {const char *name;unsigned map,restart;MoneyCache boot[5],before[5],after[5];} MoneyCacheCase;
#include "money_cache_native.h"
static unsigned case_index,submits,total,active,quit_requested;
static Uint32 fake_clock;
static NpeWorld *live;
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *e){
    if(!active)return 0;
    memset(e,0,sizeof(*e));e->type=quit_requested?SDL_QUIT:SDL_KEYDOWN;e->key.keysym.sym=SDLK_F4;return 1;
}
static void probe_present(SDL_Renderer *);
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_RenderPresent probe_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_RenderPresent
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[160];
    if(before)snprintf(path,sizeof(path),"tmp/native-money-service-%s-before.idx",money_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-money-service-%s-%u.idx",money_cases[case_index].name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const MoneyCase *c=&money_cases[case_index];assert(submits<c->commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"money %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);
    if(!quit_requested)assert(!d&&!sd);
    if(c->commits==3&&submits<2)assert(live->game_mode==11&&live->story_event_id==c->before.event&&live->money==c->after.money);
    ++submits;++total;
}
static void configure(NpeWorld *w,const NpeWorld *baseline,const MoneyCase *c){
    const MoneyState *s=&c->before;*w=*baseline;
    w->game_mode=11;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;
    assert(!npe_world_resources_open((NpeFile *)w->battle_file,w));
    const uint8_t cash[]={(uint8_t)(s->money>>24),(uint8_t)(s->money>>16),(uint8_t)(s->money>>8),(uint8_t)s->money};
    assert(!npe_rpg_runtime_write(w->battle_file,0x52799e,cash,4));w->story_progress=(uint16_t)s->progress;w->map_init_pending=(uint8_t)s->init;
    w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
    w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=s->direction;w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
    for(unsigned i=0;i<5;++i){
        uint8_t record[]={(uint8_t)(c->amount>>8),(uint8_t)c->amount,3,21,(uint8_t)c->adding,1,0x81,0xcd};
        assert(!npe_rpg_runtime_write(w->battle_file,0x5276da+i*8,record,8));
    }
    npe_world_seed(w,11);
}
static void seed_cache(NpeWorld *w,const MoneyCache c[5]){
    for(unsigned i=0;i<5;++i)w->money_events[i]=(NpeMapMoneyEvent){.id=(uint16_t)(i+1),.amount=(uint16_t)c[i].amount,
        .next_id=(uint16_t)c[i].next,.add_money=(uint8_t)c[i].adding,.next_mode=(uint8_t)c[i].mode};
}
static void check_cache(const NpeWorld *w,const MoneyCache c[5]){
    for(unsigned i=0;i<5;++i){NpeMapMoneyEvent e;assert(npe_world_money_event(w,i+1,&e)==1&&!e.raw);
        assert(e.id==i+1&&e.amount==c[i].amount&&e.next_id==c[i].next&&e.add_money==c[i].adding&&e.next_mode==c[i].mode);}
}
static void check_cache_lifetime(const NpeFile *file,const NpeWorld *baseline){
    for(unsigned i=0;i<8;++i){const MoneyCacheCase *c=&money_cache_cases[i];NpeWorld w=*baseline,saved,cold;
        check_cache(baseline,c->boot);seed_cache(&w,c->before);
        if(c->restart){assert(!npe_world_restart_game(file,&w));check_cache(&w,c->after);continue;}
        assert(!npe_world_init(file,&saved,c->map,1,1));const char *path="tmp/money-cache.npes";
        assert(!npe_world_save(file,&saved,path));assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);
        assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);
        assert(!npe_world_load(file,&cold,path));NpeMap map;assert(!npe_map(file,c->map-1,&map));
        for(unsigned j=0;j<5;++j){NpeMapMoneyEvent e;int found=npe_map_money_event(&map,j+1,&e);assert(found>=0);
            assert(cold.money_events[j].amount==(found?e.amount:0));}
        NpeWorld before=w;FILE *out=fopen(path,"wb");assert(out&&fwrite("NPESAV17",1,8,out)==8&&!fclose(out));
        assert(npe_world_restore(file,&w,path)<0&&!memcmp(&before,&w,sizeof(w))&&!remove(path));
        uint8_t tail[710]={0};tail[652+12*3+2]=6;NpeMap bad=map;bad.tail=tail;bad.tail_size=sizeof(tail);
        assert(npe_world_load_money_events(&w,&bad)<0&&!memcmp(&before,&w,sizeof(w)));
    }
    NpeWorld a=*baseline,b=a;seed_cache(&b,money_cache_cases[0].before);
    assert(!npe_world_save(file,&a,"tmp/money-wire-a.npes")&&!npe_world_save(file,&b,"tmp/money-wire-b.npes"));
    FILE *fa=fopen("tmp/money-wire-a.npes","rb"),*fb=fopen("tmp/money-wire-b.npes","rb");assert(fa&&fb);
    int ca,cb;do{ca=fgetc(fa);cb=fgetc(fb);assert(ca==cb);}while(ca!=EOF);
    assert(!fclose(fa)&&!fclose(fb)&&!remove("tmp/money-wire-a.npes")&&!remove("tmp/money-wire-b.npes"));
}
int main(int argc,char **argv){NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld baseline,w;
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("money service",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!npe_world_new_game(&file,&baseline));live=&w;check_cache_lifetime(&file,&baseline);
    for(case_index=0;case_index<18;++case_index){const MoneyCase *c=&money_cases[case_index];configure(&w,&baseline,c);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);active=0;draw_pixels(renderer,texture,pixels);
        save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&w,renderer,pixels,1);
        submits=0;active=1;fake_clock=case_index==1?UINT32_MAX-1000:0;Uint32 start=fake_clock;
        assert(advance_world_events(window,renderer,texture,&file,&font,&w,11,0)>=0);
        assert(fake_clock==start&&submits==c->commits);
        assert(w.money==c->after.money&&w.game_mode==c->after.mode&&w.story_event_id==c->after.event&&w.map_init_pending==c->after.init&&w.random_state==11);
        uint8_t cash_after[4];assert(!npe_rpg_runtime_read(&file,0x52799e,cash_after,4));
        assert((((uint32_t)cash_after[0]<<24)|((uint32_t)cash_after[1]<<16)|((uint32_t)cash_after[2]<<8)|cash_after[3])==c->after.money);
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
        active=0;const char *path="tmp/money-negative.npes";assert(!npe_world_save(&file,&w,path));NpeWorld loaded;
        assert(!npe_world_load(&file,&loaded,path)&&loaded.money==w.money&&!remove(path));
    }
    case_index=0;configure(&w,&baseline,&money_cases[0]);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);
    active=0;draw_pixels(renderer,texture,pixels);submits=0;active=quit_requested=1;
    assert(show_money_event(renderer,texture,&font,&w)==-2&&submits==1&&w.game_mode==11&&w.story_event_id==1&&w.money==150&&w.map_init_pending==1);
    active=0;npe_rpg_runtime_close(&file);
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    printf("Money service:18 original paths,%u C/SDL commits,8 original cache loads,signed amount/clip/low-byte,fixed5,warm lifetime,save bytes and quit passed\n",total-1);return 0;
}
