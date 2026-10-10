#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../text.h"
#ifdef NPE_MAGIC_DESCRIPTION_RETAIN
#include "magic_description_retain_native.h"
#define SKILL_REFERENCE_PREFIX "tmp/native-magic-description-retain-"
#define SKILL_ACTUAL_PATH "tmp/magic-description-retain-actual.idx"
#elif defined(NPE_CHARACTER_PORTRAIT)
#include "portrait_skill_native.h"
#define SKILL_REFERENCE_PREFIX "tmp/native-portrait-skill-"
#define SKILL_ACTUAL_PATH "tmp/portrait-skill-actual.idx"
#elif defined(NPE_MAP_DEFAULT_SKILL)
#include "map_default_skill_native.h"
#define SKILL_REFERENCE_PREFIX "tmp/native-map-default-skill-"
#define SKILL_ACTUAL_PATH "tmp/map-default-skill-actual.idx"
static void check_current_default_records(const char *);
#else
#include "map_skill_record_native.h"
#define SKILL_REFERENCE_PREFIX "tmp/native-map-skill-record-"
#define SKILL_ACTUAL_PATH "tmp/map-skill-record-actual.idx"
#endif
#ifdef main
#undef main
#endif
static unsigned case_index,commits,phase_index,total;
static int active;
static NpeWorld *live;
static Uint32 fake_clock;
static Uint32 ticks(void){fake_clock+=2001;return fake_clock;}
static int poll(SDL_Event *e){(void)e;return 0;}
static int input(SDL_Event *);
static void present(SDL_Renderer *);
#define SDL_GetTicks ticks
#define SDL_PollEvent poll
#define SDL_WaitEvent input
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_PollEvent
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#include "battle_input_keys.h"
#include "map_menu_ignored_input.h"
static void state(const MapSkillState *s){
#ifdef NPE_CHARACTER_PORTRAIT
    for(unsigned i=0;i<3;++i)assert(live->party_characters[i].pose_ids[7]==s->portraits[i]);
#endif
    assert(live->game_mode==s->mode && live->property_page==s->page && live->property_member==s->member);
    assert(live->skill_record_id==s->loaded && !memcmp(live->skill_record_bytes,s->bytes,144));
    assert(live->item_description_length==s->length && live->shop_description_start==s->start && live->shop_description_end==s->end);
    for(unsigned i=0;i<map_skill_cases[case_index].members;++i){
        const unsigned p[]={live->hp[i],live->max_hp[i],live->qi[i],live->max_qi[i],live->attack[i],live->defense[i],live->level[i],live->experience[i],live->experience_to_next[i],
                            live->other_stats[i][0],live->other_stats[i][1],live->other_stats[i][2],
                            live->status_guard[i][0],live->status_guard[i][1],live->status_guard[i][2],live->status_guard[i][3]};
        assert(!memcmp(p,s->party+i*16,sizeof(p)));
    }
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;map_menu_ignored_present();
    const MapSkillCase *c=&map_skill_cases[case_index];assert(commits<c->commits);
    const MapSkillFrame *f=&c->frame[commits];char path[180];
    if(f->index==65535)snprintf(path,sizeof(path),SKILL_REFERENCE_PREFIX "%s-before.idx",c->name);
    else snprintf(path,sizeof(path),SKILL_REFERENCE_PREFIX "%s-%u.idx",c->name,f->index);
    uint8_t native[28800];uint32_t rgba[28800];FILE *in=fopen(path,"rb");assert(in && fread(native,1,sizeof(native),in)==sizeof(native) && !fclose(in));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<28800;++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Map skill %s commit%u C=%u SDL=%u\n",c->name,commits,d,sd);
    if(d){FILE *o=fopen(SKILL_ACTUAL_PATH,"wb");assert(o && fwrite(last_pixel_frame,1,sizeof(native),o)==sizeof(native) && !fclose(o));}
    assert(!d && !sd);state(&f->state);++commits;++total;
}
static int input(SDL_Event *e){
    const MapSkillCase *c=&map_skill_cases[case_index];
    if(phase_index){assert(commits==c->phase[phase_index-1].end);state(&c->phase[phase_index-1].after);}
    if(map_menu_ignored_input(e,live))return 1;
    assert(phase_index<c->phases);
#ifdef NPE_MAGIC_DESCRIPTION_RETAIN
    const MapSkillPhase *phase=&c->phase[phase_index];
    if(phase->patch){
        live->item_description_length=(uint16_t)phase->patch_length;
        live->shop_description_start=(uint16_t)phase->patch_start;
        live->shop_description_end=(uint16_t)phase->patch_end;
    }
#endif
    unsigned key=c->phase[phase_index++].key;
    probe_battle_input(e,key,SDLK_RETURN);
    return 1;
}
int main(int argc,char **argv){
#ifdef NPE_MAP_DEFAULT_SKILL
    assert(argc==2||argc==3);
#else
    assert(argc==2);
#endif
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1) && !SDL_Init(SDL_INIT_VIDEO));
    uint8_t *bytes;size_t size;char error[128];NpeFile file;NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *win=SDL_CreateWindow("map skill cache",0,0,240,120,0);assert(win);
    SDL_Renderer *r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(r);
    SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(t);
    for(case_index=0;case_index<sizeof(map_skill_cases)/sizeof(*map_skill_cases);++case_index){
        const MapSkillCase *c=&map_skill_cases[case_index];const MapSkillState *s=&c->before;NpeWorld w;assert(!npe_world_new_game(&file,&w));live=&w;
        for(unsigned i=0;i<3;++i)w.party_active[i]=(uint8_t)(i<c->members);
#ifdef NPE_MAP_DEFAULT_SKILL
        /* The two-target original diagnostic clones the first148-byte
           character record before setting the second target's HP to zero. */
        if(c->members==2){
            memcpy(w.party_names[1],w.party_names[0],sizeof(w.party_names[0]));
            w.party_characters[1]=w.party_characters[0];
            memcpy(w.party_record_residue[1],w.party_record_residue[0],sizeof(w.party_record_residue[0]));
        }
#endif
#ifdef NPE_CHARACTER_PORTRAIT
        for(unsigned i=0;i<3;++i)w.party_characters[i].pose_ids[7]=(uint16_t)s->portraits[i];
#endif
        w.game_mode=6;w.property_page=(uint8_t)s->page;w.property_member=(uint8_t)s->member;
        for(unsigned i=0;i<c->members;++i){
            const unsigned *p=s->party+i*16;
            w.hp[i]=(uint16_t)p[0];w.max_hp[i]=(uint16_t)p[1];w.qi[i]=(uint16_t)p[2];w.max_qi[i]=(uint16_t)p[3];
            w.attack[i]=(uint16_t)p[4];w.defense[i]=(uint16_t)p[5];w.level[i]=(uint8_t)p[6];w.experience[i]=(uint16_t)p[7];w.experience_to_next[i]=(uint16_t)p[8];
            for(unsigned j=0;j<3;++j)w.other_stats[i][j]=(uint16_t)p[9+j];
            for(unsigned j=0;j<4;++j)w.status_guard[i][j]=(uint16_t)p[12+j];
        }
        w.skill_record_id=(uint8_t)s->loaded;memcpy(w.skill_record_bytes,s->bytes,144);memcpy(w.skill_slots[0],c->skills,18);
        w.item_description_length=(uint16_t)s->length;w.shop_description_start=(uint16_t)s->start;w.shop_description_end=(uint16_t)s->end;
        /* The warm title-load reference has a cleared LCD background. */
        w.music_enabled=0;w.lcd_background=0;assert(!npe_world_resources_open(&file,&w));
        /* Construct the caller's retained map/menu with production C code. */
        system_menu_background_valid=system_menu_frame_valid=world_map_frame_valid=0;
        uint8_t base[NPE_WIDTH*NPE_HEIGHT];unsigned levels[3]={0};
        assert(!npe_render_world_load(&file,&w,base));
        draw_pixels(r,t,base);retain_world_frame(&file,&w,r,base,0);
        assert(!draw_world(r,t,&file,&w,1,0,0,0,0,0,levels,0,&font));
        map_menu_ignored_reset(&file);active=1;commits=phase_index=0;SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        show_magic_list(win,r,t,&file,&font,&w);active=0;assert(phase_index==c->phases && commits==c->commits);state(&c->phase[c->phases-1].after);
        npe_rpg_runtime_close(&file);
    }
#ifdef NPE_MAP_DEFAULT_SKILL
    if(argc==3)check_current_default_records(argv[2]);
#endif
    map_menu_ignored_report();
    npe_font_free(&font);SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);SDL_Quit();npe_free(&file);
    printf("Map skill record:%zu native modal paths/%u actual SDL LCD commits,144 bytes and party fields matched\n",sizeof(map_skill_cases)/sizeof(*map_skill_cases),total);return 0;
}
