#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
static unsigned commits,active;
static void present(SDL_Renderer *);
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
#include "mode4_rejected_entry_native.h"
static const char *reference_directory,*case_name;
static unsigned reference_commit;
static void present(SDL_Renderer *r){
    SDL_RenderPresent(r);if(!active)return;++commits;
    char path[1024];uint8_t expected[NPE_WIDTH*NPE_HEIGHT];
    snprintf(path,sizeof(path),"%s/%s-%u.idx",reference_directory,case_name,reference_commit++);
    FILE *in=fopen(path,"rb");assert(in&&fread(expected,1,sizeof(expected),in)==sizeof(expected)&&!fclose(in));
    unsigned differences=0;for(unsigned i=0;i<sizeof(expected);++i)differences+=expected[i]!=last_pixel_frame[i];
    fprintf(stderr,"mode4 %s commit%u: original pixel differences=%u\n",case_name,reference_commit-1,differences);
    assert(!differences);
    uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){unsigned v=last_pixel_frame[i];
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
}
int main(int argc,char **argv){
    assert(argc==3);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    NpeFont font={0};assert(!load_native_fonts(&font));
    SDL_Window *win=SDL_CreateWindow("mode4 rejected entry",0,0,NPE_WIDTH,NPE_HEIGHT,SDL_WINDOW_HIDDEN);assert(win);
    SDL_Renderer *r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(r);
    SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(t);
    reference_directory=argv[2];unsigned total=0;
    for(unsigned c=0;c<sizeof(mode4_rejected_cases)/sizeof(*mode4_rejected_cases);++c){
        const Mode4RejectedCase *sample=mode4_rejected_cases+c;case_name=sample->name;reference_commit=0;
        NpeWorld w;assert(!npe_world_init(&file,&w,mode4_rejected_view[0],43,2)&&!npe_world_resources_open(&file,&w));
        w.story_progress=(uint16_t)mode4_rejected_view[1];w.view.screen_x=mode4_rejected_view[2];w.view.screen_y=mode4_rejected_view[3];
        w.view.offset_x=mode4_rejected_view[4];w.view.offset_y=mode4_rejected_view[5];w.direction=(uint8_t)mode4_rejected_view[6];
        for(unsigned i=0;i<20;++i){w.npc_override_x[i]=(uint16_t)mode4_rejected_npc[i][0];w.npc_override_y[i]=(uint16_t)mode4_rejected_npc[i][1];w.npc_override_direction[i]=(uint8_t)mode4_rejected_npc[i][2];}
        w.game_mode=4;w.story_event_id=1796;w.event_context_id=(uint16_t)sample->context;w.map_init_pending=1;w.music_enabled=0;
        w.shop_kind=(uint8_t)sample->kind;w.shop_ui=(uint8_t)sample->ui;w.shop_selected_item=65535;w.shop_first_item=9;w.shop_last_item=37;
        w.shop_row=6;w.shop_quantity=99;w.shop_choice=0;w.shop_transaction_sell=1;
        w.shop_scan_first=10;w.shop_scan_last=20;w.shop_inventory_only=255;w.shop_description_start=82;w.shop_description_end=97;
        char path[1024];uint8_t before[NPE_WIDTH*NPE_HEIGHT];snprintf(path,sizeof(path),"%s/%s-before.idx",argv[2],sample->name);
        FILE *in=fopen(path,"rb");assert(in&&fread(before,1,sizeof(before),in)==sizeof(before)&&!fclose(in));
        active=0;draw_pixels(r,t,before);retain_world_frame(&file,&w,r,before,1);active=1;commits=0;
        unsigned levels[3]={0};int result=draw_world(r,t,&file,&w,0,0,0,0,0,0,levels,0,&font);
        fprintf(stderr,"mode4 %s entry: result=%d commits=%u init=%u ui=%u\n",sample->name,result,commits,w.map_init_pending,w.shop_ui);
        assert(!result&&!commits&&!memcmp(before,last_pixel_frame,sizeof(before)));
        assert(!w.map_init_pending&&w.lcd_background==0&&w.shop_scan_first==1&&w.shop_scan_last==254&&!w.shop_inventory_only);
        assert(w.game_mode==4&&w.story_event_id==1796&&w.event_context_id==sample->context&&w.shop_kind==sample->kind&&w.shop_ui==sample->ui);
        assert(w.shop_selected_item==65535&&w.shop_first_item==9&&w.shop_last_item==37&&w.shop_row==6&&w.shop_quantity==99&&!w.shop_choice&&w.shop_transaction_sell==1);
        assert(w.shop_description_start==82&&w.shop_description_end==97);
        assert(!draw_world(r,t,&file,&w,0,0,0,0,0,0,levels,0,&font)&&!commits);
        if(sample->ui!=1){
            assert(!game_shop_return_display(r,t,&file,&w)&&commits==1);
            assert(w.game_mode==4&&w.shop_ui==1&&!w.map_init_pending&&!memcmp(before,last_pixel_frame,sizeof(before)));
            assert(!draw_world(r,t,&file,&w,0,0,0,0,0,0,levels,0,&font)&&commits==1);
        }
        assert(game_shop_return_display(r,t,&file,&w)==1&&commits==(sample->ui==1?1u:2u));
        assert(w.game_mode==1&&!w.map_init_pending&&w.shop_ui==1&&w.story_event_id==1796&&w.event_context_id==sample->context);
        assert(w.shop_kind==sample->kind&&w.shop_selected_item==65535&&w.shop_first_item==9&&w.shop_last_item==37&&w.shop_row==6&&w.shop_quantity==99&&!w.shop_choice&&w.shop_transaction_sell==1);
        assert(w.shop_description_start==82&&w.shop_description_end==97);total+=commits;
        active=0;npe_rpg_runtime_close(&file);
    }
    fprintf(stderr,"Mode4 rejected entry: 5 entries without LCD, %u original C/SDL frames matched\n",total);
    SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);npe_font_free(&font);npe_free(&file);SDL_Quit();return 0;
}
