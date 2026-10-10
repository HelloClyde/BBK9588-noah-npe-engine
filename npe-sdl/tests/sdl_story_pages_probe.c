#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../npe.h"
#include "story_text_native.h"
static NpeWorld *active_text_world;
static const StoryTextPages *active_text_pages;
static unsigned story_id,page,finish_calls;
static int probe_wait(SDL_Event *event);
static int probe_finish(const NpeFile *file,NpeWorld *world);
static void probe_present(SDL_Renderer *renderer);
#define SDL_WaitEvent probe_wait
#define SDL_RenderPresent probe_present
#define npe_world_finish_story probe_finish
#define main npe_app_main
#include "../main.c"
#undef main
#undef npe_world_finish_story
#undef SDL_RenderPresent
#undef SDL_WaitEvent

static void probe_present(SDL_Renderer *renderer){
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    char path[128];snprintf(path,sizeof(path),"tmp/native-story-%u-page-%u.idx",story_id,page);
    FILE *stream=fopen(path,"rb");assert(stream);
    assert(fread(native,1,sizeof(native),stream)==sizeof(native)&&fgetc(stream)==EOF&&!fclose(stream));
    SDL_RenderPresent(renderer);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned differences=0;
    for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        differences+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"SDL original story %u page %u: %u differences\n",story_id,page,differences);
    assert(!differences);
    const StoryTextState *text=active_text_pages->pages+page;
    assert(active_text_world->text_offset==text->offset&&active_text_world->text_more==text->more&&active_text_world->text_style==text->style);
    assert(!memcmp(active_text_world->text_rect,text->rect,8));
    ++page;
}
static int probe_wait(SDL_Event *event){
    assert(page&&!finish_calls);memset(event,0,sizeof(*event));
    event->type=SDL_KEYDOWN;
    event->key.keysym.sym=SDLK_SPACE;return 1;
}
static int probe_finish(const NpeFile *file,NpeWorld *world){
    NpeStoryRecord story;assert(!npe_story_record(file,story_id-1,&story));
    assert(world->game_mode==5&&world->story_event_id==story_id);
    assert(npe_world_finish_story(file,world)==1);
    /* Native dispatcher resolves mode14 through map9's original event1.
       All other selected records retain their raw continuation fields. */
    assert(world->game_mode==(story.control_byte==14?1:story.control_byte));
    assert(world->story_event_id==(story.control_byte==14?1:story.control_word));
    ++finish_calls;return 0; // Stop after this original record's verified followup.
}
int main(int argc,char **argv){
    assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("story pages",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    const unsigned ids[]={4,9,23,37,63,164,181,198},counts[]={1,1,2,1,3,5,5,5};unsigned total=0;
    for(unsigned c=0;c<sizeof(ids)/sizeof(*ids);++c){
        NpeWorld world;assert(!npe_world_init(&file,&world,9,29,13));
        world.game_mode=5;world.story_event_id=story_id=ids[c];page=finish_calls=0;
        active_text_world=&world;active_text_pages=story_text_pages+0+c;
        show_walk_story(window,renderer,texture,&file,&font,&world);
        assert(page==counts[c]&&finish_calls==1);total+=page;
        npe_rpg_runtime_close(&file);
    }
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    printf("8 original stories, %u actual SDL pages, advance keys and followups passed\n",total);return 0;
}
