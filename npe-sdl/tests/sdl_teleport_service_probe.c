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
typedef struct {unsigned map,x,y,marker,mode,next;} Cache;
typedef struct {unsigned mode,event,context,map,progress,init,sx,sy,vx,vy,direction,enabled,slot,visited;uint32_t rng;Cache cache[2];} TeleState;
typedef struct {const char *name;unsigned map,x,y,marker,mode,next,commits;TeleState before,after;
#ifdef NPE_PARTY_TELEPORT_RAM
 uint8_t before_cache[38],after_cache[38];
#endif
} TeleCase;
typedef struct {const char *name;unsigned map,restart;Cache boot[2],before[2],after[2];} CacheCase;
#ifdef NPE_PARTY_TELEPORT_RAM
#include "teleport_resident_native.h"
#define TELE_PREFIX "tmp/native-teleport-resident-"
#else
#include "teleport_service_native.h"
#define TELE_PREFIX "tmp/native-teleport-service-"
#endif
#include "teleport_cache_native.h"
static unsigned current,submits,total,active,closing;
static Uint32 clock_now;
static Uint32 probe_ticks(void){return clock_now;}
static void probe_delay(Uint32 ms){clock_now+=ms;}
static int probe_poll(SDL_Event *e){if(!active)return 0;memset(e,0,sizeof(*e));e->type=closing?SDL_QUIT:SDL_KEYDOWN;e->key.keysym.sym=SDLK_F4;return 1;}
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
 if(before)snprintf(path,sizeof(path),TELE_PREFIX "%s-before.idx",tele_cases[current].name);
 else snprintf(path,sizeof(path),TELE_PREFIX "%s-%u.idx",tele_cases[current].name,index);
 FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;assert(submits<tele_cases[current].commits);
 uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
 assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
 for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
  sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
 fprintf(stderr,"teleport %s commit%u C=%u SDL=%u\n",tele_cases[current].name,submits,d,sd);assert(!d&&!sd);++submits;++total;
}
static void seed_cache(NpeWorld *w,const Cache *c){for(unsigned i=0;i<2;++i)w->teleport_events[i]=(NpeMapTeleportEvent){
 .destination_map=(uint16_t)c[i].map,.x=(uint8_t)c[i].x,.y=(uint8_t)c[i].y,.marker=(uint8_t)c[i].marker,.next_mode=(uint8_t)c[i].mode,.next_id=(uint16_t)c[i].next};}
static void check_cache(const NpeWorld *w,const Cache *c){for(unsigned i=0;i<2;++i){NpeMapTeleportEvent e;
 assert(npe_world_teleport_event(w,i+1,&e)==1&&!e.raw&&!e.id);
 assert(e.destination_map==c[i].map&&e.x==c[i].x&&e.y==c[i].y&&e.marker==c[i].marker&&e.next_mode==c[i].mode&&e.next_id==c[i].next);}}
