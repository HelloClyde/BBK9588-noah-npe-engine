#include <assert.h>
#include <SDL.h>
#include "../text.h"
#ifdef NPE_TEXT_BITMAP_GUARD
#include "text_bitmap_guard_native.h"
#define ITEM_REFERENCE_PREFIX "tmp/native-text-bitmap-guard-"
#define ITEM_ACTUAL_PATH "tmp/text-bitmap-guard-actual.idx"
#elif defined(NPE_CHARACTER_PORTRAIT)
#include "portrait_item_native.h"
#define ITEM_REFERENCE_PREFIX "tmp/native-portrait-item-"
#define ITEM_ACTUAL_PATH "tmp/portrait-item-actual.idx"
#else
#include "map_item_record_native.h"
#define ITEM_REFERENCE_PREFIX "tmp/native-map-item-record-"
#define ITEM_ACTUAL_PATH "tmp/map-item-record-actual.idx"
#endif
#ifdef main
#undef main
#endif
static unsigned case_index,commits,phase_index;
static int active;
static NpeWorld *live;
static int input(SDL_Event *);
static void present(SDL_Renderer *);
#define SDL_WaitEvent input
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#include "battle_input_keys.h"
#include "map_menu_ignored_input.h"

static void state(const MapItemState *s){
    const MapItemCase *c=&map_item_cases[case_index];
    assert(live->game_mode==s->mode && live->shop_selected_item==s->selected);
    assert(live->shop_row==s->row && live->inventory_column==s->column);
    assert(live->item_description_length==s->length && live->shop_description_start==s->start && live->shop_description_end==s->end);
    assert(live->item_record_id==s->loaded && !memcmp(live->item_record_bytes,s->bytes,142));
    const unsigned p[]={live->hp[0],live->max_hp[0],live->qi[0],live->max_qi[0],live->attack[0],live->defense[0],live->level[0],live->experience[0],live->experience_to_next[0],
                        live->other_stats[0][0],live->other_stats[0][1],live->other_stats[0][2],
                        live->status_guard[0][0],live->status_guard[0][1],live->status_guard[0][2],live->status_guard[0][3]};
    assert(!memcmp(p,s->party,sizeof(p)));
#ifdef NPE_CHARACTER_PORTRAIT
    for(unsigned i=0;i<3;++i)assert(live->party_characters[i].pose_ids[7]==s->portraits[i]);
    for(unsigned i=1;i<c->members;++i){
        const unsigned member[]={live->hp[i],live->max_hp[i],live->qi[i],live->max_qi[i],live->attack[i],live->defense[i],live->level[i],live->experience[i],live->experience_to_next[i],
            live->other_stats[i][0],live->other_stats[i][1],live->other_stats[i][2],live->status_guard[i][0],live->status_guard[i][1],live->status_guard[i][2],live->status_guard[i][3]};
        assert(!memcmp(member,s->party+i*16,sizeof(member)));
    }
#endif
    for(unsigned i=0;i<c->stock_count;++i)assert(live->condition_words[c->stock[i][0]]==s->stock[i]);
}
static void present(SDL_Renderer *r){
    SDL_RenderPresent(r);if(!active)return;map_menu_ignored_present();
    const MapItemCase *c=&map_item_cases[case_index];assert(commits<c->commits);
    const MapItemFrame *frame=&c->frame[commits];char path[160];
    snprintf(path,sizeof(path),ITEM_REFERENCE_PREFIX "%s-%u.idx",c->name,frame->index);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    FILE *f=fopen(path,"rb");assert(f && fread(native,1,sizeof(native),f)==sizeof(native) && !fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        d+=last_pixel_frame[i]!=v;sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"%s commit%u C=%u SDL=%u\n",c->name,commits,d,sd);
    if(d){f=fopen(ITEM_ACTUAL_PATH,"wb");assert(f && fwrite(last_pixel_frame,1,sizeof(native),f)==sizeof(native) && !fclose(f));}
    assert(!d && !sd);state(&frame->state);++commits;
}
static int input(SDL_Event *e){
    const MapItemCase *c=&map_item_cases[case_index];
    if(phase_index){assert(commits==c->phase[phase_index-1].end);state(&c->phase[phase_index-1].after);}
    if(map_menu_ignored_input(e,live))return 1;
    assert(phase_index<c->phases);unsigned key=c->phase[phase_index++].key;
    probe_battle_input(e,key,SDLK_RETURN);
    return 1;
}
int main(int argc,char **argv){
    assert(argc==2);assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1));assert(!SDL_Init(SDL_INIT_VIDEO));
    uint8_t *bytes;size_t size;char error[128];NpeFile file;NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size) && !npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin") && !npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    SDL_Window *window=SDL_CreateWindow("map item record",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    for(case_index=0;case_index<sizeof(map_item_cases)/sizeof(*map_item_cases);++case_index){
        const MapItemCase *c=&map_item_cases[case_index];NpeWorld world;assert(!npe_world_new_game(&file,&world));live=&world;
        memset(world.condition_words,0,sizeof(world.condition_words));
        for(unsigned i=0;i<c->stock_count;++i)world.condition_words[c->stock[i][0]]=(uint16_t)c->stock[i][1];
        world.party_active[0]=1;world.party_active[1]=world.party_active[2]=0;
