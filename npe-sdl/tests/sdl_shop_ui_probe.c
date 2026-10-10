#include "../npe.h"
#include "../sound_sdl.h"
#include "../text.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef main
#undef main
#endif
#ifdef NPE_SHOP_PAGE_MATRIX
#include "shop_page_native.h"
#define SHOP_PREFIX "native-shop-page"
#define NPE_SHOP_CONTROL_MATRIX 1
#define NPE_SHOP_RESIDENT_MATRIX 1
#elif defined(NPE_SHOP_CATALOG_MATRIX)
#include "shop_catalog_native.h"
#define SHOP_PREFIX "native-shop-catalog"
#define NPE_SHOP_CONTROL_MATRIX 1
#define NPE_SHOP_RESIDENT_MATRIX 1
#elif defined(NPE_SHOP_CONTROL_MATRIX)
#include "shop_control_binding_native.h"
#define SHOP_PREFIX "native-shop-control-binding"
#define NPE_SHOP_RESIDENT_MATRIX 1
#elif defined(NPE_SHOP_SELECTION_MATRIX)
#include "shop_selection_native.h"
#define SHOP_PREFIX "native-shop-selection"
#define NPE_SHOP_RESIDENT_MATRIX 1
#elif defined(NPE_SHOP_ITEM_CACHE_MATRIX)
#include "shop_item_cache_native.h"
#define SHOP_PREFIX "native-shop-item-cache"
#define NPE_SHOP_RESIDENT_MATRIX 1
#elif defined(NPE_SHOP_BOUNDARY_MATRIX)
#include "shop_boundary_native.h"
#define SHOP_PREFIX "native-shop-boundary"
#elif defined(NPE_SHOP_WINDOW_MATRIX)
#include "shop_window_native.h"
#define SHOP_PREFIX "native-shop-window"
#else
#include "shop_ui_native.h"
#define SHOP_PREFIX "native-shop-ui"
#endif
static unsigned case_index,phase_index,commits,stage;
static int active;
#ifdef NPE_SHOP_PAGE_MATRIX
static unsigned phase_sent;
static unsigned quit_pending,pending_quits;
static int page_wait(SDL_Event *,int,int,const char *);
#endif
static Uint32 fake_clock;
static NpeWorld *live;
static NpeFont probe_font;
static SDL_Texture *app_texture;
static SDL_Texture *probe_texture(SDL_Renderer *r,Uint32 f,int a,int w,int h){app_texture=SDL_CreateTexture(r,f,a,w,h);return app_texture;}
static int input(SDL_Event *);
static int outer(SDL_Event *,int);
static void present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static Uint32 ticks(void){return fake_clock;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_WaitEvent input
#ifdef NPE_SHOP_PAGE_MATRIX
#define SDL_WaitEventTimeout(event,slice) page_wait(event,slice,timeout,caller)
#else
#define SDL_WaitEventTimeout outer
#endif
#define SDL_RenderPresent present
#define SDL_GetTicks ticks
#define SDL_CreateWindow probe_window
#define SDL_CreateTexture probe_texture
#define npe_sound_select probe_sound
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEvent
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef SDL_GetTicks
#undef SDL_CreateWindow
#undef SDL_CreateTexture
#undef npe_sound_select
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);}
static void state(const ShopUiState *s){uint8_t ids[254];unsigned count;
#ifndef NPE_SHOP_CONTROL_MATRIX
 assert(!npe_world_shop_items((const NpeFile *)live->battle_file,live,ids,&count));
#else
 /* The original popup never asks for a buy catalog after its independent
    transaction flag was changed under a sell entry context. */
 (void)ids;(void)count;
#endif
 unsigned selected=live->shop_selected_item;
 fprintf(stderr,"%s phase%u ui=%u quantity=%u choice=%u money=%u owned=%u selected=%u\n",shop_ui_cases[case_index].name,phase_index,live->shop_ui,live->shop_quantity,live->shop_choice,live->money,live->condition_words[shop_ui_cases[case_index].item],selected);
 assert(live->game_mode==s->mode&&live->event_context_id==s->context&&live->money==s->money&&live->condition_words[shop_ui_cases[case_index].item]==s->owned&&selected==s->selected&&live->shop_ui==s->ui&&live->shop_quantity==s->quantity&&live->shop_choice==s->choice&&live->shop_description_start==s->descStart&&live->shop_description_end==s->descEnd&&live->shop_first_item==s->first&&live->shop_last_item==s->last&&live->shop_row==s->row);
#ifdef NPE_SHOP_RESIDENT_MATRIX
 assert(live->item_description_length==s->descLength&&live->inventory_prefix_word==s->inventoryPrefix);
#endif
#ifdef NPE_SHOP_CONTROL_MATRIX
 assert(live->shop_kind==s->kind&&live->shop_transaction_sell==s->sellFlag);
 uint8_t flag;assert(!npe_rpg_runtime_read(live->battle_file,0x527ac2,&flag,1)&&flag==s->sellFlag);
#endif
#if defined(NPE_SHOP_CATALOG_MATRIX) || defined(NPE_SHOP_PAGE_MATRIX)
 uint8_t catalog[255],scan[3];assert(!npe_world_resident_read(live,0x526dba,catalog,255)&&!memcmp(catalog,s->catalog,255));
 for(unsigned i=0;i<3;++i)assert(!npe_rpg_runtime_read(live->battle_file,0x527a10+i*2,scan+i,1));
 assert(scan[0]==s->scanFirst&&scan[1]==s->scanLast&&scan[2]==s->inventoryOnly);
#endif
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;char path[128];
 assert(commits<shop_ui_cases[case_index].commits);
#ifdef NPE_SHOP_RESIDENT_MATRIX
 snprintf(path,sizeof(path),"tmp/%s-%s-%u.idx",SHOP_PREFIX,shop_ui_cases[case_index].name,commits);
#else
 snprintf(path,sizeof(path),"tmp/%s-%u-commit-%u.idx",SHOP_PREFIX,case_index,commits);
#endif
 uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
 FILE *f=fopen(path,"rb");assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&!fclose(f));
 assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
 unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
 fprintf(stderr,"%s commit%u C=%u SDL=%u\n",shop_ui_cases[case_index].name,commits,d,sd);
 if(d){f=fopen("tmp/shop-ui-actual.idx","wb");assert(f&&fwrite(last_pixel_frame,1,sizeof(native),f)==sizeof(native)&&!fclose(f));}
 assert(!d&&!sd);
