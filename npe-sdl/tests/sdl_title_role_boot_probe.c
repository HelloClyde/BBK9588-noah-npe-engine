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
#include "title_load_native.h"
typedef struct {
 const char *route;unsigned salt,map;
 uint8_t before_battle[888],before_map[444],after_battle[888],after_map[444],saved_map[444];
} RoleLifetimeCase;
#include "title_role_boot_native.h"
static unsigned matrix_case;
static NpeWorld *live;
static unsigned phase,submits,active;
static int probe_wait(SDL_Event *,int);
static void probe_present(SDL_Renderer *);
static void probe_sound(NpeSoundPlayer *,NpeWorld *);
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/title-role-boot-");}
static Uint32 probe_ticks(void){return 0;}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetPrefPath probe_pref
#define SDL_GetTicks probe_ticks
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout probe_wait
#define SDL_RenderPresent probe_present
#define npe_sound_select probe_sound
#include "scene_wait_input.h"
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetPrefPath
#undef SDL_GetTicks
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef SDL_PollEvent
#undef npe_sound_select
static void probe_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);}
static void npc_state(const NpeFile *f,const NpeWorld *w,const unsigned expected[20][4]){
    NpeMap map;assert(!npe_map(f,w->map_number-1,&map));
    for(unsigned i=0;i<20&&expected[i][0];++i){const uint8_t *npc;unsigned x,y;
        assert(!npe_map_npc(&map,i,&npc));npe_world_npc_position(w,npc,i,&x,&y);
        unsigned direction=w->npc_override_direction[i]!=255?w->npc_override_direction[i]:npc[6];
        fprintf(stderr,"NPC%u id%u C=%u,%u,%u native=%u,%u,%u\n",i,expected[i][0],x,y,direction,expected[i][1],expected[i][2],expected[i][3]);
        assert(((unsigned)npc[0]*256+npc[1])==expected[i][0]&&x==expected[i][1]&&y==expected[i][2]&&direction==expected[i][3]);
    }
}
static void state(void){const unsigned *s=title_load_position;
    assert(live->game_mode==s[0]&&live->story_event_id==s[1]&&live->map_number==s[2]&&live->story_progress==s[3]&&
           live->view.screen_x==s[4]&&live->view.screen_y==s[5]&&live->view.offset_x==s[6]&&live->view.offset_y==s[7]&&
           live->direction==s[8]&&live->money==s[9]&&live->x==s[4]+s[6]&&live->y==s[5]+s[7]);
    npc_state((const NpeFile *)live->battle_file,live,title_load_loaded_npc);
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const char *labels[]={"title-down","slots","load"};char path[160];
    assert(phase<3&&!submits);if(phase==2)snprintf(path,sizeof(path),"tmp/title-role-boot-native/%u-0.idx",title_role_boot_cases[matrix_case].salt);
    else snprintf(path,sizeof(path),"tmp/native-title-reload-%s-commit-0.idx",labels[phase]);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    FILE *in=fopen(path,"rb");assert(in&&fread(native,1,sizeof(native),in)==sizeof(native)&&!fclose(in));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"title load phase%u C=%u SDL=%u\n",phase,d,sd);
    if(d){FILE *out=fopen("tmp/title-role-boot-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++submits;
}
static int probe_wait(SDL_Event *event,int timeout){(void)timeout;memset(event,0,sizeof(*event));
    if(!active){assert(live->game_mode==7);active=1;phase=submits=0;}
    else{assert(submits==1);submits=0;++phase;}
    if(phase==3){state();
        uint8_t battle[888],map[444];const RoleLifetimeCase *c=&title_role_boot_cases[matrix_case];
        assert(!npe_rpg_runtime_read((const NpeFile *)live->battle_file,0x526450,battle,888));
        assert(!npe_rpg_runtime_read((const NpeFile *)live->battle_file,0x5269c6,map,444));
        assert(!memcmp(battle,c->after_battle,888)&&!memcmp(map,c->after_map,444));
        active=0;event->type=SDL_QUIT;return 1;
    }
    if(phase==2){const RoleLifetimeCase *c=&title_role_boot_cases[matrix_case];
        assert(!npe_world_resident_records_write(live,0x526450,c->before_battle,888));
        assert(!npe_world_resident_records_write(live,0x5269c6,c->before_map,444));
        live->exit_choice=1;
    }
    event->type=SDL_KEYDOWN;event->key.keysym.sym=phase?SDLK_RETURN:SDLK_DOWN;return 1;
}
int main(int argc,char **argv){
    assert(argc==2);
    (void)title_load_saved_npc;
    for(matrix_case=0;matrix_case<sizeof(title_role_boot_cases)/sizeof(*title_role_boot_cases);++matrix_case){
        const RoleLifetimeCase *c=&title_role_boot_cases[matrix_case];
        NpeFile file;NpeWorld saved;uint8_t *bytes;size_t size;char error[128];
        assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
        assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
        assert(!npe_world_new_game(&file,&saved));
        assert(!npe_world_resident_records_write(&saved,0x5269c6,c->saved_map,444));
        char *path=game_save_path(&file,0);assert(path&&!npe_world_save(&file,&saved,path));
        phase=submits=active=0;live=NULL;
        char *args[]={"npe-sdl",argv[1],NULL};assert(!run(2,args)&&phase==3);
        assert(!remove(path));free(path);npe_free(&file);
        printf("Original title boot salt%u: complete888+444 bytes and3 C/SDL submits matched\n",c->salt);
    }
    return 0;
}