#ifdef NPE_CHARACTER_PORTRAIT
        for(unsigned i=0;i<3;++i){
            world.party_active[i]=(uint8_t)(i<c->members);
            world.party_characters[i].pose_ids[7]=(uint16_t)c->seed.portraits[i];
        }
        for(unsigned i=1;i<c->members;++i){const unsigned *p=c->seed.party+i*16;
            world.hp[i]=(uint16_t)p[0];world.max_hp[i]=(uint16_t)p[1];world.qi[i]=(uint16_t)p[2];world.max_qi[i]=(uint16_t)p[3];
            world.attack[i]=(uint16_t)p[4];world.defense[i]=(uint16_t)p[5];world.level[i]=(uint8_t)p[6];world.experience[i]=(uint16_t)p[7];world.experience_to_next[i]=(uint16_t)p[8];
            for(unsigned j=0;j<3;++j)world.other_stats[i][j]=(uint16_t)p[9+j];
            for(unsigned j=0;j<4;++j)world.status_guard[i][j]=(uint16_t)p[12+j];
        }
#endif
        world.hp[0]=50;world.max_hp[0]=200;world.qi[0]=100;world.max_qi[0]=120;
        world.attack[0]=(uint16_t)c->seed.party[4];world.defense[0]=(uint16_t)c->seed.party[5];
        world.level[0]=(uint8_t)c->seed.party[6];world.experience[0]=(uint16_t)c->seed.party[7];
        world.experience_to_next[0]=(uint16_t)c->seed.party[8];
        for(unsigned i=0;i<3;++i)world.other_stats[0][i]=(uint16_t)c->seed.party[9+i];
        for(unsigned i=0;i<4;++i)world.status_guard[0][i]=(uint16_t)c->seed.party[12+i];
        world.item_record_id=(uint8_t)c->seed.loaded;memcpy(world.item_record_bytes,c->seed.bytes,142);
        world.item_description_length=(uint16_t)c->seed.length;world.shop_description_start=(uint16_t)c->seed.start;world.shop_description_end=(uint16_t)c->seed.end;
        world.game_mode=4;assert(!npe_world_resources_open(&file,&world));
#ifdef NPE_TEXT_BITMAP_GUARD
        /* Original service reads resident group words and directory bytes;
           parsed image counts/bases must not prevent the page call. */
        unsigned saved_count=file.indexed_bitmap_count;
        uint16_t saved_groups[4];
        file.indexed_bitmap_count=1;
        for(unsigned g=5;g<=8;++g){saved_groups[g-5]=file.vectors[g];file.vectors[g]^=100;}
#endif
        map_menu_ignored_reset(&file);active=1;commits=phase_index=0;SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        assert(show_use_item(window,renderer,texture,&file,&font,&world,0)==0);
#ifdef NPE_TEXT_BITMAP_GUARD
        assert(!npe_rpg_runtime_failures(&file));
#endif
        active=0;assert(phase_index==c->phases && commits==c->commits);state(&c->phase[c->phases-1].after);
        npe_rpg_runtime_close(&file);
#ifdef NPE_TEXT_BITMAP_GUARD
        file.indexed_bitmap_count=saved_count;
        for(unsigned g=5;g<=8;++g)file.vectors[g]=saved_groups[g-5];
#endif
    }
    map_menu_ignored_report();
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_font_free(&font);npe_free(&file);SDL_Quit();
    printf("Original map item UI: %zu actual SDL paths / cache, portraits and party fields matched\n",sizeof(map_item_cases)/sizeof(*map_item_cases));return 0;
}