#ifdef NPE_SHOP_RESIDENT_MATRIX
 const ShopItemFrame *frame=&shop_ui_cases[case_index].frame[commits];
 assert(live->item_record_id==frame->cache.id&&!memcmp(live->item_record_bytes,frame->cache.bytes,142));
 state(&frame->state);assert(live->item_description_length==frame->state.descLength);
#endif
 assert(++commits<=shop_ui_cases[case_index].commits);
}
static SDL_Keycode keycode(unsigned key){switch(key){case 0x86:return SDLK_RETURN;case 0x82:return SDLK_UP;case 0x83:return SDLK_DOWN;case 0x80:return SDLK_j;case 0x81:return SDLK_l;case 0x8e:return SDLK_F4;case 0x84:return SDLK_LEFT;case 0x85:return SDLK_RIGHT;default:assert(0);return 0;}}
static int next_input(SDL_Event *e){const ShopUiCase *c=&shop_ui_cases[case_index];
 if(phase_index<c->count && commits==c->phase[phase_index].end
#ifdef NPE_SHOP_PAGE_MATRIX
 && phase_sent
#endif
 ){state(&c->phase[phase_index].after);
#ifdef NPE_SHOP_RESIDENT_MATRIX
  assert(live->item_record_id==c->phase[phase_index].cache.id&&!memcmp(live->item_record_bytes,c->phase[phase_index].cache.bytes,142));
#endif
  ++phase_index;
#ifdef NPE_SHOP_PAGE_MATRIX
  phase_sent=0;
#endif
 }
 memset(e,0,sizeof(*e));if(phase_index==c->count){assert(commits==c->commits);active=0;e->type=SDL_QUIT;return 1;}
#ifdef NPE_SHOP_RESIDENT_MATRIX
 if(c->phase[phase_index].forcedRow<256){
  /* Mirror the explicit diagnostic RAM row write, preserving both LCD
     planes. The input handler still performs the original cursor update. */
  live->shop_row=(uint8_t)c->phase[phase_index].forcedRow;
  shop_frame_state.shop_row=live->shop_row;
 }
#endif
#ifdef NPE_SHOP_PAGE_MATRIX
 if(c->phase[phase_index].upper<256){uint8_t upper=(uint8_t)c->phase[phase_index].upper;assert(!npe_rpg_runtime_write(live->battle_file,0x527a12,&upper,1));}
 phase_sent=1;
#endif
 e->type=SDL_KEYDOWN;e->key.keysym.sym=keycode(c->phase[phase_index].key);return 1;
}
static int input(SDL_Event *e){assert(stage==2&&active);return next_input(e);}
static int outer(SDL_Event *e,int timeout){memset(e,0,sizeof(*e));
 if(stage==0){e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;stage=1;return 1;}
 if(stage==1){const ShopUiCase *c=&shop_ui_cases[case_index];
  live->game_mode=4;live->event_context_id=c->selling?10:13;live->shop_kind=live->shop_ready=c->selling?2:1;live->shop_ui=1;live->shop_selection=live->shop_first=0;live->shop_quantity=live->shop_choice=0;live->shop_description_start=live->shop_description_end=0;live->money=c->money;
  live->shop_first_item=c->initial_first;live->shop_last_item=c->initial_last;
#ifdef NPE_SHOP_RESIDENT_MATRIX
  live->item_record_id=(uint8_t)c->seed.id;memcpy(live->item_record_bytes,c->seed.bytes,142);
  live->inventory_prefix_word=c->frame[0].state.inventoryPrefix;
#endif
  memset(live->condition_words,0,sizeof(live->condition_words));for(unsigned i=0;i<c->stock_count;++i)live->condition_words[c->stock[i][0]]=(uint16_t)c->stock[i][1];
#if defined(NPE_SHOP_CATALOG_MATRIX) || defined(NPE_SHOP_PAGE_MATRIX)
  assert(!npe_rpg_runtime_write(live->battle_file,0x526dba,c->seed_catalog,255));
#endif
  assert(!npe_world_shop_begin((const NpeFile *)live->battle_file,live));
  active=1;stage=2;fake_clock=100;unsigned levels[3]={0};assert(!draw_world(last_pixel_renderer,app_texture,(const NpeFile *)live->battle_file,live,0,0,0,0,0,0,levels,0,&probe_font));
#ifdef NPE_SHOP_CONTROL_MATRIX
  /* Match original diagnostic writes after the initial list submission. */
  if(c->afterFlag<256){uint8_t value=(uint8_t)c->afterFlag;assert(!npe_rpg_runtime_write(live->battle_file,0x527ac2,&value,1));}
  if(c->afterKind<256){uint8_t value=(uint8_t)c->afterKind;assert(!npe_rpg_runtime_write(live->battle_file,0x527a0e,&value,1));}
#endif
#if defined(NPE_SHOP_CATALOG_MATRIX) || defined(NPE_SHOP_PAGE_MATRIX)
#ifdef NPE_SHOP_PAGE_MATRIX
  if(c->clearCatalog){uint8_t zero[255]={0};assert(!npe_rpg_runtime_write(live->battle_file,0x526dba,zero,255));}
#endif
  for(unsigned i=0;i<c->patch_count;++i){uint8_t b=(uint8_t)c->patch[i][1];assert(!npe_rpg_runtime_write(live->battle_file,0x526dba+c->patch[i][0],&b,1));}
  if(c->afterContext<65536){uint8_t b[]={(uint8_t)(c->afterContext>>8),(uint8_t)c->afterContext};assert(!npe_rpg_runtime_write(live->battle_file,0x527938,b,2));}
  const unsigned v[]={c->scanFirst,c->scanLast,c->inventoryOnly};
  for(unsigned i=0;i<3;++i)if(v[i]<256){uint8_t b=(uint8_t)v[i];assert(!npe_rpg_runtime_write(live->battle_file,0x527a10+2*i,&b,1));}
#endif
#ifdef NPE_SHOP_PAGE_MATRIX
  const unsigned indices[]={c->afterSelected,c->afterFirst,c->afterLast};
  const uint32_t addresses[]={0x527a4c,0x527ac6,0x527ab6};
  for(unsigned i=0;i<3;++i)if(indices[i]<65536){uint8_t b[]={(uint8_t)(indices[i]>>8),(uint8_t)indices[i]};assert(!npe_rpg_runtime_write(live->battle_file,addresses[i],b,2));}
  if(c->afterRow<256){uint8_t row=(uint8_t)c->afterRow;assert(!npe_rpg_runtime_write(live->battle_file,0x527a46,&row,1));}
#endif
  return next_input(e);
 }
 if(timeout>0 && active && commits<shop_ui_cases[case_index].phase[phase_index].end){fake_clock+=(Uint32)timeout;return 0;}
 return next_input(e);
}
#ifdef NPE_SHOP_PAGE_MATRIX
static int page_wait(SDL_Event *e,int slice,int logical_timeout,const char *caller){
 if(!strcmp(caller,"game_shop_move_display")){
  assert(live->shop_page_pending);
  /* Zero-LCD phases must be checked only after delivery of their key.
     Upper-bound writes occur while the same original call is suspended. */
  assert(phase_index<shop_ui_cases[case_index].count && phase_sent &&
         commits==shop_ui_cases[case_index].phase[phase_index].end);
  if(quit_pending){
   const ShopUiPhase *p=&shop_ui_cases[case_index].phase[phase_index];
   assert(phase_index==0 && commits==1);state(&p->after);
   assert(live->item_record_id==p->cache.id&&!memcmp(live->item_record_bytes,p->cache.bytes,142));
   ++pending_quits;active=0;memset(e,0,sizeof(*e));e->type=SDL_QUIT;return 1;
  }
  return next_input(e);
 }
 if(logical_timeout>=0 && stage==2 && active && phase_sent &&
    commits<shop_ui_cases[case_index].phase[phase_index].end){
  fake_clock+=(Uint32)(slice>0?slice:1);memset(e,0,sizeof(*e));return 0;
 }
 return outer(e,logical_timeout);
}
#endif
int main(int argc,char **argv){
#ifdef NPE_SHOP_PAGE_MATRIX
 assert(argc==2 || (argc==3&&!strcmp(argv[2],"--quit-pending")));quit_pending=argc==3;
#else
 assert(argc==2);
#endif
 assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));assert(!npe_font_load(&probe_font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&probe_font,"npe-sdl/assets/gbk16-native.bin"));for(case_index=0;case_index<sizeof(shop_ui_cases)/sizeof(*shop_ui_cases);++case_index){
#ifdef NPE_SHOP_PAGE_MATRIX
 if(quit_pending&&strcmp(shop_ui_cases[case_index].name,"page-pending-count0"))continue;
 phase_sent=0;
#endif
 stage=active=commits=phase_index=0;last_pixel_renderer=NULL;world_map_frame_valid=0;fake_clock=0;
 char *args[]={"npe-sdl",argv[1],NULL};
 assert(!run(2,args));}
 npe_font_free(&probe_font);
#ifdef NPE_SHOP_PAGE_MATRIX
 if(quit_pending){assert(pending_quits==1);puts("SDL quit during native page backfill: actual main shutdown passed");return 0;}
#endif
 puts("Native shop UI paths passed in actual C/SDL main");return 0;}
