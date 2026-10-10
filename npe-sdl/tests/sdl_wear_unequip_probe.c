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
#include <limits.h>
#include "wear_unequip_native.h"
static NpeWorld *live;
static unsigned case_index,phase,submits,total,stage,active,setup_index;
static Uint32 fake_clock;
static const char *input_path;
static int probe_wait(SDL_Event *,int);
static int probe_popup_wait(SDL_Event *);
static void probe_present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/wear-unequip-");}
static Uint32 probe_ticks(void){return fake_clock;}
static void probe_delay(Uint32 ms){fake_clock+=ms;}
static int probe_poll(SDL_Event *e){if(active)return 0;memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetPrefPath probe_pref
#define SDL_GetTicks probe_ticks
#define SDL_Delay probe_delay
#define SDL_PollEvent probe_poll
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout probe_wait
#define SDL_WaitEvent probe_popup_wait
#define SDL_RenderPresent probe_present
#define npe_sound_select probe_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetPrefPath
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#undef npe_sound_select
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);}
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[160];
    if(before)snprintf(path,sizeof(path),"tmp/native-wear-unequip-%s-before.idx",menu_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-wear-unequip-%s-%u-commit-%u.idx",menu_cases[case_index].name,phase,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void configure(NpeWorld *w,const MenuState *s){
    assert(w->map_number==s->map&&w->music_enabled==s->enabled);
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->story_progress=(uint16_t)s->progress;
    w->view.screen_x=s->sx;w->view.screen_y=s->sy;w->view.offset_x=s->vx;w->view.offset_y=s->vy;
    w->x=s->sx+s->vx;w->y=s->sy+s->vy;w->direction=(uint8_t)s->direction;w->money=s->money;
    w->event_context_id=(uint16_t)s->context;w->map_music_slot=(uint8_t)s->slot;npe_world_seed(w,11);
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint8_t)s->npc[i].x;
        w->npc_override_y[i]=(uint8_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
    assert(!npe_world_resident_records_write(w,0x5269c6,s->roles,sizeof(s->roles)));
    memcpy(w->condition_words,s->inventory,sizeof(s->inventory));
    w->item_description_length=(uint16_t)s->descLength;w->shop_description_start=(uint16_t)s->descStart;w->shop_description_end=(uint16_t)s->descEnd;
    w->item_record_id=(uint8_t)s->itemLoaded;memcpy(w->item_record_bytes,s->itemBytes,142);
    w->skill_record_id=(uint8_t)s->skillLoaded;memcpy(w->skill_record_bytes,s->skillBytes,144);
    w->equipment_description_item=(uint8_t)s->gearSelected;

}
static void check_state(const MenuState *s){
    assert(live->game_mode==s->mode&&live->story_event_id==s->event&&live->map_number==s->map&&live->story_progress==s->progress&&
           live->view.screen_x==s->sx&&live->view.screen_y==s->sy&&live->view.offset_x==s->vx&&live->view.offset_y==s->vy&&
           live->direction==s->direction&&live->money==s->money&&live->x==s->sx+s->vx&&live->y==s->sy+s->vy&&
           live->music_enabled==s->enabled&&live->map_music_slot==s->slot);
    assert(live->event_context_id==s->context);
    if(s->mode==6&&(s->menu==9||s->menu==10))assert(live->property_page==s->menu&&live->property_member==s->member&&(s->menu!=10||live->equipment_view_slot+1==s->equipmentSlot));
    if(s->mode==6&&s->menu==10)assert(live->equipment_description_item==s->gearSelected&&
        live->item_description_length==s->descLength&&live->shop_description_start==s->descStart&&live->shop_description_end==s->descEnd);
    for(unsigned i=0;i<142;++i)if(live->item_record_bytes[i]!=s->itemBytes[i])fprintf(stderr,"Item cache mismatch %u C=%u native=%u\n",i,live->item_record_bytes[i],s->itemBytes[i]);
    assert(live->item_record_id==s->itemLoaded && !memcmp(live->item_record_bytes,s->itemBytes,142));
    assert(live->skill_record_id==s->skillLoaded && !memcmp(live->skill_record_bytes,s->skillBytes,144));
    uint8_t roles[444];assert(!npe_world_resident_records_read(live,0x5269c6,roles,sizeof(roles)));
    for(unsigned i=0;i<sizeof(roles);++i)if(roles[i]!=s->roles[i])fprintf(stderr,"Role mismatch %u:%u C=%u native=%u\n",i/148,i%148,roles[i],s->roles[i]);
    assert(!memcmp(roles,s->roles,sizeof(roles)));
    assert(!memcmp(live->condition_words,s->inventory,sizeof(s->inventory)));
    NpeMap map;assert(!npe_map((const NpeFile *)live->battle_file,live->map_number-1,&map));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){const uint8_t *n;unsigned x,y;assert(!npe_map_npc(&map,i,&n));npe_world_npc_position(live,n,i,&x,&y);
        unsigned direction=live->npc_override_direction[i]!=255?live->npc_override_direction[i]:n[6];
        assert((unsigned)n[0]*256+n[1]==s->npc[i].id&&x==s->npc[i].x&&y==s->npc[i].y&&direction==s->npc[i].direction);}
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active||stage==4)return;
    const MenuCase *c=&menu_cases[case_index];assert(phase<c->phases);
    if(submits>=c->phase[phase].commits){fprintf(stderr,"Extra C/SDL submit: %s phase%u mask%u native=%u\n",c->name,phase,c->phase[phase].mask,c->phase[phase].commits);assert(0);}
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"wear unequip %s phase%u commit%u C=%u SDL=%u\n",c->name,phase,submits,d,sd);
    if(d){FILE *out=fopen("tmp/wear-unequip-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++submits;++total;
    /* Inject only an observed pending periodic deadline, not a physical
       tick conversion. Native snapshots include four return+NPC cases. */
    if(phase==c->phases-1&&submits==1&&c->phase[phase].commits==2)fake_clock=100000;
}
static SDL_Keycode keycode(unsigned mask){switch(mask){case 64:return SDLK_F4;case 1:return SDLK_UP;case 2:return SDLK_DOWN;
    case 4:return SDLK_LEFT;case 8:return SDLK_RIGHT;case 16:case 128:return SDLK_RETURN;
    case 1024:case 2048:return SDLK_u;case 256:return SDLK_n;case 512:return SDLK_p;case 32:return SDLK_BACKSPACE;default:assert(0);return SDLK_UNKNOWN;}}
static void verify_roundtrip(const MenuState *s){
    /* The cloned-member case deliberately duplicates resident role IDs.
       It tests U dispatch/display, not the save format's valid ID domain. */
    if(!strcmp(menu_cases[case_index].name,"two-members"))return;
    const char *path="tmp/wear-unequip-roundtrip.sav";
    const NpeFile *running_file=(const NpeFile *)live->battle_file;
    assert(!npe_world_save(running_file,live,path));
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    NpeWorld *restored=malloc(sizeof(*restored));assert(restored);
    assert(!read_file(input_path,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_world_load(&file,restored,path));
    assert(!memcmp(restored->condition_words,s->inventory,sizeof(s->inventory)));
    for(unsigned m=0;m<menu_cases[case_index].party_count;++m){
        const uint8_t *r=s->roles+148*m;
#define RW(o) ((unsigned)r[o]*256u+r[(o)+1])
        assert(!memcmp(restored->equipped_items[m],r+18,6));
        assert(restored->hp[m]==RW(80)&&restored->max_hp[m]==RW(78)&&
               restored->qi[m]==RW(84)&&restored->max_qi[m]==RW(82)&&
               restored->attack[m]==RW(92)&&restored->defense[m]==RW(94));
        for(unsigned i=0;i<3;++i)assert(restored->other_stats[m][i]==RW(86+2*i));
        for(unsigned i=0;i<4;++i)assert(restored->status_guard[m][i]==RW(62+2*i));
#undef RW
    }
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];assert(!npe_render_world(&file,restored,pixels));
    npe_free(&file);free(restored);assert(!remove(path));
    fprintf(stderr,"Wear unequip %s: cold save/load and resumed map render passed\n",menu_cases[case_index].name);
}
static int prepare_event(SDL_Event *event){memset(event,0,sizeof(*event));
    if(!stage){assert(live->game_mode==7);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_RETURN;stage=1;return 1;}
    const MenuCase *c=&menu_cases[case_index];
    if(stage==4){
        assert(live->game_mode==1);verify_roundtrip(&c->phase[c->phases-1].after);
        active=0;stage=3;event->type=SDL_QUIT;return 1;
    }
    if(stage==1){
        static const unsigned off_keys[]={64,2,2,2,16,2,2,16,16};
        if(!c->before.enabled&&setup_index<sizeof(off_keys)/sizeof(*off_keys)){
            event->type=SDL_KEYDOWN;event->key.keysym.sym=keycode(off_keys[setup_index++]);return 1;}
        configure(live,&c->before);read_frame(1,0,last_pixel_frame);
        retain_world_frame((const NpeFile *)live->battle_file,live,last_pixel_renderer,last_pixel_frame,0);
        fake_clock=100;phase=submits=0;active=1;stage=2;
    }else{
        if(submits<c->phase[phase].commits){
            fprintf(stderr,"Missing original LCD: %s phase%u mask%u C=%u native=%u\n",c->name,phase,c->phase[phase].mask,submits,c->phase[phase].commits);
            assert(0);
        }
        assert(submits==c->phase[phase].commits);check_state(&c->phase[phase].after);
        if(!submits)fprintf(stderr,"Original ignored input: %s phase%u mask%u\n",c->name,phase,c->phase[phase].mask);
        submits=0;++phase;
    }
    /* Save from the map through an actual F4 return. This extra close frame
       is outside the original wear-page LCD measurements. */
    if(phase==c->phases){stage=4;event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_F4;return 1;}
    if(c->phase[phase].selected!=UINT_MAX)live->equipment_description_item=(uint8_t)c->phase[phase].selected;
    event->type=SDL_KEYDOWN;event->key.keysym.sym=keycode(c->phase[phase].mask);
    if(event->key.keysym.sym==SDLK_u){
        const char *variant=getenv("NPE_PROBE_WEAR_KEYS");
        event->key.keysym.mod=(Uint16)(variant&&!strcmp(variant,"caps")?KMOD_CAPS:
            variant&&!strcmp(variant,"shift")?KMOD_SHIFT:c->phase[phase].mask==2048?KMOD_SHIFT:KMOD_NONE);
    }
    return 1;
}
static int probe_wait(SDL_Event *e,int timeout){(void)timeout;return prepare_event(e);}
static int probe_popup_wait(SDL_Event *e){assert(live->game_mode==6);return prepare_event(e);}
int main(int argc,char **argv){NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(argc==2&&!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    input_path=argv[1];
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    char *paths[3];for(unsigned i=0;i<3;++i){paths[i]=game_save_path(&file,i);assert(paths[i]);remove(paths[i]);}
    NpeWorld saved;assert(!npe_world_new_game(&file,&saved)&&!npe_world_save(&file,&saved,paths[0]));
    for(case_index=0;case_index<sizeof(menu_cases)/sizeof(*menu_cases);++case_index){
        stage=active=phase=submits=setup_index=0;fake_clock=0;
        char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&stage==3);
        fprintf(stderr,"Wear unequip %s complete; full roles/inventory/item cache/NPC/settings matched\n",menu_cases[case_index].name);}
    for(unsigned i=0;i<3;++i){remove(paths[i]);free(paths[i]);}npe_free(&file);
    printf("Wear unequip return:%u original C/SDL commits, ignored keys, full roles/inventory/item cache/NPC/settings passed\n",total);return 0;
}
