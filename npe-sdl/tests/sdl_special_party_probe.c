/* Current-file special party records, sampled separately from their followups.
   Native state references come from the original CPU service, never this C
   output. Exported records are explicitly diagnostic inputs to that service. */
#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
static Uint32 probe_clock;
static unsigned frames,scene_frames;
static int no_input(SDL_Event *e){(void)e;return 0;}
static int next_key(SDL_Event *e){memset(e,0,sizeof(*e));e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_SPACE;return 1;}
static Uint32 ticks(void){return probe_clock;}
static void delay(Uint32 ms){probe_clock+=ms;}
static void present(SDL_Renderer *);
#define SDL_PollEvent no_input
#define SDL_WaitEvent next_key
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEvent
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_RenderPresent
static void present(SDL_Renderer *r){
    SDL_RenderPresent(r);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){unsigned v=last_pixel_frame[i];
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    ++frames;assert(++scene_frames<2000);
}
#include "special_party_native.h"
static void output(const char *dir,const char *name,const char *suffix,const void *data,size_t n){
    char path[1024];snprintf(path,sizeof(path),"%s/%s.%s",dir,name,suffix);
    FILE *f=fopen(path,"wb");assert(f&&fwrite(data,1,n,f)==n&&!fclose(f));
}
int main(int argc,char **argv){
    assert(argc==4);FILE *manifest=fopen(argv[1],"rb");char source[1024];unsigned maps[16],map_count=0,n;
    assert(manifest&&fgets(source,sizeof(source),manifest));source[strcspn(source,"\r\n")]=0;
    while(fscanf(manifest,"%u",&n)==1){assert(map_count<16);maps[map_count++]=n;}assert(map_count&&!fclose(manifest));
    uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(source,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    NpeFont font={0};assert(!load_native_fonts(&font));
    SDL_Window *win=SDL_CreateWindow("special party",0,0,NPE_WIDTH,NPE_HEIGHT,SDL_WINDOW_HIDDEN);assert(win);
    SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    char path[1024];snprintf(path,sizeof(path),"%s/cases.jsonl",argv[2]);FILE *report=fopen(path,"wb");assert(report);
    unsigned cases=0,menu_entries=0,menu_frames=0;int verify=strcmp(argv[3],"--export")!=0;
    for(unsigned mi=0;mi<map_count;++mi){NpeMap map;unsigned party_count,leader_count;
        assert(!npe_map(&file,maps[mi]-1,&map)&&!npe_map_group_count(&map,6,&party_count)&&!npe_map_group_count(&map,3,&leader_count));
        for(unsigned mode=10;mode<=13;mode+=3)for(unsigned slot=1;slot<=(mode==10?party_count:leader_count);++slot)
        for(unsigned variant=0;variant<(mode==10?2u:1u);++variant){
            char name[80];snprintf(name,sizeof(name),"map%u-mode%u-slot%u-v%u",maps[mi],mode,slot,variant);
            NpeWorld world;scene_frames=0;assert(!npe_world_init(&file,&world,maps[mi],2,2)&&!npe_world_resources_open(&file,&world));
            world.music_enabled=0;world.defer_story_events=1;world.hp[0]=world.qi[0]=1;
            world.party_active[0]=1;world.party_active[1]=(uint8_t)(mode==13||variant);world.party_active[2]=(uint8_t)(mode==13);
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];assert(!npe_render_world_load(&file,&world,pixels));
            draw_pixels(renderer,texture,pixels);save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&world,renderer,pixels,1);
            world.game_mode=(uint16_t)mode;world.story_event_id=(uint16_t)slot;
            uint8_t before[444];special_party_records(&world,before);output(argv[2],name,"before",before,sizeof(before));
            if(verify)check_special_party_native(argv[3],name,&world,1);
            int result=mode==10?show_party_event(renderer,texture,&file,&font,&world):show_party_adjust_event(renderer,texture,&file,&world);
            fprintf(report,"{\"name\":\"%s\",\"map\":%u,\"mode\":%u,\"slot\":%u,\"variant\":%u,\"service_result\":%d,\"after_mode\":%u,\"after_event\":%u,\"hp\":%u,\"qi\":%u,\"level\":%u,\"experience\":%u,\"native_checked\":%s,",
                name,maps[mi],mode,slot,variant,result,world.game_mode,world.story_event_id,world.hp[0],world.qi[0],world.level[0],world.experience[0],verify?"true":"false");
            fflush(report);assert(result>=0);if(verify)check_special_party_native(argv[3],name,&world,0);
            uint8_t after[444];special_party_records(&world,after);output(argv[2],name,"c-after",after,sizeof(after));
            result=advance_world_events_dispatch(win,renderer,texture,&file,&font,&world,mode,0);
            unsigned followup_mode=world.game_mode,followup_event=world.story_event_id,return_frames=0;
            if(world.game_mode==4 && world.story_event_id!=2 && (int16_t)world.event_context_id<10){
                unsigned before_frames=scene_frames,levels[3]={0};uint8_t visible[NPE_WIDTH*NPE_HEIGHT];
                memcpy(visible,last_pixel_frame,sizeof(visible));uint8_t old_ui=world.shop_ui,old_kind=world.shop_kind;
                assert(world.map_init_pending);
                assert(!draw_world(renderer,texture,&file,&world,0,0,0,0,0,0,levels,0,&font));
                assert(scene_frames==before_frames&&!world.map_init_pending&&world.shop_ui==old_ui&&world.shop_kind==old_kind);
                assert(!memcmp(visible,last_pixel_frame,sizeof(visible))&&world.shop_scan_first==1&&world.shop_scan_last==254&&!world.shop_inventory_only);
                if(old_ui!=1){
                    assert(!game_shop_return_display(renderer,texture,&file,&world));
                    assert(scene_frames==before_frames+1&&world.game_mode==4&&world.shop_ui==1&&!memcmp(visible,last_pixel_frame,sizeof(visible)));
                }
                assert(game_shop_return_display(renderer,texture,&file,&world)==1&&world.game_mode==1&&!world.map_init_pending&&world.story_event_id==followup_event);
                return_frames=scene_frames-before_frames;assert(return_frames==(old_ui==1?1u:2u));++menu_entries;menu_frames+=return_frames;
            }
            fprintf(report,"\"followup_result\":%d,\"followup_mode\":%u,\"followup_event\":%u,\"menu_return_frames\":%u,\"final_mode\":%u,\"final_event\":%u,\"frames\":%u,\"runtime_failures\":%u}\n",
                result,followup_mode,followup_event,return_frames,world.game_mode,world.story_event_id,scene_frames,npe_rpg_runtime_failures(&file));fflush(report);
            fprintf(stderr,"special party %s service=%d followup=%d ->%u/%u frames=%u\n",name,mode==10?1:0,result,world.game_mode,world.story_event_id,scene_frames);
            assert(result>=0&&!npe_rpg_runtime_failures(&file));++cases;npe_rpg_runtime_close(&file);
        }
    }
    assert(fingerprint==npe_game_fingerprint(&file)&&!fclose(report));
    fprintf(stderr,"Special party: %u cases, %u SDL/C frames, original states %s; %u rejected menu entries, %u return frames\n",cases,frames,verify?"matched":"not checked (input export)",menu_entries,menu_frames);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);npe_font_free(&font);npe_free(&file);SDL_Quit();return 0;
}
