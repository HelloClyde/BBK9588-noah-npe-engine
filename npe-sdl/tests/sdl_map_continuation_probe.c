/* Enter requested maps through unchanged current-NPE exit records and the
   actual SDL walking/notice controller. Pixel checks compare SDL with C;
   this does not supply an original LCD reference or prove natural reachability. */
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
static Uint32 probe_clock;
static unsigned submits;
static Uint32 ticks(void){return probe_clock;}
static void delay(Uint32 ms){probe_clock+=ms;}
static int no_input(SDL_Event *event){(void)event;return 0;}
static int wait_timeout(SDL_Event *event,int ms){
    (void)event;assert(ms>=0);probe_clock+=(Uint32)ms;return 0;
}
static void present(SDL_Renderer *);
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent no_input
#define SDL_WaitEventTimeout wait_timeout
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){unsigned v=last_pixel_frame[i];assert(v<16);
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    ++submits;assert(submits<1000);
}
static void check_bad_buffer_saves(NpeFile *file,NpeWorld *cold,const char *path){
    uint8_t *wire;size_t size;assert(!read_file(path,&wire,&size));
    if(memcmp(wire,"NPESAV24",8)){free(wire);return;}
    size_t trailer=size>=4&&!memcmp(wire+size-4,"WTBL",4)?4:0;
    assert(size>7196+trailer);size_t extension=size-7196-trailer;
    uint8_t *bad=malloc(size+2);NpeWorld *before=malloc(sizeof(*before));assert(bad&&before);
    for(unsigned kind=0;kind<4;++kind){
        memcpy(bad,wire,size);size_t length=size;
        if(kind==0)bad[extension+2]^=1;
        else if(kind==1)length-=2;
        else if(kind==2){bad[length++]=0x5a;bad[length++]=0xa5;}
        else ++bad[43]; /* Inconsistent saved screen X, checksum still valid. */
        uint64_t hash=UINT64_C(14695981039346656037);
        for(size_t i=36;i<length;++i){hash^=bad[i];hash*=UINT64_C(1099511628211);}
        for(unsigned i=0;i<4;++i)bad[24+i]=(uint8_t)((length-36)>>(8*i));
        for(unsigned i=0;i<8;++i)bad[28+i]=(uint8_t)(hash>>(8*i));
        char broken[180];snprintf(broken,sizeof(broken),"%s.bad-%u",path,kind);
        FILE *out=fopen(broken,"wb");assert(out&&fwrite(bad,1,length,out)==length&&!fclose(out));
        *before=*cold;assert(npe_world_load(file,cold,broken)==-1&&!memcmp(before,cold,sizeof(*cold)));
        assert(!remove(broken));
    }
    fprintf(stderr,"special-map-save four checksum-valid malformed V24 slots rejected transactionally\n");
    free(before);free(bad);free(wire);
}
static int check_save_restore(NpeFile *file,NpeWorld *world,SDL_Renderer *renderer,SDL_Texture *texture,const NpeFont *font){
    char path[160];snprintf(path,sizeof(path),"tmp/special-map-%u-save.npes",world->map_number);
    uint8_t expected[NPE_WIDTH*NPE_HEIGHT],actual[NPE_WIDTH*NPE_HEIGHT],records[3][148],raw[148];
    assert(!npe_render_world_load(file,world,expected));
    for(unsigned i=0;i<3;++i)assert(!npe_world_map_record_image(world,i,records[i]));
    unsigned map=world->map_number,x=world->x,y=world->y,money=world->money,event=world->story_event_id;
    NpeViewport view=world->view;
    int result=npe_world_save(file,world,path);
    fprintf(stderr,"special-map-save map%u save=%d pos=%u,%u view=%u,%u+%u,%u\n",
        map,result,x,y,view.offset_x,view.offset_y,view.screen_x,view.screen_y);
    if(result)return 1;
    char saved_frame[200];snprintf(saved_frame,sizeof(saved_frame),"%s.pixels",path);
    FILE *saved_pixels=fopen(saved_frame,"wb");assert(saved_pixels&&fwrite(expected,1,sizeof(expected),saved_pixels)==sizeof(expected)&&!fclose(saved_pixels));
    uint16_t buffer[NPE_MAP_BUFFER_WORDS];
    for(unsigned i=0;i<NPE_MAP_BUFFER_WORDS;++i)assert(npe_rpg_runtime_map_tile(file,map,(uint16_t)i,buffer+i)==1);
    char buffer_path[180];snprintf(buffer_path,sizeof(buffer_path),"tmp/special-map-%u-before-buffer.bin",map);
    FILE *buffer_file=fopen(buffer_path,"wb");assert(buffer_file);
    for(unsigned i=0;i<NPE_MAP_BUFFER_WORDS;++i){uint8_t pair[2]={(uint8_t)buffer[i],(uint8_t)(buffer[i]>>8)};
        assert(fwrite(pair,1,2,buffer_file)==2);}
    assert(!fclose(buffer_file));
    NpeWorld *cold=malloc(sizeof(*cold));assert(cold);
    assert(!npe_world_load(file,cold,path));
    world->money^=0x1234;
    assert(!npe_world_restore(file,world,path));
    assert(world->map_number==map&&world->x==x&&world->y==y&&world->money==money&&world->story_event_id==event);
    assert(!memcmp(&world->view,&view,sizeof(view)));
    for(unsigned i=0;i<3;++i)assert(!npe_world_map_record_image(world,i,raw)&&!memcmp(records[i],raw,148));
    assert(!npe_render_world_load(file,world,actual));
    unsigned warm_diff=0;for(unsigned i=0;i<sizeof(actual);++i)warm_diff+=actual[i]!=expected[i];
    draw_pixels(renderer,texture,actual);
    fprintf(stderr,"special-map-save map%u warm pixels=%u\n",map,warm_diff);assert(!warm_diff);
    int move_result=game_world_step_display(file,world,3,renderer,texture,font);
    unsigned move_x=world->x,move_y=world->y,move_mode=world->game_mode,move_event=world->story_event_id;
    NpeViewport move_view=world->view;
    uint8_t move_pixels[NPE_WIDTH*NPE_HEIGHT];memcpy(move_pixels,last_pixel_frame,sizeof(move_pixels));
    npe_rpg_runtime_close(file);assert(!npe_world_resources_open(file,cold));
    assert(cold->map_number==map&&cold->x==x&&cold->y==y&&cold->money==money&&cold->story_event_id==event);
    assert(!memcmp(&cold->view,&view,sizeof(view)));
    for(unsigned i=0;i<3;++i)assert(!npe_world_map_record_image(cold,i,raw)&&!memcmp(records[i],raw,148));
    assert(!npe_render_world_load(file,cold,actual));
    if(cold->terrain_buffer_snapshot_ready)for(unsigned i=0;i<NPE_MAP_BUFFER_WORDS;++i){
        uint16_t tile;assert(npe_rpg_runtime_map_tile(file,map,(uint16_t)i,&tile)==1&&tile==buffer[i]);
    }
    unsigned cold_diff=0;for(unsigned i=0;i<sizeof(actual);++i)cold_diff+=actual[i]!=expected[i];
    char frame[180];snprintf(frame,sizeof(frame),"tmp/special-map-%u-before-save.idx",map);
    FILE *out=fopen(frame,"wb");assert(out&&fwrite(expected,1,sizeof(expected),out)==sizeof(expected)&&!fclose(out));
    snprintf(frame,sizeof(frame),"tmp/special-map-%u-cold-load.idx",map);
    out=fopen(frame,"wb");assert(out&&fwrite(actual,1,sizeof(actual),out)==sizeof(actual)&&!fclose(out));
    draw_pixels(renderer,texture,actual);
    fprintf(stderr,"special-map-save map%u cold pixels=%u buffer=%u\n",map,cold_diff,
        cold->terrain_buffer_snapshot_ready?NPE_MAP_BUFFER_WORDS:0);
    retain_world_frame(file,cold,renderer,actual,1);
    int cold_move=game_world_step_display(file,cold,3,renderer,texture,font);
    unsigned move_diff=0;for(unsigned i=0;i<sizeof(move_pixels);++i)move_diff+=last_pixel_frame[i]!=move_pixels[i];
    fprintf(stderr,"special-map-save map%u next-left result=%d/%d pixels=%u\n",map,move_result,cold_move,move_diff);
    assert(move_result==cold_move&&cold->x==move_x&&cold->y==move_y&&cold->game_mode==move_mode&&cold->story_event_id==move_event);
    assert(!memcmp(&cold->view,&move_view,sizeof(move_view))&&!move_diff);
    char moved_save[180];snprintf(moved_save,sizeof(moved_save),"%s.after-move",path);
    assert(!npe_world_save(file,cold,moved_save));
    snprintf(saved_frame,sizeof(saved_frame),"%s.pixels",moved_save);
    saved_pixels=fopen(saved_frame,"wb");assert(saved_pixels&&fwrite(move_pixels,1,sizeof(move_pixels),saved_pixels)==sizeof(move_pixels)&&!fclose(saved_pixels));
    check_bad_buffer_saves(file,cold,path);
    printf("{\"save_checked\":true,\"map\":%u,\"warm_diff\":%u,\"cold_diff\":%u,\"move_diff\":%u,\"buffer_words\":%u}\n",
        map,warm_diff,cold_diff,move_diff,cold->terrain_buffer_snapshot_ready?NPE_MAP_BUFFER_WORDS:0);
    npe_rpg_runtime_close(file);
    char closed_save[180];snprintf(closed_save,sizeof(closed_save),"%s.closed",path);
    assert(!npe_world_save(file,cold,closed_save));
    uint8_t *moved_wire,*closed_wire;size_t moved_size,closed_size;
    assert(!read_file(moved_save,&moved_wire,&moved_size)&&!read_file(closed_save,&closed_wire,&closed_size));
    assert(moved_size==closed_size&&!memcmp(moved_wire,closed_wire,moved_size));
    free(moved_wire);free(closed_wire);
    fprintf(stderr,"special-map-save map%u closed save equals live after-move slot\n",map);
    free(cold);return cold_diff?1:0;
}
int main(int argc,char **argv){
    assert(argc>=3);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    assert(!load_native_fonts(&font));
    SDL_Window *window=SDL_CreateWindow("NPE map continuation",0,0,NPE_WIDTH,NPE_HEIGHT,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    unsigned failed=0;int check_save=argc>3&&!strcmp(argv[2],"--save");
    for(int argument=check_save?3:2;argument<argc;++argument){
        unsigned target=(unsigned)strtoul(argv[argument],NULL,10),matched=0;
        for(unsigned number=1;number<=file.offset_table_records[8];++number){
            NpeMap map;assert(!npe_map(&file,number-1,&map));
            for(unsigned slot=0;slot<12;++slot){
                NpeMapExit event;assert(!npe_map_exit(&map,slot,&event));
                if(event.destination_map!=target)continue;
                /* This short path covers automatic primary exits, not the
                   secondary choice branch or diagnostic collision overrides. */
                assert(!event.kind&&event.x<map.width&&event.y+1u<map.height);
                NpeWorld world;assert(!npe_world_init(&file,&world,number,event.x,event.y+1u));
                world.music_enabled=0;world.defer_story_events=1;
                assert(world.story_progress>=event.min_progress&&!npe_world_resources_open(&file,&world));
                uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];assert(!npe_render_world_load(&file,&world,pixels));
                draw_pixels(renderer,texture,pixels);save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&world,renderer,pixels,1);
                unsigned before=submits;int result=game_world_step_display(&file,&world,2,renderer,texture,&font);
                fprintf(stderr,"exit %u/%u ->%u result=%d map=%u pos=%u,%u view=%u,%u screen=%u,%u\n",
                    number,slot+1,target,result,world.map_number,world.x,world.y,
                    world.view.offset_x,world.view.offset_y,world.view.screen_x,world.view.screen_y);
                int passed=result==NPE_MOVE_EXIT&&world.map_number==target;
                if(!passed)++failed;
                int name_result=npe_render_map_name_notice(&file,&font,&world,pixels);
                NpeMap destination;assert(!npe_map(&file,target-1,&destination));
                assert(!npe_rpg_runtime_failures(&file)&&npe_game_fingerprint(&file)==fingerprint);
                printf("{\"passed\":%s,\"result\":%d,\"map_name_result\":%d,\"source_map\":%u,\"exit_slot\":%u,\"target_map\":%u,\"width\":%u,\"height\":%u,\"mode\":%u,\"event\":%u,\"x\":%u,\"y\":%u,\"view_x\":%u,\"view_y\":%u,\"screen_x\":%u,\"screen_y\":%u,\"frames\":%u,\"original_lcd_checked\":false}\n",
                    passed?"true":"false",result,name_result,number,slot+1,target,destination.width,destination.height,world.game_mode,world.story_event_id,
                    world.x,world.y,world.view.offset_x,world.view.offset_y,world.view.screen_x,world.view.screen_y,submits-before);
                if(passed&&check_save)failed+=(unsigned)check_save_restore(&file,&world,renderer,texture,&font);
                npe_rpg_runtime_close(&file);++matched;
            }
        }
        assert(matched);
    }
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);
    npe_free(&file);SDL_Quit();return failed?1:0;
}
