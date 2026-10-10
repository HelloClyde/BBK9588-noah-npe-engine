#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
typedef struct {unsigned mode,event,context,count,wrong,previous,category,type,length,stats[22];uint8_t input[32];} QuizState;
typedef struct {const char *route;unsigned commits;QuizState saved,before,after;} QuizStateCase;
#include "quiz_state_native.h"
static NpeWorld *live;
static unsigned case_index,phase,active,submits,total;
static Uint32 ticks(void){return 0;}
static char *probe_pref(const char *a,const char *b){(void)a;(void)b;return SDL_strdup("tmp/quiz-state-");}
static int wait_event(SDL_Event *,int);
static void present(SDL_Renderer *);
static void select_sound(NpeSoundPlayer *,NpeWorld *);
static int restore(const NpeFile *,NpeWorld *,const char *);
static int restart(const NpeFile *,NpeWorld *);
static SDL_Window *probe_window(const char *s,int x,int y,int w,int h,Uint32 flags){(void)w;(void)h;return SDL_CreateWindow(s,x,y,NPE_WIDTH,NPE_HEIGHT,flags);}
#define SDL_GetTicks ticks
#define SDL_GetPrefPath probe_pref
#define SDL_CreateWindow probe_window
#define SDL_WaitEventTimeout wait_event
#define SDL_RenderPresent present
#define npe_sound_select select_sound
#define npe_world_restore restore
#define npe_world_restart_game restart
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_GetTicks
#undef SDL_GetPrefPath
#undef SDL_CreateWindow
#undef SDL_WaitEventTimeout
#undef SDL_RenderPresent
#undef npe_sound_select
#undef npe_world_restore
#undef npe_world_restart_game
static void configure(NpeWorld *w,const QuizState *s){
    w->event_context_id=(uint16_t)s->context;
    for(unsigned i=0;i<10;++i){w->quiz_category_correct[i]=(uint16_t)s->stats[2*i];w->quiz_category_wrong[i]=(uint16_t)s->stats[2*i+1];}
    w->quiz_fixed_correct=(uint16_t)s->stats[20];w->quiz_fixed_wrong=(uint16_t)s->stats[21];
    w->quiz_answer_count=(uint16_t)s->count;w->quiz_wrong_count=(uint8_t)s->wrong;
    w->quiz_previous_index=(uint16_t)s->previous;w->quiz_category=(uint16_t)s->category;
    w->quiz_input_type=(uint8_t)s->type;w->quiz_input_length=(uint8_t)s->length;
    memcpy(w->quiz_input,s->input,32);w->quiz_input[32]=0;
}
static void check(const NpeWorld *w,const QuizState *s){
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->event_context_id==s->context);
    for(unsigned i=0;i<10;++i)assert(w->quiz_category_correct[i]==s->stats[2*i]&&w->quiz_category_wrong[i]==s->stats[2*i+1]);
    assert(w->quiz_fixed_correct==s->stats[20]&&w->quiz_fixed_wrong==s->stats[21]);
    assert(w->quiz_answer_count==s->count&&w->quiz_wrong_count==s->wrong&&w->quiz_previous_index==s->previous&&
        w->quiz_category==s->category&&w->quiz_input_type==s->type&&w->quiz_input_length==s->length&&
        !memcmp(w->quiz_input,s->input,32)&&!w->quiz_input[32]&&!w->quiz_ready);
}
static void select_sound(NpeSoundPlayer *p,NpeWorld *w){live=w;npe_sound_select(p,w);SDL_PauseAudioDevice(p->device,1);}
static int restore(const NpeFile *file,NpeWorld *w,const char *path){
    configure(w,&quiz_state_cases[case_index].before);active=1;
    int result=npe_world_restore(file,w,path);
    if(strcmp(quiz_state_cases[case_index].route,"empty")) {assert(!result);check(w,&quiz_state_cases[case_index].after);}
    return result;
}
static int restart(const NpeFile *file,NpeWorld *w){
    /* Empty title slots now follow the native constructor directly. The
       original quiz fixture starts at that constructor, after chooser clear,
       rather than at an attempted host restore of a nonexistent save. */
    if(!strcmp(quiz_state_cases[case_index].route,"empty")){
        configure(w,&quiz_state_cases[case_index].before);active=1;
    }
    return npe_world_restart_game(file,w);
}
static void present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const QuizStateCase *c=&quiz_state_cases[case_index];assert(submits<c->commits);
    char path[180];snprintf(path,sizeof(path),"tmp/native-quiz-state-%s-%u.idx",c->route,submits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];FILE *f=fopen(path,"rb");
    assert(f&&fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"Quiz lifetime %s commit%u C=%u SDL=%u\n",c->route,submits,d,sd);assert(!d&&!sd);++submits;++total;
}
static int wait_event(SDL_Event *e,int timeout){(void)timeout;memset(e,0,sizeof(*e));
    const QuizStateCase *c=&quiz_state_cases[case_index];
    if(active){assert(submits==c->commits);check(live,&c->after);active=0;e->type=SDL_QUIT;return 1;}
    if(!phase++){live->music_enabled=0;live->lcd_background=!strncmp(c->route,"cold",4)?2:0;
        if(strcmp(c->route,"system"))live->game_mode=7;e->type=SDL_USEREVENT;return 1;}
    e->type=SDL_KEYDOWN;
    if(!strcmp(c->route,"new")) {configure(live,&c->before);active=1;e->key.keysym.sym=SDLK_RETURN;return 1;}
    const SDL_Keycode system[]={SDLK_F4,SDLK_DOWN,SDLK_DOWN,SDLK_DOWN,SDLK_RETURN,SDLK_RETURN,SDLK_RETURN};
    const SDL_Keycode empty[]={SDLK_DOWN,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN};
    const SDL_Keycode title[]={SDLK_DOWN,SDLK_RETURN,SDLK_RETURN};
    const SDL_Keycode *keys=!strcmp(c->route,"system")?system:!strcmp(c->route,"empty")?empty:title;
    unsigned count=!strcmp(c->route,"system")?7:!strcmp(c->route,"empty")?4:3;
    assert(phase-2<count);e->key.keysym.sym=keys[phase-2];return 1;
}
static uint64_t checksum(const uint8_t *b,size_t n){uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;++i)h=(h^b[i])*UINT64_C(1099511628211);return h;}
static void put_le(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)p[i]=(uint8_t)(v>>(8*i));}
static void write_bytes(const char *path,uint8_t *bytes,size_t size){put_le(bytes+24,size-36,4);put_le(bytes+28,checksum(bytes+36,size-36),8);FILE *f=fopen(path,"wb");assert(f&&fwrite(bytes,1,size,f)==size&&!fclose(f));}
static void codecs(NpeFile *file){
    const char *path="tmp/quiz-state-codec.npes";NpeWorld w,cold;
    assert(!npe_world_new_game(file,&w));configure(&w,&quiz_state_cases[0].saved);
    assert(!npe_world_save(file,&w,path)&&!npe_world_load(file,&cold,path));
    for(unsigned i=0;i<10;++i)assert(cold.quiz_category_correct[i]==w.quiz_category_correct[i]&&cold.quiz_category_wrong[i]==w.quiz_category_wrong[i]);
    assert(cold.quiz_fixed_correct==w.quiz_fixed_correct&&cold.quiz_fixed_wrong==w.quiz_fixed_wrong&&
        !cold.quiz_answer_count&&!cold.quiz_wrong_count&&!cold.quiz_previous_index&&!cold.quiz_input_length);
    uint8_t bytes[4096];FILE *f=fopen(path,"rb");assert(f);size_t size=fread(bytes,1,sizeof(bytes),f);assert(!fclose(f)&&!memcmp(bytes,"NPESAV22",8));
    /* Current V22 writes a trailing walk-table ownership tag. Remove it
       before constructing historical V18 payloads; it was never V18 data. */
    assert(size>=4&&!memcmp(bytes+size-4,"WTBL",4));size-=4;
    /* V19 appends32 tile bytes; the V18 prefix remains unchanged. */
    size-=128+NPE_CHARACTER_RECORD_SAVE_BYTES;memcpy(bytes,"NPESAV18",8);write_bytes(path,bytes,size);
    assert(!npe_world_load(file,&cold,path));
    /* V15 prefix has 10 interleaved pairs and five obsolete session bytes. */
    enum { QUIZ_OFFSET=36+13+256+510+18+57+200+8+256+6+12 };
    assert(bytes[QUIZ_OFFSET]==100&&bytes[QUIZ_OFFSET+20]==170);
    uint8_t original[4096];memcpy(original,bytes,size);
    memcpy(bytes,"NPESAV17",8);write_bytes(path,bytes,size-2);
    assert(!npe_world_load(file,&cold,path)&&cold.quiz_fixed_correct==w.quiz_fixed_correct&&cold.quiz_fixed_wrong==w.quiz_fixed_wrong);
    memcpy(bytes,original,size);
    memcpy(bytes,"NPESAV16",8);write_bytes(path,bytes,size-6);
    assert(!npe_world_load(file,&cold,path)&&cold.quiz_fixed_correct==w.quiz_fixed_correct&&cold.quiz_fixed_wrong==w.quiz_fixed_wrong);
    memcpy(bytes,original,size);
    size_t old_size=size-10;memcpy(bytes,"NPESAV15",8);
    put_le(bytes+QUIZ_OFFSET+20,61234,2);put_le(bytes+QUIZ_OFFSET+22,65535,2);
    put_le(bytes+QUIZ_OFFSET+40,321,2);put_le(bytes+QUIZ_OFFSET+42,2,1);put_le(bytes+QUIZ_OFFSET+43,1780,2);
    write_bytes(path,bytes,old_size);
    assert(!npe_world_load(file,&cold,path)&&cold.quiz_fixed_correct==61234&&cold.quiz_fixed_wrong==65535&&
        !cold.quiz_category_correct[5]&&!cold.quiz_category_wrong[5]&&!cold.quiz_answer_count&&!cold.quiz_wrong_count&&!cold.quiz_previous_index);
    for(unsigned i=0;i<10;++i)if(i!=5)assert(cold.quiz_category_correct[i]==w.quiz_category_correct[i]&&cold.quiz_category_wrong[i]==w.quiz_category_wrong[i]);
    /* Exercise each accepted historical prefix without the broad story walk. */
    static const unsigned trim[16]={0,0,12,45,42,40,6,20,57,0,0,0,0,10,182,6};
    size_t prefix=old_size;
    for(unsigned version=15;version>=1;--version){
        char magic[9];snprintf(magic,sizeof(magic),"NPESAV%02u",version);memcpy(bytes,magic,8);
        write_bytes(path,bytes,prefix);assert(!npe_world_load(file,&cold,path));
        assert(!cold.quiz_answer_count&&!cold.quiz_wrong_count&&!cold.quiz_previous_index&&
            !cold.quiz_category&&!cold.quiz_input_type&&!cold.quiz_input_length&&!cold.quiz_input[0]);
        if(version>=3){assert(cold.quiz_fixed_correct==61234&&cold.quiz_fixed_wrong==65535&&
            !cold.quiz_category_correct[5]&&!cold.quiz_category_wrong[5]);
            for(unsigned i=0;i<10;++i)if(i!=5)assert(cold.quiz_category_correct[i]==w.quiz_category_correct[i]&&cold.quiz_category_wrong[i]==w.quiz_category_wrong[i]);
        }else {assert(!cold.quiz_fixed_correct&&!cold.quiz_fixed_wrong);
            for(unsigned i=0;i<10;++i)assert(!cold.quiz_category_correct[i]&&!cold.quiz_category_wrong[i]);}
        assert(cold.map_number==w.map_number&&cold.x==w.x&&cold.y==w.y&&cold.money==w.money);
        prefix-=trim[version];
    }
    /* A nonempty category5 makes old merged counts impossible to invert.
       Preserve the aggregate in both fields; all later updates are separate. */
    uint8_t *changed=malloc(file->size);assert(changed);memcpy(changed,file->bytes,file->size);
    NpeFile other=*file;other.bytes=changed;size_t counts=file->sections[15].offset+2;
    assert(!changed[counts+10]&&!changed[counts+11]);
    changed[counts+10]=changed[counts];changed[counts+11]=changed[counts+1];changed[counts]=changed[counts+1]=0;
    unsigned first,count;assert(!npe_quiz_category_range(&other,5,&first,&count)&&count);
    memcpy(bytes,original,size);memcpy(bytes,"NPESAV15",8);
    put_le(bytes+16,npe_game_fingerprint(&other),8);put_le(bytes+QUIZ_OFFSET+20,61234,2);put_le(bytes+QUIZ_OFFSET+22,65535,2);
    write_bytes(path,bytes,old_size);assert(!npe_world_load(&other,&cold,path)&&
        cold.quiz_category_correct[5]==61234&&cold.quiz_category_wrong[5]==65535&&
        cold.quiz_fixed_correct==61234&&cold.quiz_fixed_wrong==65535);
    assert(!npe_world_save(&other,&cold,path));NpeWorld independent;
    assert(!npe_world_load(&other,&independent,path)&&independent.quiz_category_correct[5]==61234&&independent.quiz_fixed_correct==61234);
    free(changed);
    /* A shortened V18 tail or a trailing byte must fail transactionally. */
    NpeWorld before=cold;memcpy(bytes,original,size);write_bytes(path,bytes,size-1);
    assert(npe_world_load(file,&cold,path)==-1&&!memcmp(&before,&cold,sizeof(cold)));
    memcpy(bytes,original,size);bytes[size]=0;write_bytes(path,bytes,size+1);
    assert(npe_world_load(file,&cold,path)==-1&&!memcmp(&before,&cold,sizeof(cold)));
    assert(!remove(path));puts("V18 independent22-word statistics; V1..17 prefixes, empty/nonempty-category5 migration, obsolete sessions and malformed-tail transactions passed");
}
int main(int argc,char **argv){assert(argc==2);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    for(unsigned i=0;i<5;++i)for(unsigned mods=0;mods<4;++mods){
        SDL_KeyboardEvent key={0};key.keysym.sym="ijklm"[i];
        key.keysym.mod=(mods&1?KMOD_SHIFT:0)|(mods&2?KMOD_CAPS:0);
        assert(quiz_sdl_key(&key)==(unsigned)("ijklm"[i]-((mods==1||mods==2)?32:0)));
    }
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));codecs(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_setenv("SDL_AUDIODRIVER","dummy",1));
    char *paths[2]={game_save_path(&file,0),game_save_path(&file,1)};assert(paths[0]&&paths[1]&&strstr(paths[0],"tmp/quiz-state-")==paths[0]&&strstr(paths[1],"tmp/quiz-state-")==paths[1]);remove(paths[1]);
    for(case_index=0;case_index<sizeof(quiz_state_cases)/sizeof(*quiz_state_cases);++case_index){
        const QuizStateCase *c=&quiz_state_cases[case_index];NpeWorld saved;
        assert(!npe_world_new_game(&file,&saved));configure(&saved,&c->saved);assert(!npe_world_save(&file,&saved,paths[0]));
        if(!strncmp(c->route,"cold",4)){NpeWorld cold;assert(!npe_world_load(&file,&cold,paths[0]));check(&cold,&c->after);}
        phase=active=submits=0;char *args[]={"npe-sdl","--new-game",argv[1],NULL};assert(!run(3,args));
    }
    assert(!remove(paths[0]));free(paths[0]);free(paths[1]);npe_free(&file);
    assert(total==10);puts("Six original UI quiz-state lifetimes:22 independent counters, session retention and10 actual main/SDL map commits matched");return 0;
}
