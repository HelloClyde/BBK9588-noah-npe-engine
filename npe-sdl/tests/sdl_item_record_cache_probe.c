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
typedef struct {unsigned id,x,y,direction;} CacheNpc;
typedef struct {unsigned mode,event,map,progress,init,sx,sy,vx,vy,direction,loaded,quantity;uint8_t bytes[142];CacheNpc npc[20];} ItemRecordState;
typedef struct {const char *name;unsigned item,commits;ItemRecordState before,after;} ItemRecordCase;
typedef struct {unsigned selected,loaded,length,start,end;uint8_t bytes[142];} ItemResidentState;
typedef struct {const char *route;ItemResidentState before,after,stages[6];} ItemLifetimeCase;
#include "item_record_cache_native.h"
static unsigned case_index,submits,total;
static unsigned equipment_phase;
static int equipment_active;
static int active;
static NpeWorld *live;
static Uint32 clock_ms;
static Uint32 ticks(void){return clock_ms;}
static void delay(Uint32 ms){clock_ms+=ms;}
static int poll(SDL_Event *e){memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;}
static void present(SDL_Renderer *);
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent poll
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_RenderPresent
static void read_frame(unsigned before,unsigned index,uint8_t *pixels){char path[180];
    if(before)snprintf(path,sizeof(path),"tmp/native-item-record-cache-%s-before.idx",item_record_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-item-record-cache-%s-%u.idx",item_record_cases[case_index].name,index);
    FILE *f=fopen(path,"rb");assert(f&&fread(pixels,1,28800,f)==28800&&fgetc(f)==EOF&&!fclose(f));
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const ItemRecordCase *c=&item_record_cases[case_index];uint8_t reference[28800];uint32_t rgba[28800];
    const ItemResidentState *resident=NULL;
    if(equipment_active){char path[180];assert(!submits);
        snprintf(path,sizeof(path),"tmp/native-item-record-equipment-%u-0.idx",equipment_phase);
        FILE *f=fopen(path,"rb");assert(f&&fread(reference,1,28800,f)==28800&&fgetc(f)==EOF&&!fclose(f));
        resident=&item_lifetime_cases[5].stages[equipment_phase];
    }else{assert(submits<c->commits);read_frame(0,submits,reference);}
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
    for(unsigned i=0;i<28800;++i){unsigned v=reference[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Item record %s phase%u commit%u C%u SDL%u\n",equipment_active?"equipment":c->name,equipment_phase,submits,d,sd);assert(!d&&!sd);
    assert(live->item_record_id==(resident?resident->loaded:c->after.loaded)&&!memcmp(live->item_record_bytes,resident?resident->bytes:c->after.bytes,142));
    if(resident)assert(live->equipment_description_item==resident->selected&&live->item_description_length==resident->length&&live->shop_description_start==resident->start&&live->shop_description_end==resident->end);
    ++submits;++total;
}
static void configure(NpeWorld *w,const NpeWorld *baseline,const ItemRecordCase *c){
    *w=*baseline;const ItemRecordState *s=&c->before;
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->story_progress=(uint16_t)s->progress;
    w->map_init_pending=(uint8_t)s->init;w->view=(NpeViewport){s->sx,s->sy,s->vx,s->vy};w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=s->direction;
    w->music_enabled=0;w->lcd_background=0;npe_world_seed(w,11);
    uint8_t loaded=(uint8_t)s->loaded;
    assert(!npe_world_resident_write(w,0x527a72,&loaded,1));
    assert(!npe_world_resident_write(w,0x52739e,s->bytes,142));
    w->condition_words[c->item]=(uint16_t)s->quantity;
    w->item_events[0]=(NpeMapItemEvent){.quantity=1,.item_index=(uint16_t)c->item,.next_id=789,.add_item=1,.next_mode=1};
    for(unsigned i=0;i<20;++i){w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;w->npc_override_direction[i]=(uint8_t)s->npc[i].direction;}
}
static void lifetime_checks(const NpeFile *file,const NpeWorld *baseline){
    NpeWorld w=*baseline,loaded;NpeItem item;w.item_record_id=1;memcpy(w.item_record_bytes,item_record_cases[1].before.bytes,142);
    assert(!npe_world_item(NULL,&w,257,&item)&&item.name_size==8&&item.effect_type==0xa5&&item.price==0xa5a5);
    assert(!memcmp(w.item_record_bytes,item_record_cases[1].before.bytes,142));
    uint8_t copy[142];memcpy(copy,w.item_record_bytes,142);
    const char *path="tmp/item-record-cache.npes";w.inventory_prefix_word=321;
    w.inventory_column=2;
    assert(!npe_world_save(file,&w,path)&&!npe_world_load(file,&loaded,path)&&!loaded.item_record_id&&!loaded.inventory_prefix_word);
    for(unsigned i=0;i<142;++i)assert(!loaded.item_record_bytes[i]);
    assert(!loaded.inventory_column);
    assert(!npe_world_restore(file,&w,path)&&w.item_record_id==1&&!memcmp(w.item_record_bytes,copy,142)&&w.inventory_prefix_word==321);
    assert(w.inventory_column==2);
    assert(!npe_world_restart_game(file,&w)&&!w.item_record_id&&w.inventory_prefix_word==321);
    assert(w.inventory_column==2);
    for(unsigned i=0;i<142;++i)assert(!w.item_record_bytes[i]);
    w.item_record_id=1;memcpy(w.item_record_bytes,copy,142);
    w.game_mode=16;w.story_event_id=1;
    w.teleport_events[0]=(NpeMapTeleportEvent){.destination_map=80,.x=27,.y=20,.next_mode=1};
    assert(npe_world_apply_teleport_event(file,&w)==1&&w.item_record_id==1&&!memcmp(w.item_record_bytes,copy,142));
    assert(!npe_world_item(NULL,&w,256,&item)&&!w.item_record_id&&!item.name_size&&!item.description_size&&!item.effect_type);
    for(unsigned i=0;i<142;++i)assert(!w.item_record_bytes[i]);
    w.item_record_id=1;memcpy(w.item_record_bytes,copy,142);
    /* This is a cold constructor, not the original warm map loader. */
    assert(!npe_world_init(file,&w,80,27,20)&&!w.item_record_id&&!w.inventory_prefix_word);
    for(unsigned i=0;i<142;++i)assert(!w.item_record_bytes[i]);
    assert(!remove(path));
    /* Shared resident bytes do not enter the existing V19 wire format. */
    w=*baseline;assert(!npe_world_save(file,&w,path));uint8_t *a,*b;size_t na,nb;assert(!read_file(path,&a,&na));
    w.inventory_prefix_word=65432;
    w.inventory_column=255;
    assert(!npe_world_item(file,&w,91,&item)&&!npe_world_save(file,&w,path)&&!read_file(path,&b,&nb));
    assert(na==nb&&!memcmp(a,b,na));free(a);free(b);assert(!remove(path));
    for(unsigned i=0;i<5;++i){const ItemLifetimeCase *c=&item_lifetime_cases[i];
        w=*baseline;w.item_record_id=(uint8_t)c->before.loaded;memcpy(w.item_record_bytes,c->before.bytes,142);
        if(i<2)assert(!npe_world_restart_game(file,&w));
        else if(i<4){assert(!npe_world_save(file,baseline,path)&&!npe_world_restore(file,&w,path)&&!remove(path));}
        else{w.game_mode=16;w.story_event_id=1;w.teleport_events[0]=(NpeMapTeleportEvent){.destination_map=2,.x=1,.y=1,.next_mode=1,.next_id=789};assert(npe_world_apply_teleport_event(file,&w)==1);}
        assert(w.item_record_id==c->after.loaded&&!memcmp(w.item_record_bytes,c->after.bytes,142));
    }
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeWorld baseline,w;NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error))&&!npe_world_new_game(&file,&baseline));
    lifetime_checks(&file,&baseline);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("shared item record",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));live=&w;
    for(case_index=0;case_index<4;++case_index){const ItemRecordCase *c=&item_record_cases[case_index];configure(&w,&baseline,c);
        uint8_t pixels[28800];read_frame(1,0,pixels);active=0;system_menu_background_valid=system_menu_frame_valid=0;
        draw_pixels(renderer,texture,pixels);save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&w,renderer,pixels,1);
        active=1;submits=0;assert(advance_world_events(window,renderer,texture,&file,&font,&w,12,0)>=0&&submits==3);
        /* The reference's public F4 at tick7 opens the real system menu. */
        assert(npe_world_open_system_menu(&w)==1);
        const unsigned levels[3]={0};
        assert(!draw_world(renderer,texture,&file,&w,0,0,0,0,0,0,levels,0,&font)&&submits==c->commits);
        assert(w.game_mode==c->after.mode&&w.story_event_id==c->after.event&&w.condition_words[c->item]==c->after.quantity&&w.random_state==11);active=0;
    }
    assert(total==16);
    case_index=0;w=baseline;live=&w;const ItemLifetimeCase *c=&item_lifetime_cases[5];
    w.item_record_id=(uint8_t)c->before.loaded;memcpy(w.item_record_bytes,c->before.bytes,142);
    memset(w.equipped_items[0],0,6);w.equipped_items[0][0]=1;
    w.game_mode=6;w.property_page=10;w.property_member=w.equipment_view_slot=0;w.lcd_background=0;
    system_menu_background_valid=system_menu_frame_valid=0;equipment_active=active=1;
    const unsigned levels[3]={0};
    for(equipment_phase=3;equipment_phase<6;++equipment_phase){
        if(equipment_phase>3)w.equipment_view_slot=(uint8_t)(equipment_phase==4?1:0);
        submits=0;assert(!draw_world(renderer,texture,&file,&w,0,0,0,0,0,0,levels,0,&font)&&submits==1);
    }
    active=equipment_active=0;assert(total==19);
    /* Eligible paging an empty slot loadsID0; ordinary empty selection does not. */
    w.equipment_description_item=0;w.shop_description_start=0;w.shop_description_end=65;w.item_description_length=100;
    assert(npe_world_equipment_description_page(&file,&w,1)==1&&!w.item_record_id);
    for(unsigned i=0;i<142;++i)assert(!w.item_record_bytes[i]);
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    puts("Item record cache:4 original UI paths/16 actual C-SDL frames plus3 equipment frames,all142 bytes,6 original lifetime paths,low-byte/same-ID/clear/tail,V19 wire isolation passed");return 0;
}