static void configure(NpeWorld *w,const NpeWorld *base,const TeleCase *c){*w=*base;const TeleState *s=&c->before;
 w->game_mode=16;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;w->story_progress=(uint16_t)s->progress;
 w->view=(NpeViewport){.screen_x=s->sx,.screen_y=s->sy,.offset_x=s->vx,.offset_y=s->vy};w->x=s->sx+s->vx;w->y=s->sy+s->vy;
 w->direction=(uint8_t)s->direction;w->map_init_pending=(uint8_t)s->init;w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;
 w->lcd_background=0;w->random_state=s->rng;seed_cache(w,s->cache);
}
static void cache_lifetime(const NpeFile *file,const NpeWorld *base){for(unsigned i=0;i<10;++i){const CacheCase *c=&cache_cases[i];NpeWorld w=*base,saved,cold;
 check_cache(base,c->boot);seed_cache(&w,c->before);
 if(c->restart){assert(!npe_world_restart_game(file,&w));check_cache(&w,c->after);continue;}
 assert(!npe_world_init(file,&saved,c->map,1,1));const char *path="tmp/teleport-cache.npes";
 assert(!npe_world_save(file,&saved,path)&&!npe_world_restore(file,&w,path));check_cache(&w,c->after);
 assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);assert(!npe_world_load(file,&cold,path));
 NpeMap map;assert(!npe_map(file,c->map-1,&map));for(unsigned j=0;j<2;++j){NpeMapTeleportEvent e;int found=npe_map_teleport_event(&map,j+1,&e);assert(found>=0);
  assert(cold.teleport_events[j].destination_map==(found?e.destination_map:0));}
 NpeWorld before=w;FILE *out=fopen(path,"wb");assert(out&&fwrite("NPESAV17",1,8,out)==8&&!fclose(out));
 assert(npe_world_restore(file,&w,path)<0&&!memcmp(&before,&w,sizeof(w))&&!remove(path));
 uint8_t tail[710]={0};tail[652+5*3+2]=3;NpeMap bad=map;bad.tail=tail;bad.tail_size=sizeof(tail);
 assert(npe_world_load_teleport_events(&w,&bad)<0&&!memcmp(&before,&w,sizeof(w)));
 }
 NpeWorld a=*base,b=a;seed_cache(&b,cache_cases[0].before);assert(!npe_world_save(file,&a,"tmp/tele-wire-a.npes")&&!npe_world_save(file,&b,"tmp/tele-wire-b.npes"));
 FILE *fa=fopen("tmp/tele-wire-a.npes","rb"),*fb=fopen("tmp/tele-wire-b.npes","rb");assert(fa&&fb);int x,y;do{x=fgetc(fa);y=fgetc(fb);assert(x==y);}while(x!=EOF);
 assert(!fclose(fa)&&!fclose(fb)&&!remove("tmp/tele-wire-a.npes")&&!remove("tmp/tele-wire-b.npes"));
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld base,w;
 assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
 assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
 SDL_Window *win=SDL_CreateWindow("teleport service",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(win);SDL_Renderer *r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(r);
 SDL_Texture *tex=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(tex);
 assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin")&&!npe_world_new_game(&file,&base));cache_lifetime(&file,&base);
#ifdef NPE_PARTY_TELEPORT_RAM
 uint64_t fingerprint=npe_game_fingerprint(&file);
#endif
 for(current=0;current<12;++current){const TeleCase *c=&tele_cases[current];configure(&w,&base,c);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);
#ifdef NPE_PARTY_TELEPORT_RAM
 assert(!npe_world_resources_open(&file,&w));assert(!npe_rpg_runtime_write(&file,0x52788e,c->before_cache,38));
#endif
  active=0;draw_pixels(r,tex,pixels);save_map_edges(&file,r,pixels);retain_world_frame(&file,&w,r,pixels,1);submits=0;active=1;clock_now=current==1?UINT32_MAX-1000:0;
  Uint32 start=clock_now;assert(advance_world_events(win,r,tex,&file,&font,&w,16,0)>=0&&clock_now==start&&submits==c->commits);
  const TeleState *s=&c->after;assert(w.game_mode==s->mode&&w.story_event_id==s->event&&w.map_number==s->map&&w.story_progress==s->progress&&w.map_init_pending==s->init);
  fprintf(stderr,"teleport state actual=%u,%u,%u,%u dir=%u slot=%u rng=%08x expected=%u,%u,%u,%u dir=%u slot=%u rng=%08x\n",w.view.screen_x,w.view.screen_y,w.view.offset_x,w.view.offset_y,w.direction,w.map_music_slot,w.random_state,s->sx,s->sy,s->vx,s->vy,s->direction,s->slot,s->rng);
  assert(w.view.screen_x==s->sx&&w.view.screen_y==s->sy&&w.view.offset_x==s->vx&&w.view.offset_y==s->vy&&w.direction==s->direction&&w.map_music_slot==s->slot&&w.random_state==s->rng);
  assert(w.condition5_bytes[c->map]==s->visited);check_cache(&w,s->cache);
#ifdef NPE_PARTY_TELEPORT_RAM
  uint8_t actual[38];assert(!npe_rpg_runtime_read(&file,0x52788e,actual,38));assert(!memcmp(actual,c->after_cache,38));
  assert(npe_game_fingerprint(&file)==fingerprint&&!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
#endif
 }
 active=0;current=0;configure(&w,&base,&tele_cases[0]);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);draw_pixels(r,tex,pixels);save_map_edges(&file,r,pixels);
 submits=0;active=closing=1;assert(show_teleport_event(r,tex,&file,&font,&w)==-2&&submits==2&&w.map_number==2&&w.map_init_pending==0);
 active=0;npe_font_free(&font);SDL_DestroyTexture(tex);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);SDL_Quit();npe_free(&file);
 printf("Teleport service:12 original diagnostic paths,%u C/SDL commits,10 original UI cache loads,fixed2/low-byte,warm lifetime,map notice,RNG,save bytes and quit passed\n",total-2);return 0;
}
