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
typedef struct {unsigned next,progress,mode;} StoryCache;
typedef struct {unsigned id,x,y,direction;} Npc;
typedef struct {unsigned mode,event,context,map,progress,init,sx,sy,vx,vy,direction,enabled,slot,visited;uint32_t rng;Cache cache[2];StoryCache story[15];uint8_t npc_state[200];Npc npc[20];} TeleState;
typedef struct {const char *name;unsigned map,x,y,marker,mode,next,commits;TeleState before,after;} TeleCase;
typedef struct {const char *name;unsigned map,restart;Cache boot[2],before[2],after[2];} CacheCase;
#include "teleport_followup_native.h"

static unsigned current,submits,total,active,closing;
static Uint32 clock_now;
static Uint32 probe_ticks(void){return clock_now;}
static void probe_delay(Uint32 ms){clock_now+=ms;}
static int probe_poll(SDL_Event *e){if(!active)return 0;memset(e,0,sizeof(*e));e->type=closing?SDL_QUIT:SDL_KEYDOWN;e->key.keysym.sym=SDLK_F4;return 1;}
static int probe_wait(SDL_Event *e){memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_SPACE;return 1;}
static void probe_present(SDL_Renderer *);
#define SDL_WaitEvent probe_wait
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_RenderPresent probe_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_RenderPresent
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[160];
 if(before)snprintf(path,sizeof(path),"tmp/native-teleport-followup-%s-before.idx",tele_cases[current].name);
 else snprintf(path,sizeof(path),"tmp/native-teleport-followup-%s-%u.idx",tele_cases[current].name,index);
 FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;assert(submits<tele_cases[current].commits);
 uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
 assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
 for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
  sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
 fprintf(stderr,"teleport %s commit%u C=%u SDL=%u\n",tele_cases[current].name,submits,d,sd);if(d){FILE *out=fopen("tmp/teleport-followup-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
 assert(!d&&!sd);++submits;++total;
}
static void seed_cache(NpeWorld *w,const Cache *c){for(unsigned i=0;i<2;++i)w->teleport_events[i]=(NpeMapTeleportEvent){
 .destination_map=(uint16_t)c[i].map,.x=(uint8_t)c[i].x,.y=(uint8_t)c[i].y,.marker=(uint8_t)c[i].marker,.next_mode=(uint8_t)c[i].mode,.next_id=(uint16_t)c[i].next};}
static void check_cache(const NpeWorld *w,const Cache *c){for(unsigned i=0;i<2;++i){NpeMapTeleportEvent e;
 assert(npe_world_teleport_event(w,i+1,&e)==1&&!e.raw&&!e.id);
 assert(e.destination_map==c[i].map&&e.x==c[i].x&&e.y==c[i].y&&e.marker==c[i].marker&&e.next_mode==c[i].mode&&e.next_id==c[i].next);}}
static void configure(const NpeFile *file,NpeWorld *w,const NpeWorld *base,const TeleCase *c){*w=*base;const TeleState *s=&c->before;
 NpeWorld saved;const char *path="tmp/teleport-followup-config.npes";assert(!npe_world_init(file,&saved,s->map,1,1)&&!npe_world_save(file,&saved,path)&&!npe_world_restore(file,w,path)&&!remove(path));
 for(unsigned i=0;i<15;++i){const StoryCache *k=&s->story[i];assert(w->story_events[i].next_id==k->next&&w->story_events[i].story_progress==k->progress&&w->story_events[i].mode==k->mode);}
 memcpy(w->npc_state,s->npc_state,sizeof(s->npc_state));
 for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}

 w->game_mode=16;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;w->story_progress=(uint16_t)s->progress;
 w->view=(NpeViewport){.screen_x=s->sx,.screen_y=s->sy,.offset_x=s->vx,.offset_y=s->vy};w->x=s->sx+s->vx;w->y=s->sy+s->vy;
 w->direction=(uint8_t)s->direction;w->map_init_pending=(uint8_t)s->init;w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;
 w->lcd_background=0;w->random_state=s->rng;seed_cache(w,s->cache);
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld base,w;
 assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
 assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
 SDL_Window *win=SDL_CreateWindow("teleport service",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(win);SDL_Renderer *r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(r);
 SDL_Texture *tex=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(tex);
 assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin")&&!npe_world_new_game(&file,&base));
 for(current=0;current<6;++current){const TeleCase *c=&tele_cases[current];configure(&file,&w,&base,c);NpeWorld numeric=w;assert(!numeric.defer_story_events&&npe_world_apply_teleport_event(&file,&numeric)==1);
 if(c->mode==14){NpeMap dest;NpeMapStoryEvent e[15];unsigned count;assert(!npe_map(&file,c->map-1,&dest)&&!npe_map_story_events(&dest,e,&count));
   unsigned expected_progress=c->next<=count?e[c->next-1].story_progress:c->before.story[c->next-1].progress;
   fprintf(stderr,"numeric %s progress=%u expected=%u mode=%u\n",c->name,numeric.story_progress,expected_progress,numeric.game_mode);
   assert(numeric.story_progress==expected_progress);
   unsigned expected_mode=c->next<=count?e[c->next-1].mode:c->before.story[c->next-1].mode;
   unsigned expected_next=c->next<=count?e[c->next-1].next_id:c->before.story[c->next-1].next;
   assert(numeric.game_mode==expected_mode&&numeric.story_event_id==expected_next&&numeric.map_number==c->map);
 }
 uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);
  active=0;draw_pixels(r,tex,pixels);save_map_edges(&file,r,pixels);retain_world_frame(&file,&w,r,pixels,1);submits=0;active=1;clock_now=current==1?UINT32_MAX-1000:0;
  Uint32 start=clock_now;assert(advance_world_events(win,r,tex,&file,&font,&w,16,0)>=0&&clock_now==start&&submits==c->commits);
  const TeleState *s=&c->after;assert(w.game_mode==s->mode&&w.story_event_id==s->event&&w.map_number==s->map&&w.story_progress==s->progress&&w.map_init_pending==s->init);
  fprintf(stderr,"teleport state actual=%u,%u,%u,%u dir=%u slot=%u rng=%08x expected=%u,%u,%u,%u dir=%u slot=%u rng=%08x\n",w.view.screen_x,w.view.screen_y,w.view.offset_x,w.view.offset_y,w.direction,w.map_music_slot,w.random_state,s->sx,s->sy,s->vx,s->vy,s->direction,s->slot,s->rng);
  assert(w.view.screen_x==s->sx&&w.view.screen_y==s->sy&&w.view.offset_x==s->vx&&w.view.offset_y==s->vy&&w.direction==s->direction&&w.map_music_slot==s->slot&&w.random_state==s->rng);
  assert(w.condition5_bytes[c->map]==s->visited);check_cache(&w,s->cache);assert(!memcmp(w.npc_state,s->npc_state,sizeof(s->npc_state)));
 for(unsigned i=0;i<15;++i){const StoryCache *k=&s->story[i];assert(w.story_events[i].next_id==k->next&&w.story_events[i].story_progress==k->progress&&w.story_events[i].mode==k->mode);}

 }
 active=0;npe_font_free(&font);SDL_DestroyTexture(tex);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);SDL_Quit();npe_free(&file);
 printf("Teleport followup:six original real/diagnostic paths,%u C/SDL commits,source/destination partial cache,numeric backend,NPE stories,continuous teleport and RNG passed\n",total);return 0;
}
