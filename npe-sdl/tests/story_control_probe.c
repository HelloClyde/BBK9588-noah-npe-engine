#include "../npe.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
static NpeWorld *loop_world;
static unsigned loop_pages,loop_finishes;
static uint8_t loop_native[NPE_WIDTH*NPE_HEIGHT];
static int loop_key(SDL_Event *e);
static int loop_finish(const NpeFile *file,NpeWorld *w);
static void loop_present(SDL_Renderer *r);
#define SDL_WaitEvent loop_key
#define npe_world_finish_story loop_finish
#define SDL_RenderPresent loop_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef npe_world_finish_story
#undef SDL_RenderPresent
static void loop_present(SDL_Renderer *r){
    SDL_RenderPresent(r);uint32_t pixels[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,pixels,NPE_WIDTH*4));
    for(unsigned i=0;i<sizeof(loop_native);++i){unsigned v=loop_native[i];
        assert(last_pixel_frame[i]==v);
        assert(pixels[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));
    }
}
static int loop_key(SDL_Event *e){
    assert(loop_world->game_mode==5&&loop_world->story_control_ready&&!loop_world->map_init_pending);
    ++loop_pages;memset(e,0,sizeof(*e));
    if(loop_finishes==220){e->type=SDL_QUIT;return 1;}
    loop_world->mode2_followup_mode=5;loop_world->mode2_followup_event_id=20;
    e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_SPACE;return 1;
}
static int loop_finish(const NpeFile *file,NpeWorld *w){
    assert(w==loop_world&&w->map_init_pending&&w->story_control_ready);
    assert(npe_world_finish_story(file,w)==1&&w->game_mode==5&&w->story_event_id==20&&!w->story_control_ready);
    ++loop_finishes;return 1;
}
int main(int argc,char **argv){
    assert(argc==2);uint8_t *bytes;size_t size;NpeFile file;char error[160];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    NpeWorld baseline;memset(&baseline,0xa5,sizeof(baseline));
    assert(!npe_world_init(&file,&baseline,18,13,5)&&!baseline.story_control_ready);
    const unsigned invalid[]={0,25,255};
    for(unsigned display=0;display<2;++display)for(unsigned i=0;i<3;++i){
        NpeWorld w=baseline;w.game_mode=5;w.story_event_id=20;w.defer_story_events=(uint8_t)display;
        assert(npe_world_prepare_story(&file,&w)==1&&w.story_control_ready&&!w.mode2_ready);
        w.mode2_followup_mode=(uint8_t)invalid[i];w.mode2_followup_event_id=65535;
        assert(npe_world_finish_story(&file,&w)==1&&!w.story_control_ready);
        assert(w.game_mode==(display?invalid[i]:1)&&w.story_event_id==65535&&w.story_progress==1);
        w=baseline;w.game_mode=2;w.mode2_ready=1;w.defer_story_events=(uint8_t)display;
        w.mode2_followup_mode=(uint8_t)invalid[i];w.mode2_followup_event_id=65535;
        assert(npe_world_finish_mode2(&w)==1&&!w.mode2_ready);
        assert(w.game_mode==(display?invalid[i]:1)&&w.story_event_id==65535&&w.story_progress==1);
        w=baseline;w.defer_story_events=(uint8_t)display;
        NpeNpcEvent e={0};e.followup_mode=(uint8_t)invalid[i];e.followup_event_id=65535;
        assert(npe_world_finish_npc_event(&w,&e)==1&&w.game_mode==(display?invalid[i]:1)&&w.story_event_id==65535);
    }
    NpeWorld w=baseline;w.game_mode=5;w.story_event_id=20;NpeStoryRecord story;
    assert(!npe_story_record(&file,19,&story)&&npe_world_prepare_story(&file,&w)==1);
    uint8_t *control=(uint8_t *)story.text-3,saved[3];memcpy(saved,control,3);
    control[0]=255;control[1]=0xff;control[2]=0xff;
    assert(npe_world_finish_story(&file,&w)==1&&w.game_mode==story.control_byte&&w.story_event_id==story.control_word);
    // A new entry reads the changed resource; an already open page does not.
    w.game_mode=5;w.story_event_id=20;assert(npe_world_prepare_story(&file,&w)==1);
    assert(w.mode2_followup_mode==255&&w.mode2_followup_event_id==65535);
    memcpy(control,saved,3);
    NpeWorld before=w;w.game_mode=5;w.story_event_id=65535;w.story_control_ready=0;before=w;
    assert(npe_world_prepare_story(&file,&w)<0&&!memcmp(&before,&w,sizeof(w)));
    const char *slot="tmp/text-control-slot.npes",*other="tmp/text-control-wire.npes";
    assert(!npe_world_save(&file,&baseline,slot));
    w=baseline;w.mode2_followup_mode=25;w.mode2_followup_event_id=65535;w.story_control_ready=1;w.mode2_ready=1;
    assert(!npe_world_save(&file,&w,other));
    uint8_t *sa,*sb;size_t na,nb;
    assert(!read_file(slot,&sa,&na)&&!read_file(other,&sb,&nb)&&na==nb&&!memcmp(sa,sb,na));
    assert(na>8&&!memcmp(sa,"NPESAV22",8));free(sa);free(sb);
    assert(!npe_world_restore(&file,&w,slot)&&w.mode2_followup_mode==25&&w.mode2_followup_event_id==65535&&!w.story_control_ready&&!w.mode2_ready);
    assert(!npe_world_restart_game(&file,&w)&&w.mode2_followup_mode==25&&w.mode2_followup_event_id==65535&&!w.story_control_ready&&!w.mode2_ready);
    assert(!npe_world_load(&file,&w,slot)&&!w.mode2_followup_mode&&!w.mode2_followup_event_id&&!w.story_control_ready&&!w.mode2_ready);
    assert(!remove(slot)&&!remove(other));
    w=baseline;w.story_progress=25;w.story_control_ready=1;NpeNpcEvent npc;
    assert(!npe_world_npc_event(&file,&w,24,&npc)&&w.mode2_ready&&!w.story_control_ready);
    FILE *native=fopen("tmp/native-text-cycle-4.idx","rb");
    assert(native&&fread(loop_native,1,sizeof(loop_native),native)==sizeof(loop_native)&&fgetc(native)==EOF&&!fclose(native));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("story loop",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *r=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(r);
    SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(t);
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    w=baseline;w.game_mode=5;w.story_event_id=20;w.defer_story_events=1;loop_world=&w;
    show_walk_story(window,r,t,&file,&font,&w);
    assert(loop_finishes==220&&loop_pages==221&&w.game_mode==5&&w.story_event_id==20&&w.story_control_ready);
    SDL_Event quit;int saw_quit=0;while(SDL_PollEvent(&quit))if(quit.type==SDL_QUIT)saw_quit=1;
    assert(saw_quit);
    npe_font_free(&font);SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(window);SDL_Quit();
    npe_free(&file);
    puts("Text continuations: invalid modes, resident cache isolation/lifetime, save byte isolation, failed preparation transaction, 220 SDL story reentries and cancellation passed");
    return 0;
}
