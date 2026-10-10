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
typedef struct {unsigned id,x,y,direction;} ItemNpc;
typedef struct {unsigned mode,event,context,map,progress,quantity,init,sx,sy,vx,vy,direction,enabled,slot;ItemNpc npc[20];} ItemState;
typedef struct {const char *name;unsigned item,quantity,adding,next,real,commits;ItemState before,after;uint8_t native_name[12];} ItemCase;
#ifdef NPE_ITEM_RESIDENT_PUBLIC
typedef struct {uint8_t before[80],after[80];} ItemResidentCase;
#include "item_resident_public_native.h"
#else
#include "item_service_native.h"
#endif
static uint32_t quantity_address(const ItemCase *c){
    int32_t index=c->item<32768?(int32_t)c->item:(int32_t)c->item-65536;
    return (uint32_t)(INT32_C(0x5267c8)+index*2);
}
static unsigned live_quantity(const NpeWorld *w,const ItemCase *c){
    uint8_t bytes[2];
    if(npe_world_resident_read(w,quantity_address(c),bytes,2))assert(!npe_rpg_runtime_read(w->battle_file,quantity_address(c),bytes,2));
    return ((unsigned)bytes[0]<<8)|bytes[1];
}
typedef struct {unsigned quantity,item,next,adding,mode;} ItemCache;
typedef struct {const char *name;unsigned map,restart;ItemCache boot[5],before[5],after[5];} ItemCacheCase;
#include "item_cache_native.h"
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
#ifdef NPE_ITEM_RESIDENT_PUBLIC
#define ITEM_FRAME_PREFIX "native-item-resident-public"
#else
#define ITEM_FRAME_PREFIX "native-item-service"
#endif
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[160];
    if(before)snprintf(path,sizeof(path),"tmp/" ITEM_FRAME_PREFIX "-%s-before.idx",item_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/" ITEM_FRAME_PREFIX "-%s-%u.idx",item_cases[case_index].name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const ItemCase *c=&item_cases[case_index];assert(submits<c->commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"item %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);
    if(!quit_requested)assert(!d&&!sd);
#ifdef NPE_ITEM_RESIDENT_PUBLIC
    char output[160];snprintf(output,sizeof(output),"tmp/c-item-resident-public-%s-%u.idx",c->name,submits);
    FILE *out=fopen(output,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));
#endif
    if(c->commits==3&&submits<2)assert(live->game_mode==12&&live->story_event_id==c->before.event&&live_quantity(live,c)==c->after.quantity);
    ++submits;++total;
}
static void configure(NpeWorld *w,const NpeWorld *baseline,const ItemCase *c){
    const ItemState *s=&c->before;*w=*baseline;
    w->game_mode=12;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;
    assert(!npe_world_resources_open((NpeFile *)w->battle_file,w));
    /* Write the same full-word guest address used by the original. */
    uint8_t initial[]={(uint8_t)(s->quantity>>8),(uint8_t)s->quantity};
    assert(!npe_rpg_runtime_write(w->battle_file,quantity_address(c),initial,2));w->story_progress=(uint16_t)s->progress;w->map_init_pending=(uint8_t)s->init;
    w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
    w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=s->direction;w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
#ifdef NPE_ITEM_RESIDENT_PUBLIC
    const ItemResidentCase *resident=&item_resident_cases[case_index];
    assert(!npe_rpg_runtime_write(w->battle_file,0x5276b0,resident->before,40));
    assert(!npe_rpg_runtime_write(w->battle_file,0x5276da,resident->before+40,40));
#else
    if(!c->real)for(unsigned i=0;i<5;++i){
        uint8_t record[]={(uint8_t)(c->quantity>>8),(uint8_t)c->quantity,(uint8_t)(c->item>>8),(uint8_t)c->item,
                         (uint8_t)(c->next>>8),(uint8_t)c->next,(uint8_t)c->adding,1};
        assert(!npe_rpg_runtime_write(w->battle_file,0x5276b0+i*8,record,8));
    }
#endif
    npe_world_seed(w,11);
}
static void seed_cache(NpeWorld *w,const ItemCache c[5]){
    for(unsigned i=0;i<5;++i)w->item_events[i]=(NpeMapItemEvent){.item_index=(uint16_t)c[i].item,.quantity=(uint16_t)c[i].quantity,
        .next_id=(uint16_t)c[i].next,.add_item=(uint8_t)c[i].adding,.next_mode=(uint8_t)c[i].mode};
}
static void check_cache(const NpeWorld *w,const ItemCache c[5]){
    for(unsigned i=0;i<5;++i){NpeMapItemEvent e;assert(npe_world_item_event(w,i+1,&e)==1&&!e.raw);
        assert(e.item_index==c[i].item&&e.quantity==c[i].quantity&&e.next_id==c[i].next&&e.add_item==c[i].adding&&e.next_mode==c[i].mode);}
}
static void check_cache_lifetime(const NpeFile *file,const NpeWorld *baseline){
    for(unsigned i=0;i<8;++i){const ItemCacheCase *c=&item_cache_cases[i];NpeWorld w=*baseline,saved,cold;
        check_cache(baseline,c->boot);seed_cache(&w,c->before);
        if(c->restart){assert(!npe_world_restart_game(file,&w));check_cache(&w,c->after);continue;}
        assert(!npe_world_init(file,&saved,c->map,1,1));const char *path="tmp/item-cache.npes";
        assert(!npe_world_save(file,&saved,path));assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);
        assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);
        assert(!npe_world_load(file,&cold,path));NpeMap map;assert(!npe_map(file,c->map-1,&map));
        for(unsigned j=0;j<5;++j){NpeMapItemEvent e;int found=npe_map_item_event(&map,j+1,&e);assert(found>=0);
            assert(cold.item_events[j].quantity==(found?e.quantity:0));}
        NpeWorld before=w;FILE *out=fopen(path,"wb");assert(out&&fwrite("NPESAV17",1,8,out)==8&&!fclose(out));
        assert(npe_world_restore(file,&w,path)<0&&!memcmp(&before,&w,sizeof(w))&&!remove(path));
        uint8_t tail[710]={0};tail[652+13*3+2]=6;NpeMap bad=map;bad.tail=tail;bad.tail_size=sizeof(tail);
        assert(npe_world_load_item_events(&w,&bad)<0&&!memcmp(&before,&w,sizeof(w)));
    }
    NpeWorld a=*baseline,b=a;seed_cache(&b,item_cache_cases[0].before);
    assert(!npe_world_save(file,&a,"tmp/item-wire-a.npes")&&!npe_world_save(file,&b,"tmp/item-wire-b.npes"));
    FILE *fa=fopen("tmp/item-wire-a.npes","rb"),*fb=fopen("tmp/item-wire-b.npes","rb");assert(fa&&fb);
    int ca,cb;do{ca=fgetc(fa);cb=fgetc(fb);assert(ca==cb);}while(ca!=EOF);
    assert(!fclose(fa)&&!fclose(fb)&&!remove("tmp/item-wire-a.npes")&&!remove("tmp/item-wire-b.npes"));
}
int main(int argc,char **argv){NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld baseline,w;
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("item service",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!npe_world_new_game(&file,&baseline));live=&w;check_cache_lifetime(&file,&baseline);
    for(case_index=0;case_index<sizeof(item_cases)/sizeof(*item_cases);++case_index){const ItemCase *c=&item_cases[case_index];configure(&w,&baseline,c);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);active=0;draw_pixels(renderer,texture,pixels);
        save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&w,renderer,pixels,1);
        submits=0;active=1;fake_clock=case_index==1?UINT32_MAX-1000:0;Uint32 start=fake_clock;
        assert(advance_world_events(window,renderer,texture,&file,&font,&w,12,0)>=0);
        assert(fake_clock==start&&submits==c->commits);
        assert(live_quantity(&w,c)==c->after.quantity&&w.game_mode==c->after.mode&&w.story_event_id==c->after.event&&w.map_init_pending==c->after.init&&w.random_state==11);
        if((c->before.event&255)>0&&(c->before.event&255)<=5){
            if(c->item&255){NpeItem item;assert(!npe_native_item(&file,c->item&255,&item));assert(item.name_size<12&&!memcmp(item.name,c->native_name,item.name_size)&&!c->native_name[item.name_size]);}
            else for(unsigned j=0;j<12;++j)assert(!c->native_name[j]);
        }
        if(c->real){NpeMapItemEvent event;NpeMap map;assert(!npe_map(&file,0,&map)&&npe_map_item_event(&map,c->before.event,&event)==1);
            assert(event.quantity==c->quantity&&event.item_index==c->item&&event.add_item==c->adding&&event.next_id==c->next&&event.next_mode==1);}

