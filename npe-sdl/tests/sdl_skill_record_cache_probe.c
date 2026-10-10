#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../text.h"
#include "skill_record_native.h"
#ifdef main
#undef main
#endif
static unsigned case_index,submits,total;
static int active;
static NpeWorld *live;
static int poll(SDL_Event *e){memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;}
static void present(SDL_Renderer *);
#define SDL_PollEvent poll
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_RenderPresent
static void cache(const NpeWorld *w,const SkillRecordState *s){
    assert(w->skill_record_id==s->loaded && !memcmp(w->skill_record_bytes,s->bytes,144));
}
static void frame(int before,unsigned index,uint8_t *pixels){char path[180];
    if(before)snprintf(path,sizeof(path),"tmp/native-skill-record-cache-%s-before.idx",skill_record_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-skill-record-cache-%s-%u.idx",skill_record_cases[case_index].name,index);
    FILE *f=fopen(path,"rb");assert(f && fread(pixels,1,28800,f)==28800 && !fclose(f));
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const SkillRecordCase *c=&skill_record_cases[case_index];assert(submits<c->commits);
    uint8_t native[28800];uint32_t rgba[28800];frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<28800;++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Skill record %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);
    if(d){FILE *f=fopen("tmp/skill-record-cache-failed.idx","wb");assert(f && fwrite(last_pixel_frame,1,28800,f)==28800 && !fclose(f));}
    assert(!d && !sd);cache(live,&c->after);++submits;++total;
}
static void seed(NpeWorld *w,const SkillRecordState *s){
    uint8_t loaded=(uint8_t)s->loaded;
    assert(!npe_world_resident_write(w,0x527a74,&loaded,1));
    assert(!npe_world_resident_write(w,0x52730e,s->bytes,144));
}
static void lifetime(const NpeFile *file,const NpeWorld *base){
    NpeWorld w,saved,cold;NpeSkill skill;
    for(unsigned i=0;i<5;++i){const SkillRecordCase *c=&skill_record_lifetimes[i];w=*base;seed(&w,&c->before);
        if(!strcmp(c->name,"new") || !strcmp(c->name,"empty"))assert(!npe_world_restart_game(file,&w));
        else if(!strcmp(c->name,"teleport")){
            w.game_mode=16;w.story_event_id=1;
            w.teleport_events[0]=(NpeMapTeleportEvent){.destination_map=2,.x=1,.y=1,.next_mode=1,.next_id=789};
            assert(npe_world_apply_teleport_event(file,&w)==1);
        }else{
            saved=*base;assert(!npe_world_save(file,&saved,"tmp/skill-record-life.npes"));
            assert(!npe_world_restore(file,&w,"tmp/skill-record-life.npes"));
            cache(&w,&c->after);assert(!npe_world_restore(file,&w,"tmp/skill-record-life.npes"));
            assert(!npe_world_load(file,&cold,"tmp/skill-record-life.npes") && !cold.skill_record_id);
            for(unsigned j=0;j<144;++j)assert(!cold.skill_record_bytes[j]);
            assert(!remove("tmp/skill-record-life.npes"));
        }
        cache(&w,&c->after);
    }
    w=*base;seed(&w,&skill_record_cases[1].before);
    assert(!npe_world_skill(NULL,&w,258,&skill) && skill.name_size==8 && skill.qi_cost==0xa5a5);
    cache(&w,&skill_record_cases[1].before);
    assert(!npe_world_skill(NULL,&w,256,&skill) && !skill.name_size && !skill.hp_effect);
    cache(&w,&skill_record_cases[3].after);
    /* The separate item cache must not be changed by a skill load. */
    w=*base;w.item_record_id=7;memset(w.item_record_bytes,0x5a,142);
    assert(!npe_world_skill(file,&w,2,&skill) && w.item_record_id==7);
    for(unsigned j=0;j<142;++j)assert(w.item_record_bytes[j]==0x5a);
    assert(!npe_world_save(file,base,"tmp/skill-record-wire.npes"));uint8_t *a,*b;size_t na,nb;
    assert(!read_file("tmp/skill-record-wire.npes",&a,&na));w=*base;
    assert(!npe_world_skill(file,&w,41,&skill) && !npe_world_save(file,&w,"tmp/skill-record-wire.npes"));
    assert(!read_file("tmp/skill-record-wire.npes",&b,&nb) && na==nb && !memcmp(a,b,na));free(a);free(b);
    NpeWorld before=w;FILE *bad=fopen("tmp/skill-record-wire.npes","wb");assert(bad && fwrite("NPESAV19",1,8,bad)==8 && !fclose(bad));
    assert(npe_world_restore(file,&w,"tmp/skill-record-wire.npes")<0 && !memcmp(&before,&w,sizeof(w)));
    assert(!remove("tmp/skill-record-wire.npes"));
}
int main(int argc,char **argv){assert(argc==2);uint8_t *bytes;size_t size;char error[128];NpeFile file;NpeFont font={0};NpeWorld base,w;
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)) && !npe_world_new_game(&file,&base));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *win=SDL_CreateWindow("skill record",0,0,240,120,0);assert(win);
    SDL_Renderer *r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(r);
    SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(t);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    lifetime(&file,&base);live=&w;
    for(case_index=0;case_index<6;++case_index){const SkillRecordCase *c=&skill_record_cases[case_index];const SkillRecordState *s=&c->before;
        w=base;seed(&w,s);w.game_mode=(uint8_t)s->mode;w.story_event_id=(uint16_t)s->event;w.story_progress=(uint16_t)s->progress;
        w.map_init_pending=(uint8_t)s->init;w.view=(NpeViewport){s->sx,s->sy,s->vx,s->vy};w.x=s->sx+s->vx;w.y=s->sy+s->vy;w.direction=s->direction;
        w.music_enabled=0;w.lcd_background=0;npe_world_seed(&w,11);memcpy(w.skill_slots[0],s->skills,18);
        w.skill_events[0]=(NpeMapSkillEvent){.skill_id=(uint8_t)c->id,.character_id=1,.next_mode=1,.next_id=789};
        for(unsigned i=0;i<20;++i){w.npc_override_x[i]=(uint16_t)s->npc[i].x;w.npc_override_y[i]=(uint16_t)s->npc[i].y;w.npc_override_direction[i]=(uint8_t)s->npc[i].direction;}
        uint8_t pixels[28800];frame(1,0,pixels);active=0;draw_pixels(r,t,pixels);save_map_edges(&file,r,pixels);retain_world_frame(&file,&w,r,pixels,1);
        submits=0;active=1;assert(advance_world_events(win,r,t,&file,&font,&w,9,0)>=0);active=0;
        assert(submits==c->commits);cache(&w,&c->after);
        assert(w.game_mode==c->after.mode && w.story_event_id==c->after.event && w.map_init_pending==c->after.init && !memcmp(w.skill_slots[0],c->after.skills,18));
    }
    npe_font_free(&font);SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);SDL_Quit();npe_free(&file);
    printf("Skill record cache:6 native SDL paths/%u LCD commits,144 bytes,5 original lifetimes,low-byte/clear/tail/padding,item isolation,V19 isolation and failed restore passed\n",total);return 0;
}
