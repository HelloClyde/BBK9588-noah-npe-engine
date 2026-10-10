#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
static int npc_wait(SDL_Event *);
static void npc_present(SDL_Renderer *);
static int npc_load(const NpeFile *,NpeWorld *);
#define SDL_WaitEvent npc_wait
#define SDL_RenderPresent npc_present
#define npe_world_prepare_mode2 npc_load
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef SDL_RenderPresent
#undef npe_world_prepare_mode2
#include "npc_text_probe_helpers.h"
static const NpcTextCase *current;
static NpeWorld *active;
static NpeFile *active_file;
static unsigned pages,waits,restores,loads,reads_before,patched;
static uint8_t bitmap_before[32];
static int npc_load(const NpeFile *file,NpeWorld *world){
    uint8_t bitmap[32];assert(world==active);
    assert(!npe_rpg_runtime_read(file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32)&&memcmp(bitmap,bitmap_before,32));
    ++loads;return npe_world_prepare_mode2(file,world);
}
static void npc_present(SDL_Renderer *renderer){
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];char path[200];
    if(pages<current->count){
        snprintf(path,sizeof(path),NPC_TEXT_FRAME_PREFIX "%s-page-%u.idx",current->name,pages);
        assert(!npc_state_differences(active,current->pages+pages,current->npc));++pages;
    }else if(current->patch&&!current->closes){
        assert(!current->shortcut&&!current->ignored&&patched&&pages==current->count);
        snprintf(path,sizeof(path),NPC_TEXT_FRAME_PREFIX "%s-continue-0.idx",current->name);
        assert(!npc_state_differences(active,&current->after,current->npc));++pages;
    }else{
        assert(active->map_init_pending&&active->game_mode==2&&active->text_more!=1);
        /* The more2 sample records the original restore submission itself.
           Other normal closes restore the sampled original input plane. */
        snprintf(path,sizeof(path),current->closes?NPC_TEXT_FRAME_PREFIX "%s-continue-0.idx":
                 NPC_TEXT_FRAME_PREFIX "%s-before.idx",current->name);++restores;
    }
    npc_frame(path,native);SDL_RenderPresent(renderer);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned bad=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];
        bad+=last_pixel_frame[i]!=v;
        bad+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));
    }
    fprintf(stderr,"SDL NPC %s pages%u restore%u: %u differences\n",current->name,pages,restores,bad);assert(!bad);
}
static int npc_wait(SDL_Event *event){
    memset(event,0,sizeof(*event));assert(pages&&!restores);
    if(current->patch){
        if(!waits++){
            npc_patch(active_file,active,&current->patch_before,current->npc);patched=1;
            reads_before=npe_rpg_runtime_reads(active_file);event->type=SDL_KEYDOWN;
            event->key.keysym.sym=current->key==0x86?SDLK_RETURN:SDLK_SPACE;
#ifdef NPE_NPC_IGNORED_KEYS
            assert(current->key==0x71||current->key==0x51);
            event->key.keysym.sym=SDLK_q;
            event->key.keysym.mod=current->key==0x51?KMOD_SHIFT:KMOD_NONE;
#endif
        }else{
            assert(!current->closes&&!current->shortcut);
            assert(!npc_state_differences(active,&current->after,current->npc));
            if(current->ignored)assert(npe_rpg_runtime_reads(active_file)==reads_before);
            event->type=SDL_QUIT;
        }
    }else{event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_SPACE;++waits;}
    return 1;
}
int main(int argc,char **argv){assert(argc==2);NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("NPC text",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    NpeFont font={0};assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    NpeWorld world;active=&world;active_file=&file;unsigned total=0,restore_total=0;
    for(unsigned i=0;i<sizeof(npc_text_cases)/sizeof(*npc_text_cases);++i){current=npc_text_cases+i;
        assert(!npe_world_init(&file,&world,1,24,9)&&!npe_world_resources_open(&file,&world));
        npc_patch(&file,&world,&current->before,current->npc);
        if(i==1){world.mode2_ready=1;world.mode2_dialogue_offset=0xffffffffu;}
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap_before,32));
        char path[180];snprintf(path,sizeof(path),NPC_TEXT_FRAME_PREFIX "%s-before.idx",current->name);
        npc_frame(path,last_pixel_frame);last_pixel_renderer=renderer;pages=waits=restores=loads=patched=0;
        int result=show_dialogue_pages(window,renderer,texture,&file,&world,&font,0);
        assert(loads==1&&pages==current->count+(current->patch&&!current->closes&&!current->shortcut&&!current->ignored));
        if(current->patch&&!current->closes&&!current->shortcut){assert(result==-1&&!restores);}
        else{
            assert(!result&&world.map_init_pending&&restores==!current->shortcut);
            if(current->patch)assert(npe_rpg_runtime_reads(&file)==reads_before);
            if(current->shortcut)assert(!world.text_more);
            assert(npe_world_finish_mode2(&world)==1&&!world.mode2_ready);
            if(current->patch){NpcTextState completed=current->after;
                /* Original controller's next map/shop call clears mapInit;
                   this isolated main function ends before that next call. */
                completed.map_init=1;assert(!npc_state_differences(&world,&completed,current->npc));}
        }
        total+=pages;restore_total+=restores;uint8_t bitmap[32];
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32)&&!memcmp(bitmap,bitmap_before,32));
        npe_rpg_runtime_close(&file);SDL_Event event;while(SDL_PollEvent(&event)){}
    }
    printf("%zu actual main NPC paths: %u original page comparisons, %u restores, exact more==1/ignored Enter/shop shortcut/host cancellation passed\n",sizeof(npc_text_cases)/sizeof(*npc_text_cases),total,restore_total);
    npe_font_free(&font);npe_free(&file);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
