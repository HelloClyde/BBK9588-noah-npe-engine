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
typedef struct {unsigned id,x,y,direction;} SkillNpc;
typedef struct {unsigned id,active;uint8_t skills[18];} SkillActor;
typedef struct {unsigned mode,event,context,map,progress,init,sx,sy,vx,vy,direction,enabled,slot;SkillNpc npc[20];SkillActor actors[3];} SkillState;
typedef struct {unsigned next,skill,character,mode;} SkillCache;
typedef struct {const char *name;unsigned real,commits;SkillState before,after;SkillCache cache[2];uint8_t native_name[12];} SkillCase;
#include "skill_service_native.h"

typedef struct {const char *name;unsigned map,restart;SkillCache boot[2],before[2],after[2];} SkillCacheCase;
#include "skill_cache_native.h"
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
    if(before)snprintf(path,sizeof(path),"tmp/native-skill-service-%s-before.idx",skill_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-skill-service-%s-%u.idx",skill_cases[case_index].name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const SkillCase *c=&skill_cases[case_index];assert(submits<c->commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"skill %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);
    if(d){char path[180];snprintf(path,sizeof(path),"tmp/skill-service-failed-%s-%u.idx",c->name,submits);
        FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,NPE_WIDTH*NPE_HEIGHT,out)==NPE_WIDTH*NPE_HEIGHT&&!fclose(out));}
    if(!quit_requested)assert(!d&&!sd);
    if(c->commits>=3&&submits<2){assert(live->game_mode==9&&live->story_event_id==c->before.event);
 for(unsigned j=0;j<3;++j)assert(!memcmp(live->skill_slots[j],c->after.actors[j].skills,18));}
    ++submits;++total;
}
static void configure(const NpeFile *file,NpeWorld *w,const NpeWorld *baseline,const SkillCase *c){
 const SkillState *s=&c->before;*w=*baseline;
 if(s->map!=1){NpeWorld saved;const char *path="tmp/skill-config.npes";assert(!npe_world_init(file,&saved,s->map,1,1)&&!npe_world_save(file,&saved,path)&&!npe_world_restore(file,w,path)&&!remove(path));}
 w->game_mode=9;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;
 w->story_progress=(uint16_t)s->progress;w->map_init_pending=(uint8_t)s->init;
 w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
 w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=s->direction;w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;
 for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
 for(unsigned i=0;i<3;++i){w->party_ids[i]=(uint8_t)s->actors[i].id;w->party_active[i]=(uint8_t)s->actors[i].active;memcpy(w->skill_slots[i],s->actors[i].skills,18);}
 assert(!npe_world_resources_open((NpeFile *)file,w));
 for(unsigned i=0;i<2;++i){const SkillCache *k=&c->cache[i];
  uint8_t bytes[]={(uint8_t)(k->next>>8),(uint8_t)k->next,(uint8_t)k->skill,(uint8_t)k->character,(uint8_t)k->mode,(uint8_t)(0x81+i)};
  assert(!npe_rpg_runtime_write(file,0x52790a+i*6,bytes,6));}
 npe_world_seed(w,11);
}
static void seed_cache(NpeWorld *w,const SkillCache c[2]){
    for(unsigned i=0;i<2;++i)w->skill_events[i]=(NpeMapSkillEvent){.skill_id=(uint8_t)c[i].skill,.character_id=(uint8_t)c[i].character,.next_id=(uint16_t)c[i].next,.next_mode=(uint8_t)c[i].mode};
}
static void check_cache(const NpeWorld *w,const SkillCache c[2]){
    for(unsigned i=0;i<2;++i){NpeMapSkillEvent e;assert(npe_world_skill_event(w,i+1,&e)==1&&!e.raw&&!e.id);assert(e.skill_id==c[i].skill&&e.character_id==c[i].character&&e.next_id==c[i].next&&e.next_mode==c[i].mode);}
}
static void check_state(const NpeFile *file,const NpeWorld *w,const SkillState *s){
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->event_context_id==s->context);
    assert(w->map_number==s->map&&w->story_progress==s->progress&&w->map_init_pending==s->init);
    assert(w->view.screen_x==s->sx&&w->view.screen_y==s->sy&&w->view.offset_x==s->vx&&w->view.offset_y==s->vy);
    assert(w->x==s->sx+s->vx&&w->y==s->sy+s->vy&&w->direction==s->direction);
    assert(w->music_enabled==s->enabled&&w->map_music_slot==s->slot&&w->random_state==11);
    for(unsigned i=0;i<3;++i)assert(w->party_ids[i]==s->actors[i].id&&w->party_active[i]==s->actors[i].active&&!memcmp(w->skill_slots[i],s->actors[i].skills,18));
    NpeMap map;assert(!npe_map(file,s->map-1,&map));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){const uint8_t *npc;unsigned x,y;
        assert(!npe_map_npc(&map,i,&npc));npe_world_npc_position(w,npc,i,&x,&y);
        unsigned direction=w->npc_override_direction[i]!=255?w->npc_override_direction[i]:npc[6];
        assert(x==s->npc[i].x&&y==s->npc[i].y&&direction==s->npc[i].direction);}
}
static void check_cache_lifetime(const NpeFile *file,const NpeWorld *baseline){
    for(unsigned i=0;i<8;++i){const SkillCacheCase *c=&skill_cache_cases[i];NpeWorld w=*baseline,saved,cold;
        check_cache(baseline,c->boot);seed_cache(&w,c->before);
        if(c->restart){assert(!npe_world_restart_game(file,&w));check_cache(&w,c->after);continue;}
        assert(!npe_world_init(file,&saved,c->map,1,1));const char *path="tmp/skill-cache.npes";
        assert(!npe_world_save(file,&saved,path));assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);
        assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);
        assert(!npe_world_load(file,&cold,path));NpeMap map;assert(!npe_map(file,c->map-1,&map));
        for(unsigned j=0;j<2;++j){NpeMapSkillEvent e;int found=npe_map_skill_event(&map,j+1,&e);assert(found>=0);assert(cold.skill_events[j].skill_id==(found?e.skill_id:0));}
        NpeWorld before=w;FILE *out=fopen(path,"wb");assert(out&&fwrite("NPESAV17",1,8,out)==8&&!fclose(out));
        assert(npe_world_restore(file,&w,path)<0&&!memcmp(&before,&w,sizeof(w))&&!remove(path));
        uint8_t tail[710]={0};tail[652+15*3+2]=3;NpeMap bad=map;bad.tail=tail;bad.tail_size=sizeof(tail);
        assert(npe_world_load_skill_events(&w,&bad)<0&&!memcmp(&before,&w,sizeof(w)));
    }
    NpeWorld a=*baseline,b=a;seed_cache(&b,skill_cache_cases[0].before);
    assert(!npe_world_save(file,&a,"tmp/skill-wire-a.npes")&&!npe_world_save(file,&b,"tmp/skill-wire-b.npes"));
    FILE *fa=fopen("tmp/skill-wire-a.npes","rb"),*fb=fopen("tmp/skill-wire-b.npes","rb");assert(fa&&fb);
    int ca,cb;do{ca=fgetc(fa);cb=fgetc(fb);assert(ca==cb);}while(ca!=EOF);
    assert(!fclose(fa)&&!fclose(fb)&&!remove("tmp/skill-wire-a.npes")&&!remove("tmp/skill-wire-b.npes"));
}
int main(int argc,char **argv){NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld baseline,w;
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("skill service",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!npe_world_new_game(&file,&baseline));live=&w;check_cache_lifetime(&file,&baseline);
    for(case_index=0;case_index<21;++case_index){const SkillCase *c=&skill_cases[case_index];configure(&file,&w,&baseline,c);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);active=0;draw_pixels(renderer,texture,pixels);
        save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&w,renderer,pixels,1);
        submits=0;active=1;fake_clock=case_index==1?UINT32_MAX-1000:0;Uint32 start=fake_clock;
        assert(advance_world_events(window,renderer,texture,&file,&font,&w,9,0)>=0);
        assert(fake_clock==start&&submits==c->commits);
        check_state(&file,&w,&c->after);check_cache(&w,c->cache);
        unsigned id=c->before.event&255u;
        if(id>0&&id<=2){unsigned skill=c->cache[id-1].skill;if(skill){NpeSkill decoded;assert(!npe_skill(&file,skill-1,&decoded)&&decoded.name_size<12&&!memcmp(decoded.name,c->native_name,decoded.name_size)&&!c->native_name[decoded.name_size]);}else for(unsigned j=0;j<12;++j)assert(!c->native_name[j]);}
        if(c->real){NpeMap map;NpeMapSkillEvent e;assert(!npe_map(&file,c->before.map-1,&map)&&npe_map_skill_event(&map,id,&e)==1);const SkillCache *k=&c->cache[id-1];assert(e.skill_id==k->skill&&e.character_id==k->character&&e.next_mode==k->mode&&e.next_id==k->next);}
        active=0;const char *path="tmp/skill-slots.npes";assert(!npe_world_save(&file,&w,path));NpeWorld loaded;
        if(!strcmp(c->name,"duplicate-id")){
            /* Deliberate duplicate IDs exercise the original service, but
               remain invalid in the existing host save format. */
            assert(npe_world_load(&file,&loaded,path)<0);
        }else assert(!npe_world_load(&file,&loaded,path)&&!memcmp(loaded.skill_slots,w.skill_slots,sizeof(w.skill_slots)));
        assert(!remove(path));
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
    }
    case_index=0;configure(&file,&w,&baseline,&skill_cases[0]);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);
    active=0;draw_pixels(renderer,texture,pixels);submits=0;active=quit_requested=1;
    assert(show_skill_event(renderer,texture,&file,&font,&w)==-2&&submits==1&&w.game_mode==9&&w.story_event_id==1&&w.skill_slots[0][0]==2&&w.map_init_pending==1);
    active=0;npe_rpg_runtime_close(&file);
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    printf("Skill service:21 original paths,%u C/SDL commits,8 original cache loads,fixed2/low-byte,character fallback,18-byte insertion,NPE notice,warm lifetime,save bytes and quit passed\n",total-1);return 0;
}