#ifdef NPE_ITEM_RESIDENT_PUBLIC
        uint8_t resident_bytes[80];
        assert(!npe_rpg_runtime_read(&file,0x5276b0,resident_bytes,40));
        assert(!npe_rpg_runtime_read(&file,0x5276da,resident_bytes+40,40));
        assert(!memcmp(resident_bytes,item_resident_cases[case_index].after,80));
        assert(!memcmp(w.item_record_bytes,c->native_name,12));
        npe_rpg_runtime_close(&file);NpeWorld direct;
        configure(&direct,&baseline,c);assert(npe_world_apply_item_event(&file,&direct)==1);
        assert(live_quantity(&direct,c)==c->after.quantity&&direct.game_mode==c->after.mode&&direct.story_event_id==c->after.event);
        assert(!npe_rpg_runtime_read(&file,0x5276b0,resident_bytes,40));
        assert(!npe_rpg_runtime_read(&file,0x5276da,resident_bytes+40,40));
        assert(!memcmp(resident_bytes,item_resident_cases[case_index].after,80)&&!memcmp(direct.item_record_bytes,c->native_name,12));
#else
        active=0;const char *path="tmp/item-negative.npes";assert(!npe_world_save(&file,&w,path));NpeWorld loaded;
        assert(!npe_world_load(&file,&loaded,path)&&loaded.condition_words[c->item]==w.condition_words[c->item]&&!remove(path));
#endif
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
    }
#ifndef NPE_ITEM_RESIDENT_PUBLIC
    case_index=0;configure(&w,&baseline,&item_cases[0]);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);
    active=0;draw_pixels(renderer,texture,pixels);submits=0;active=quit_requested=1;
    assert(show_item_event(renderer,texture,&file,&font,&w)==-2&&submits==1&&w.game_mode==12&&w.story_event_id==1&&w.condition_words[1]==7&&w.map_init_pending==1);
    active=0;npe_rpg_runtime_close(&file);
#endif
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
#ifdef NPE_ITEM_RESIDENT_PUBLIC
    printf("Item resident:16 original paths,%u C/SDL commits,16 direct services,80 end-table bytes per path,full-word/shared/raw aliases and8 cache loads passed\n",total);
#else
    printf("Item service:22 original paths,%u C/SDL commits,8 original cache loads,word count/wrap/low-byte,fixed5,warm lifetime,save bytes and quit passed\n",total-1);
#endif
    return 0;
}
