#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#ifdef NPE_STORY_IGNORED_KEYS
#include "story_ignored_native.h"
#else
#include "story_text_native.h"
#endif
static int text_wait(SDL_Event *);
static void text_present(SDL_Renderer *);
static int text_finish(const NpeFile *,NpeWorld *);
#define SDL_WaitEvent text_wait
#define SDL_RenderPresent text_present
#define npe_world_finish_story text_finish
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#undef npe_world_finish_story
#include "story_ignored_input.h"
static const StoryTextCase *current;
static NpeWorld *active;
static NpeFile *active_file;
static unsigned pages,finished,waits,reads_before,patched;
static void patch(const StoryTextState *s){
    uint8_t bytes[8];
    for(unsigned i=0;i<4;++i){bytes[2*i]=(uint8_t)(s->rect[i]>>8);bytes[2*i+1]=(uint8_t)s->rect[i];}
    assert(!npe_rpg_runtime_write(active_file,0x52791e,bytes,8));
    for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(s->pointer>>(24u-8u*i));
    assert(!npe_rpg_runtime_write(active_file,0x52797a,bytes,4));
    bytes[0]=(uint8_t)(s->offset>>8);bytes[1]=(uint8_t)s->offset;bytes[2]=s->more;
    assert(!npe_rpg_runtime_write(active_file,0x527a1a,bytes,3));
    bytes[0]=s->style;assert(!npe_rpg_runtime_write(active_file,0x527ace,bytes,1));
}
static void check(const StoryTextState *s){
    assert(active->text_file_position==s->pointer&&active->text_offset==s->offset);
    assert(active->text_more==s->more&&active->text_style==s->style&&!memcmp(active->text_rect,s->rect,8));
}
static void text_present(SDL_Renderer *renderer){
    story_ignored_present();
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];char path[180];
    if(!pages)snprintf(path,sizeof(path),"tmp/native-story-4-page-0.idx");
    else{assert(pages==1&&!current->closes);
#ifdef NPE_STORY_IGNORED_KEYS
        snprintf(path,sizeof(path),"tmp/story-ignored-original/%s-0.idx",current->name);
#else
        snprintf(path,sizeof(path),"tmp/native-story-text-%s-0.idx",current->name);
#endif
    }
    FILE *in=fopen(path,"rb");assert(in);
    assert(fread(native,1,sizeof(native),in)==sizeof(native)&&fgetc(in)==EOF&&!fclose(in));
    SDL_RenderPresent(renderer);assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned bad=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        bad+=last_pixel_frame[i]!=v;
        bad+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));
    }
    fprintf(stderr,"SDL live text %s page%u: %u differences\n",current->name,pages,bad);assert(!bad);
    if(pages)check(&current->after);
    ++pages;
}
static int text_wait(SDL_Event *event){
    memset(event,0,sizeof(*event));assert(pages&&!finished);
    if(!patched){
        patch(&current->before);check(&current->before);reads_before=npe_rpg_runtime_reads(active_file);patched=1;
    }
    if(story_ignored_input(event,active,active_file))return 1;
    if(!waits++){event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_SPACE;}
    else{check(&current->after);event->type=SDL_QUIT;}
    return 1;
}
static int text_finish(const NpeFile *file,NpeWorld *world){
    assert(!story_ignored_pending);
    assert(current->closes&&pages==1&&world==active);
    assert(npe_world_finish_story(file,world)==1);check(&current->after);
    assert(world->game_mode==1&&world->story_event_id==1&&npe_rpg_runtime_reads(file)==reads_before);
    ++finished;return 0;
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("resident text",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    NpeWorld world;active=&world;active_file=&file;unsigned total=0;
    for(unsigned i=0;i<sizeof(story_text_cases)/sizeof(*story_text_cases);++i){current=story_text_cases+i;
        assert(!npe_world_init(&file,&world,9,29,13)&&!npe_world_resources_open(&file,&world));
        world.game_mode=5;world.story_event_id=4;pages=finished=waits=patched=0;story_ignored_reset();
        show_walk_story(window,renderer,texture,&file,&font,&world);
        assert(pages==(current->closes?1u:2u)&&finished==current->closes);total+=pages;
        npe_rpg_runtime_close(&file);SDL_Event event;while(SDL_PollEvent(&event)){}
    }
    printf("%u actual SDL live text paths, %u reader LCD comparisons, exact more==1 and host cancellation passed\n",(unsigned)(sizeof(story_text_cases)/sizeof(*story_text_cases)),total);
    story_ignored_report();
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
