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
typedef struct {const char *entry;unsigned map;uint8_t before[100],after[100];} TriggerLoad;
typedef struct {
    unsigned mode,event,context,map,progress,init,sx,sy,vx,vy,direction,enabled,slot;
    unsigned menuPartyCount,menuStage,menuRow,menuMember;
    uint8_t cache[100],npc[280],monster[56],chase[28];uint16_t viewCache[105];
} TriggerState;
typedef struct {const char *name;unsigned numeric_mode,numeric_event,frames;TriggerState before,after;} TriggerCase;
#include "walk_trigger_native.h"
static const TriggerCase *current;
static unsigned active,submits,total;
static void probe_present(SDL_Renderer *);
#define SDL_RenderPresent probe_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent

static void original_frame(unsigned index,int before,uint8_t *pixels){
    char path[200];
    if(before)snprintf(path,sizeof(path),"tmp/native-walk-trigger-%s-before.idx",current->name);
    else snprintf(path,sizeof(path),"tmp/native-walk-trigger-%s-%u.idx",current->name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void cache(const NpeWorld *w,const uint8_t *expected){
    uint8_t actual[100];assert(!npe_world_resident_read(w,0x527490,actual,100));
    for(unsigned i=0;i<100;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"trigger byte%u C%u original%u\n",i,actual[i],expected[i]);assert(0);
    }
}
static void configure(NpeFile *f,NpeWorld *w){
    const TriggerState *s=&current->before;
    assert(!npe_world_init(f,w,s->map,s->sx+s->vx,s->sy+s->vy));
    w->view.offset_x=s->vx;w->view.offset_y=s->vy;w->view.screen_x=s->sx;w->view.screen_y=s->sy;
    w->game_mode=s->mode;w->story_event_id=s->event;w->event_context_id=s->context;
    w->story_progress=s->progress;w->map_init_pending=s->init;w->direction=s->direction;
    w->menu_party_count=s->menuPartyCount;w->system_menu_stage=s->menuStage;
    w->music_enabled=s->enabled;w->map_music_slot=s->slot;w->encounter_steps=0;
    assert(!npe_world_resources_open(f,w));
    uint8_t zero[2]={0};assert(!npe_rpg_runtime_write(f,0x527a5c,zero,2));
    assert(!npe_rpg_runtime_write(f,0x527a32,zero,1));
    uint8_t row=(uint8_t)s->menuRow,member=(uint8_t)s->menuMember;
    assert(!npe_rpg_runtime_write(f,0x527ae2,&row,1)&&!npe_rpg_runtime_write(f,0x527a78,&member,1));
    assert(!npe_rpg_runtime_write(f,0x527490,s->cache,100));
    assert(!npe_rpg_runtime_write(f,0x526ca2,s->npc,280));
    assert(!npe_rpg_runtime_write(f,0x52761a,s->monster,56));
    assert(!npe_rpg_runtime_write(f,0x52783a,s->chase,28));
    for(unsigned i=0;i<105;++i){uint8_t word[2]={(uint8_t)(s->viewCache[i]>>8),(uint8_t)s->viewCache[i]};
        assert(!npe_rpg_runtime_write(f,0x5270b4+2*i,word,2));}
    npe_world_seed(w,11);cache(w,s->cache);
}
static void state(const NpeWorld *w){
    const TriggerState *s=&current->after;
    fprintf(stderr,"%s state %u/%u init%u expected%u/%u init%u\n",current->name,w->game_mode,w->story_event_id,w->map_init_pending,s->mode,s->event,s->init);
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->map_init_pending==s->init);
    assert(w->story_progress==s->progress&&w->map_number==s->map&&w->event_context_id==s->context);
    assert(w->view.screen_x==s->sx&&w->view.screen_y==s->sy&&w->view.offset_x==s->vx&&w->view.offset_y==s->vy);
    assert(w->direction==s->direction&&w->menu_party_count==s->menuPartyCount&&w->system_menu_stage==s->menuStage);
    uint8_t row,member;assert(!npe_rpg_runtime_read(w->battle_file,0x527ae2,&row,1)&&!npe_rpg_runtime_read(w->battle_file,0x527a78,&member,1));
    assert(row==s->menuRow&&member==s->menuMember);cache(w,s->cache);
}
static void probe_present(SDL_Renderer *r){
    SDL_RenderPresent(r);if(!active)return;assert(submits<current->frames);
    uint8_t expected[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    original_frame(submits,0,expected);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));unsigned d=0,sd=0;
    for(unsigned i=0;i<sizeof(expected);++i){unsigned v=expected[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s frame%u C%u SDL%u\n",current->name,submits,d,sd);
    char path[160];snprintf(path,sizeof(path),"tmp/walk-trigger/c-%s-%u.idx",current->name,submits);
    FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(expected),out)==sizeof(expected)&&!fclose(out));
    assert(!d&&!sd);++submits;++total;
}
typedef struct {SDL_Renderer *r;SDL_Texture *t;} WalkContext;
static int walk_commit(void *context,const uint8_t *pixels,unsigned wait){
    (void)wait;WalkContext *c=context;draw_pixels(c->r,c->t,pixels);return 0;
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];NpeWorld w;
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<8;++i){const TriggerLoad *c=&trigger_loads[i];NpeMap map;memset(&w,0,sizeof(w));
        assert(!npe_map(&f,c->map-1,&map)&&!npe_world_resident_write(&w,0x527490,c->before,100));
        assert(!npe_world_load_walk_triggers(&w,&map));cache(&w,c->after);
        fprintf(stderr,"original trigger load %s map%u:100 bytes match\n",c->entry,c->map);
    }
    for(unsigned i=0;i<10;++i){current=&trigger_services[i];configure(&f,&w);
        int result=npe_world_apply_move_event(&f,&w);
        assert(result==((current->before.event&255u)>0&&(current->before.event&255u)<=10));
        assert(w.game_mode==current->numeric_mode&&w.story_event_id==current->numeric_event&&w.map_init_pending==1);
        cache(&w,current->before.cache);assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
    }
    /* Warm constructors retain non-position bytes, then map-load the table. */
    assert(!npe_world_new_game(&f,&w));
    const char *save="tmp/walk-trigger/self-save.dat";
    assert(!npe_world_save(&f,&w,save));
    assert(!npe_world_resident_write(&w,0x527490,trigger_loads[0].before,100));
    assert(!npe_world_restore(&f,&w,save));cache(&w,trigger_loads[0].after);
    assert(!npe_world_resident_write(&w,0x527490,trigger_loads[6].before,100));
    assert(!npe_world_restart_game(&f,&w));cache(&w,trigger_loads[6].after);
    /* The existing own-save byte image already contains this alias. */
    assert(!npe_world_resident_write(&w,0x527490,trigger_loads[0].before,100));
    assert(!npe_world_save(&f,&w,save));NpeWorld loaded;
    assert(!npe_world_load(&f,&loaded,save));cache(&loaded,trigger_loads[0].before);
    /* Legacy NPESAV22 has no WTBL trailer; its old flag bytes must not
       overwrite the newly loaded walking table with zeros or old flags. */
    uint8_t *wire;size_t wire_size;assert(!read_file(save,&wire,&wire_size));
    assert(wire_size>40&&!memcmp(wire+wire_size-4,"WTBL",4));wire_size-=4;
    uint32_t payload=(uint32_t)(wire_size-36);uint64_t checksum=UINT64_C(14695981039346656037);
    for(size_t i=36;i<wire_size;++i){checksum^=wire[i];checksum*=UINT64_C(1099511628211);}
    for(unsigned i=0;i<4;++i)wire[24+i]=(uint8_t)(payload>>(8*i));
    for(unsigned i=0;i<8;++i)wire[28+i]=(uint8_t)(checksum>>(8*i));
    FILE *legacy=fopen(save,"wb");assert(legacy&&fwrite(wire,1,wire_size,legacy)==wire_size&&!fclose(legacy));free(wire);
    assert(!npe_world_load(&f,&loaded,save));uint8_t legacy_cache[100]={0};
    memcpy(legacy_cache,trigger_loads[0].after,9);cache(&loaded,legacy_cache);assert(!remove(save));
    NpeMap map;NpeMapMoveTrigger event;assert(!npe_map(&f,0,&map));
    uint8_t zero[100]={0};assert(!npe_world_resident_write(&w,0x527490,zero,100));w.story_progress=0;
    assert(npe_world_move_trigger(&w,&map,0,0,&event)==1&&event.mode==0&&event.event_id==0);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("walk triggers",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *r=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(r);
    SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(t);
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    for(unsigned i=0;i<16;++i){current=i<10?&trigger_services[i]:&trigger_walks[i-10];configure(&f,&w);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];original_frame(0,1,pixels);active=0;draw_pixels(r,t,pixels);
        save_map_edges(&f,r,pixels);retain_world_frame(&f,&w,r,pixels,1);submits=0;active=1;
        if(i>=10){WalkContext c={r,t};int result=npe_world_step_with_display(&f,&w,1,pixels,walk_commit,&c);assert(result>=0);}
        assert(advance_world_events(window,r,t,&f,&font,&w,i<10?20:1,0)>=0);
        if(submits<current->frames&&w.game_mode==6){unsigned levels[3]={0};assert(!draw_world(r,t,&f,&w,0,0,0,0,0,0,levels,0,&font));}
        assert(submits==current->frames);state(&w);assert(!npe_rpg_runtime_failures(&f));active=0;npe_rpg_runtime_close(&f);
    }
    assert(npe_game_fingerprint(&f)==fingerprint);npe_font_free(&font);SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(window);SDL_Quit();npe_free(&f);
    printf("Walk triggers:8 original100-byte loads,10 original numeric services,6 walking paths,%u original C/SDL frames,warm/cold lifetime and aliases passed\n",total);return 0;
}
