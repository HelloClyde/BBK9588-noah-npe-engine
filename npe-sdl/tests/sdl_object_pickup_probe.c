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
typedef struct {const char *name;unsigned before,done_before,direction,after,done_after,acted;} PickupCase;
#include "object_pickup_native.h"
static unsigned case_index,phase,inputs,submits,total,finished,dismissed;
static int closing;
static int timeout_case;
static Uint32 fake_clock;
static NpeWorld *live;
static const NpeFile *fixture_file;
static uint8_t base[28800],overlay[28800];
static void present(SDL_Renderer *);
static int wait_input(SDL_Event *,int),poll_input(SDL_Event *);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 ms){fake_clock+=ms;}
static SDL_Window *create_window(const char *name,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;return SDL_CreateWindow(name,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_RenderPresent present
#define SDL_WaitEventTimeout wait_input
#define SDL_PollEvent poll_input
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_CreateWindow create_window
#define npe_sound_select select_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
#undef SDL_WaitEventTimeout
#undef SDL_PollEvent
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_CreateWindow
#undef npe_sound_select

static void check_state(unsigned quantity,unsigned done){
    assert(live&&live->condition_words[69]==quantity&&live->object_done[5]==done);
    assert(live->game_mode==1&&live->map_init_pending==0&&live->random_state==11);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){const PickupCase *c=&pickup_cases[case_index];live=w;
        NpeWorld prior=*w;
        assert(!npe_world_init(fixture_file,w,80,27,20));
        w->battle_animation_observer=prior.battle_animation_observer;w->battle_animation_context=prior.battle_animation_context;
        w->sound_observer=prior.sound_observer;w->sound_context=prior.sound_context;
        w->direction=c->direction>3?3:c->direction;w->music_enabled=0;w->map_init_pending=0;w->lcd_background=0;
        w->condition_words[69]=(uint16_t)c->before;w->object_done[5]=(uint8_t)c->done_before;
        NpeMap map;assert(!npe_map(fixture_file,79,&map)&&!npe_world_load_tiles(w,&map));npe_world_seed(w,11);
    }
    npe_sound_select(p,w);SDL_PauseAudioDevice(p->device,1);
}
static void present(SDL_Renderer *r){
    SDL_RenderPresent(r);const PickupCase *c=&pickup_cases[case_index];
    uint32_t rgba[28800];assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<28800;++i){unsigned v=last_pixel_frame[i];
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    if(!phase){check_state(c->before,c->done_before);phase=1;return;}
    fprintf(stderr,"Pickup %s submit%u qty%u done%u clock%u\n",c->name,submits,live->condition_words[69],live->object_done[5],fake_clock);
    assert(c->acted&&submits<3);
    if(!submits){
        check_state(c->before,1);memcpy(base,last_pixel_frame,sizeof(base));
        NpeMap map;assert(!npe_map(fixture_file,79,&map));
        assert(npe_world_tile(live,&map,26,20)==53);
    }else if(submits==1){
        check_state(c->after,1);
        for(unsigned i=0;i<28800;++i)assert(last_pixel_frame[i]==(overlay[i]==255?base[i]:overlay[i]));
    }else{check_state(c->after,1);assert(!memcmp(base,last_pixel_frame,sizeof(base)));}
    ++submits;++total;
}
static int poll_input(SDL_Event *e){
    memset(e,0,sizeof(*e));
    if(submits==2&&!dismissed&&closing){dismissed=finished=1;e->type=SDL_QUIT;return 1;}
    /* Alternate immediate-key dismissal with the exact nominal timeout.
       Both traverse the actual main.c timed-notice implementation. */
    if(submits==2&&!dismissed&&!timeout_case){dismissed=1;e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F4;return 1;}
    return 0;
}
static int wait_input(SDL_Event *e,int timeout){
    (void)timeout;memset(e,0,sizeof(*e));const PickupCase *c=&pickup_cases[case_index];
    if(!inputs&&c->direction>3){live->direction=c->direction;world_map_frame_state.direction=c->direction;}
    if(inputs){check_state(c->after,c->done_after);assert(submits==(c->acted?3u:0u));
        if(c->acted)assert(fake_clock==100u);}
    if(inputs==2){finished=1;e->type=SDL_QUIT;return 1;}
    ++inputs;e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_RETURN;return 1;
}
static void put_bytes(const char *path,uint8_t *bytes,size_t size){
    for(unsigned i=0;i<4;++i)bytes[24+i]=(uint8_t)((size-36)>>(8*i));
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=36;i<size;++i){hash^=bytes[i];hash*=UINT64_C(1099511628211);}
    for(unsigned i=0;i<8;++i)bytes[28+i]=(uint8_t)(hash>>(8*i));
    FILE *f=fopen(path,"wb");assert(f&&fwrite(bytes,1,size,f)==size&&!fclose(f));
}
static void save_checks(const char *path){
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(path,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    NpeWorld w,loaded;NpeMapObject object;NpeMap map;
    assert(!npe_world_new_game(&file,&w)&&!npe_world_init(&file,&w,80,27,20)&&!npe_map(&file,79,&map));w.direction=3;
    assert(npe_world_object_action(&file,&w,&object)==1&&npe_world_tile(&w,&map,26,20)==53);
    const char *save="tmp/object-pickup.npes";
    assert(!npe_world_save(&file,&w,save)&&!npe_world_load(&file,&loaded,save));
    assert(loaded.condition_words[69]==1&&loaded.object_done[5]==1&&npe_world_tile(&loaded,&map,26,20)==53);
    assert(!npe_world_object_action(&file,&loaded,&object));
    uint8_t raw[4096];FILE *f=fopen(save,"rb");assert(f);size=fread(raw,1,sizeof(raw),f);assert(!fclose(f)&&!memcmp(raw,"NPESAV22",8));
    memcpy(raw,"NPESAV18",8);put_bytes(save,raw,size-128-NPE_CHARACTER_RECORD_SAVE_BYTES);assert(!npe_world_load(&file,&loaded,save));
    assert(loaded.object_done[5]==1&&npe_world_tile(&loaded,&map,26,20)==53);
    /* A genuine old C save had a taken flag but no pickup tile pair. */
    w.tile_override_count=0;memset(w.tile_override_indices,0,sizeof(w.tile_override_indices));memset(w.tile_override_tiles,0,sizeof(w.tile_override_tiles));
    assert(!npe_world_save(&file,&w,save));f=fopen(save,"rb");assert(f);size=fread(raw,1,sizeof(raw),f);assert(!fclose(f));
    memcpy(raw,"NPESAV18",8);put_bytes(save,raw,size-128-NPE_CHARACTER_RECORD_SAVE_BYTES);assert(!npe_world_load(&file,&loaded,save));
    assert(npe_world_tile(&loaded,&map,26,20)==53);
    /* All45 story pairs plus eight object cells fit without memory damage. */
    assert(!npe_world_init(&file,&w,80,27,20));w.direction=3;
    w.tile_override_count=45;for(unsigned i=0;i<45;++i){w.tile_override_indices[i]=(uint16_t)i;w.tile_override_tiles[i]=10;}
    for(unsigned i=0;i<8;++i){w.object_done[5]=0;w.view.offset_x=20-i;w.view.screen_x=7;NpeMapObject candidate;
        const uint8_t coordinate=(uint8_t)(26-i);
        assert(!npe_world_resident_write(&w,0x5275a4,&coordinate,1));
        assert(npe_world_object_action(&file,&w,&candidate)==1);}
    assert(w.tile_override_count==53);
    assert(!remove(save));npe_free(&file);
}
static void timeout_checks(void){
    assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("pickup timeout",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    NpeWorld w;assert(!npe_world_new_game(fixture_file,&w)&&!npe_world_init(fixture_file,&w,80,27,20));
    w.direction=3;w.music_enabled=0;w.lcd_background=0;w.map_init_pending=0;npe_world_seed(&w,11);live=&w;
    uint8_t pixels[28800];assert(!npe_render_world_load(fixture_file,&w,pixels));
    case_index=0;closing=0;timeout_case=1;phase=submits=dismissed=0;fake_clock=UINT32_MAX-4095;
    draw_pixels(renderer,texture,pixels);Uint32 start=fake_clock;
    assert(show_facing_action(window,renderer,texture,fixture_file,&font,&w,0)==1&&submits==3&&fake_clock==start+5000u);
    assert(world_map_frame_valid&&world_map_frame_uncommitted&&same_display_world(&world_map_frame_state,&w));
    assert(!show_facing_action(window,renderer,texture,fixture_file,&font,&w,0)&&submits==3);
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    puts("Pickup standalone service:5000ms timer across SDL tick wrap,source notice and retained map passed");
}
static void capacity_save_checks(void){
    NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeWorld w,loaded;NpeMap map;NpeMapObject object;
    assert(!read_file("tmp/object-pickup-capacity.npe",&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!npe_world_new_game(&file,&w)&&!npe_world_init(&file,&w,80,27,20)&&!npe_map(&file,79,&map));
    assert(w.tile_override_count==45);
    for(unsigned i=0;i<8;++i){w.direction=3;w.view.offset_x=20-i;w.view.screen_x=7;w.x=27-i;
        assert(npe_world_object_action(&file,&w,&object)==1&&object.id==i+1&&npe_world_tile(&w,&map,26-i,20)==53);}
    assert(w.tile_override_count==53&&w.condition_words[69]==8);
    const char *path="tmp/object-pickup-capacity.npes";assert(!npe_world_save(&file,&w,path)&&!npe_world_load(&file,&loaded,path));
    assert(loaded.tile_override_count==53&&!memcmp(loaded.tile_override_indices,w.tile_override_indices,sizeof(w.tile_override_indices))&&
           !memcmp(loaded.tile_override_tiles,w.tile_override_tiles,sizeof(w.tile_override_tiles)));
    for(unsigned i=0;i<8;++i)assert(loaded.object_done[i]==1&&npe_world_tile(&loaded,&map,26-i,20)==53);
    uint8_t raw[4096];FILE *f=fopen(path,"rb");assert(f);size=fread(raw,1,sizeof(raw),f);assert(!fclose(f));
    raw[size-128-NPE_CHARACTER_RECORD_SAVE_BYTES]=0xff;raw[size-127-NPE_CHARACTER_RECORD_SAVE_BYTES]=0xff;put_bytes(path,raw,size);NpeWorld prior=loaded;
    assert(npe_world_load(&file,&loaded,path)==-1&&!memcmp(&prior,&loaded,sizeof(prior)));
    assert(!remove(path));npe_free(&file);
    puts("Diagnostic NPE:45 story plus8 object cells,V19 all53 pairs round trip and bad extended-tail transaction passed");
}
int main(int argc,char **argv){assert(argc==2);FILE *f=fopen("tmp/object-pickup-notice-overlay.bin","rb");
    assert(f&&fread(overlay,1,sizeof(overlay),f)==sizeof(overlay)&&fgetc(f)==EOF&&!fclose(f));save_checks(argv[1]);capacity_save_checks();
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));fixture_file=&file;
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<12;++case_index){phase=inputs=submits=finished=dismissed=0;fake_clock=100;live=NULL;
        world_map_frame_valid=world_map_saved_edges_valid=0;char *args[]={"npe-sdl","--new-game",argv[1],NULL};
        assert(!run(3,args)&&finished&&submits==(pickup_cases[case_index].acted?3u:0u));
        printf("Pickup %s:actual SDL main,numeric original match,source notice,%u commits passed\n",pickup_cases[case_index].name,submits);}
    assert(total==24);case_index=0;closing=1;phase=inputs=submits=finished=dismissed=0;fake_clock=100;live=NULL;world_map_frame_valid=world_map_saved_edges_valid=0;
    char *args[]={"npe-sdl","--new-game",argv[1],NULL};assert(!run(3,args)&&finished&&submits==2);
    timeout_checks();assert(total==29);npe_free(&file);puts("Pickup:12 original numeric paths,24 main/3 timeout/2 close source-backed SDL commits,repeat/key/close,V19/V18 migration and53 tile capacity passed");return 0;
}
