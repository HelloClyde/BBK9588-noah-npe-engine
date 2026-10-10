#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned id,x,y,direction;} ScriptNpc;
typedef struct {const char *name;unsigned x,y,target_x,target_y,commits;uint32_t seed;
    unsigned mode,event,progress,input_event,next_mode,next_id,nonterminal,wait_steps;uint32_t rng;ScriptNpc npc[20];} NpcScriptCase;
#include "npc_move_byte_native.h"
static NpeWorld *live;
static unsigned case_index,commits,total;
static Uint32 fake_clock;
static int active,started,abort_sent;
static void codec_checks(const NpeFile *file,const NpeWorld *baseline){
    enum{NPC_OFFSET=36+13+256+510+18+57+200+8+256+6+12+45+42};
    const char *path="tmp/npc-move-byte-codec.npes";NpeWorld w=*baseline,loaded;
    w.game_mode=1;
    const unsigned positions[][2]={{255,9},{6,255},{255,255}};
    for(unsigned i=0;i<3;++i){w.npc_override_x[0]=(uint16_t)positions[i][0];w.npc_override_y[0]=(uint16_t)positions[i][1];
        assert(!npe_world_save(file,&w,path)&&!npe_world_load(file,&loaded,path));
        assert(loaded.npc_override_x[0]==positions[i][0]&&loaded.npc_override_y[0]==positions[i][1]);}
    uint8_t original[4096],bytes[4096];FILE *f=fopen(path,"rb");assert(f);
    size_t size=fread(original,1,sizeof(original),f);assert(!fclose(f)&&size>40&&!memcmp(original,"NPESAV22",8));
    for(unsigned bad=0;bad<4;++bad){memcpy(bytes,original,size);size_t n=size;
        if(bad<2){
            bytes[size-134-NPE_CHARACTER_RECORD_SAVE_BYTES+(bad?0:2)]^=(uint8_t)(bad?1:16); /* missing valid bit / reserved bit20 */
            if(bad==1)bytes[NPC_OFFSET+1]=9; /* FF/FF with a clear bit is a valid absent pair. */
        }else {memcpy(bytes,"NPESAV16",8);n-=134+NPE_CHARACTER_RECORD_SAVE_BYTES;if(bad==3){
            /* A representable V16 override remains a position. */
            bytes[NPC_OFFSET]=3;bytes[NPC_OFFSET+1]=4;}}
        uint64_t hash=UINT64_C(14695981039346656037);
        for(size_t j=36;j<n;++j)hash=(hash^bytes[j])*UINT64_C(1099511628211);
        for(unsigned j=0;j<4;++j)bytes[24+j]=(uint8_t)((n-36)>>(8*j));
        for(unsigned j=0;j<8;++j)bytes[28+j]=(uint8_t)(hash>>(8*j));
        f=fopen(path,"wb");assert(f&&fwrite(bytes,1,n,f)==n&&!fclose(f));
        if(bad<2){NpeWorld before=loaded;
            assert(npe_world_load(file,&loaded,path)==-1&&!memcmp(&before,&loaded,sizeof(loaded)));
            assert(npe_world_restore(file,&loaded,path)==-1&&!memcmp(&before,&loaded,sizeof(loaded)));
        }else{assert(!npe_world_load(file,&loaded,path));
            assert(loaded.npc_override_x[0]==(bad==3?3:NPE_NPC_POSITION_NONE)&&
                loaded.npc_override_y[0]==(bad==3?4:NPE_NPC_POSITION_NONE));}
    }
    w.npc_override_x[0]=256;assert(npe_world_save(file,&w,path)==-1&&!remove(path));
    puts("NPC positions:V17 coordinate255, mask validation, failed transactions and V16 sentinel migration passed");
}
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static void present(SDL_Renderer *);
static int wait_event(SDL_Event *,int);
static Uint32 ticks(void){return fake_clock;}
static void delay(Uint32 ms){fake_clock+=ms;}
static int no_input(SDL_Event *e){
    const NpcScriptCase *c=&npc_script_cases[case_index];
    if(active&&c->nonterminal&&commits==c->commits&&!abort_sent){
        abort_sent=1;active=0;memset(e,0,sizeof(*e));e->type=SDL_QUIT;return 1;}
    return 0;
}
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 f){
    (void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,f);}
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_PollEvent no_input
#define SDL_WaitEventTimeout wait_event
#define SDL_RenderPresent present
#define SDL_CreateWindow probe_window
#define npe_sound_select select_sound
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef npe_sound_select
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){
    if(!live){
        const NpcScriptCase *c=&npc_script_cases[case_index];live=w;
        NpeFile *file=(NpeFile *)w->battle_file;NpeMap map;
        assert(!npe_map(file,0,&map));
        unsigned slot=(c->input_event&255)==5?4:0;
        w->npc_move_events[slot]=(NpeNpcMoveCache){.npc_id=1,.x=(uint16_t)c->target_x,
            .y=(uint16_t)c->target_y,.next_mode=(uint8_t)c->next_mode,.next_id=(uint16_t)c->next_id};
        for(unsigned i=0;i<20;++i){const uint8_t *npc;assert(!npe_map_npc(&map,i,&npc));((uint8_t *)npc)[7]=0;}
        w->npc_override_x[0]=(uint8_t)c->x;w->npc_override_y[0]=(uint8_t)c->y;
        w->direction=1;w->music_enabled=0;w->random_state=c->seed;
        if(!case_index)codec_checks(file,w);
        {
            NpeWorld numeric=*w;numeric.game_mode=18;numeric.story_event_id=(uint16_t)c->input_event;
            if(c->nonterminal){NpeWorld before=numeric;
                assert(npe_world_apply_npc_move_event(file,&numeric)==NPE_NPC_MOVE_NONTERMINATING&&
                    !memcmp(&before,&numeric,sizeof(numeric)));
            }else{
            assert(npe_world_apply_npc_move_event(file,&numeric)==1);
            assert(numeric.game_mode==c->mode&&numeric.story_event_id==c->event&&numeric.random_state==c->seed);
            unsigned nx,ny;const uint8_t *npc;assert(!npe_map_npc(&map,0,&npc));
            npe_world_npc_position(&numeric,npc,0,&nx,&ny);
            assert(nx==c->npc[0].x&&ny==c->npc[0].y);
            if(c->target_x>=map.width || c->target_y>=map.height){
                const char *path="tmp/npc-move-dispatch-outside.npes";NpeWorld cold,warm=*w;
                assert(!npe_world_save(file,&numeric,path)&&!npe_world_load(file,&cold,path));
                npe_world_npc_position(&cold,npc,0,&nx,&ny);
                assert(nx==c->npc[0].x&&ny==c->npc[0].y);
                assert(!npe_world_restore(file,&warm,path));npe_world_npc_position(&warm,npc,0,&nx,&ny);
                assert(nx==npc[2]&&ny==npc[3]&&!remove(path));
            }
            }
        }
    }
    npe_sound_select(p,w);SDL_PauseAudioDevice(p->device,1);
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const NpcScriptCase *c=&npc_script_cases[case_index];assert(commits<c->commits);
    char path[160];snprintf(path,sizeof(path),"tmp/native-npc-move-byte-%u-%u.idx",case_index,commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"NPC script %s commit%u C=%u SDL=%u time%u\n",c->name,commits,d,sd,fake_clock);
    if(d){FILE *out=fopen("tmp/npc-move-byte-actual.idx","wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);++commits;++total;
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;memset(e,0,sizeof(*e));
    if(!started){started=1;live->game_mode=18;live->story_event_id=(uint16_t)npc_script_cases[case_index].input_event;active=1;
        e->type=SDL_KEYDOWN;e->key.keysym.sym=SDLK_F12;return 1;}
    const NpcScriptCase *c=&npc_script_cases[case_index];assert(commits==c->commits);
    assert(live->game_mode==c->mode&&live->story_event_id==c->event&&live->story_progress==c->progress&&live->random_state==c->rng);
    const NpeFile *file=(const NpeFile *)live->battle_file;NpeMap map;assert(!npe_map(file,0,&map));
    for(unsigned i=0;i<20;++i){const uint8_t *npc;unsigned x,y;assert(!npe_map_npc(&map,i,&npc));
        npe_world_npc_position(live,npc,i,&x,&y);unsigned dir=live->npc_override_direction[i]<=3?live->npc_override_direction[i]:npc[6];
        assert(x==c->npc[i].x&&y==c->npc[i].y&&dir==c->npc[i].direction);}
    assert(fake_clock==(c->wait_steps-c->nonterminal)*141u);
    assert(abort_sent==(int)c->nonterminal);
    active=0;e->type=SDL_QUIT;return 1;
}
int main(int argc,char **argv){assert(argc==2);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    for(case_index=0;case_index<sizeof(npc_script_cases)/sizeof(*npc_script_cases);++case_index){
        live=NULL;commits=fake_clock=active=started=abort_sent=0;
        char *args[]={"npe-sdl","--walk-preview","1,7,9,0,5",argv[1],NULL};assert(!run(4,args));
    }
    printf("Original NPC byte movement: %u actual main/SDL commits, %u paths, final NPC state, C RNG preservation and nominal waits passed\n",total,
        (unsigned)(sizeof(npc_script_cases)/sizeof(*npc_script_cases)));
    return 0;
}
