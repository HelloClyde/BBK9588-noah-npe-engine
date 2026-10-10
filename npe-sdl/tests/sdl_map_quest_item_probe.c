#include <assert.h>
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "../text.h"
#include "map_quest_native.h"
static SDL_atomic_t ready,prompt_visible;
static const char *prompt_reference;
static Uint32 prompt_started;
static int probe_wait(SDL_Event *event){SDL_AtomicSet(&ready,1);return SDL_WaitEvent(event);}
static int probe_rejection(const NpeFont *,unsigned,uint8_t *);
static int checking_story;
static const char *story_variant;
static unsigned story_pages;
static int probe_dialogue(const NpeFont *,const uint8_t *,size_t,uint8_t *);
static int probe_window_story(const NpeFont *,const NpeStoryRecord *,size_t,size_t *,uint8_t *);
#define SDL_WaitEvent probe_wait
#define npe_render_map_item_rejection probe_rejection
#define npe_render_native_dialogue probe_dialogue
#define npe_render_native_window_story_page probe_window_story
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_WaitEvent
#undef npe_render_map_item_rejection
#undef npe_render_native_dialogue
#undef npe_render_native_window_story_page

static uint16_t get16(const uint8_t *p){return (uint16_t)((unsigned)p[0]<<8|p[1]);}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static unsigned screen_count;
static void compare(const char *path,const uint8_t *pixels){
    FILE *stream=fopen(path,"rb");assert(stream);unsigned differences=0;
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){int c=fgetc(stream);assert(c>=0);differences+=pixels[i]!=(uint8_t)c;}
    assert(fgetc(stream)==EOF&&fclose(stream)==0);
    fprintf(stderr,"Original quest screen %s: %u differences\n",path,differences);
    if(differences){stream=fopen("tmp/quest-item-actual.idx","wb");assert(stream);assert(fwrite(pixels,1,NPE_WIDTH*NPE_HEIGHT,stream)==NPE_WIDTH*NPE_HEIGHT&&fclose(stream)==0);}
    assert(!differences);++screen_count;
}
static int probe_rejection(const NpeFont *font,unsigned rejection,uint8_t *pixels){
    int result=npe_render_map_item_rejection(font,rejection,pixels);
    if(prompt_reference){assert(!result&&rejection==2);compare(prompt_reference,pixels);prompt_started=SDL_GetTicks();SDL_AtomicSet(&prompt_visible,1);}
    return result;
}
static int probe_dialogue(const NpeFont *font,const uint8_t *text,size_t size,uint8_t *pixels){
    int result=npe_render_native_dialogue(font,text,size,pixels);
    if(checking_story){
        const char *labels[]={"settled","follow0","follow1"};char path[160];
        assert(story_pages<3&&!result);snprintf(path,sizeof(path),"tmp/quest-item-%s-%s.idx",story_variant,labels[story_pages++]);compare(path,pixels);
    }
    return result;
}
static int probe_window_story(const NpeFont *font,const NpeStoryRecord *story,size_t start,size_t *next,uint8_t *pixels){
    int result=npe_render_native_window_story_page(font,story,start,next,pixels);
    if(checking_story){
        const char *labels[]={"settled","follow0","follow1"};char path[160];
        assert(story_pages<3&&!result);snprintf(path,sizeof(path),"tmp/quest-item-%s-%s.idx",story_variant,labels[story_pages++]);compare(path,pixels);
    }
    return result;
}
static void await_flag(SDL_atomic_t *flag){for(unsigned elapsed=0;!SDL_AtomicGet(flag);++elapsed){assert(elapsed<8000);SDL_Delay(1);}}
static void key(SDL_Keycode value){SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=value;assert(SDL_PushEvent(&event)==1);}
static int send_rejected(void *unused){
    (void)unused;await_flag(&ready);SDL_AtomicSet(&ready,0);key(SDLK_RETURN);await_flag(&prompt_visible);
    SDL_Delay(30);key(SDLK_RETURN);await_flag(&ready);
    assert(SDL_GetTicks()-prompt_started<2000);key(SDLK_F4);return 0;
}
static int send_success(void *unused){(void)unused;await_flag(&ready);key(SDLK_RETURN);return 0;}
static int send_story(void *unused){
    /* NPC window dialogue advances with the original confirm key (Space).
       Return is a separate key in the retained mode2 service. */
    (void)unused;for(unsigned i=0;i<3;++i){await_flag(&ready);SDL_AtomicSet(&ready,0);key(SDLK_SPACE);}return 0;
}
/* Build a valid independent NPE in memory: insert group7 rows in map1,
   fix its group offsets, the map directory and subsequent section ranges.
   Resources are unchanged; no original disk file is written. */
static NpeFile fixture_file(const NpeFile *original,const MapQuestNative *sample){
    NpeMap map;assert(npe_map(original,0,&map)==0);const uint8_t *index=map.tail+652;
    unsigned counts[17],absolute[17],all_zero=1;size_t offsets[17];
    const unsigned sizes[]={0,13,10,7,8,10,7,13,11,9,10,7,8,8,11,7};
    for(unsigned i=0;i<17;++i){counts[i]=index[3*i+2];absolute[i]=get16(index+3*i);if(absolute[i])all_zero=0;}
    assert(!counts[7]);
    if(all_zero){size_t used=map.tail_size-703;for(unsigned i=1;i<16;++i)used-=counts[i]*sizes[i];offsets[0]=0;
        for(unsigned i=1;i<17;++i){offsets[i]=used;if(i<16)used+=counts[i]*sizes[i];}
    }else for(unsigned i=0;i<17;++i)offsets[i]=absolute[i]-absolute[0];
    size_t insertion=(size_t)(map.tail-original->bytes)+703+offsets[7],extra=65;
    uint8_t *bytes=malloc(original->size+extra);assert(bytes);
    memcpy(bytes,original->bytes,insertion);memset(bytes+insertion,0,extra);
    memcpy(bytes+insertion+extra,original->bytes+insertion,original->size-insertion);
    size_t directory=(size_t)(index-original->bytes);bytes[directory+7*3+2]=5;
    if(!all_zero)for(unsigned i=8;i<17;++i)put16(bytes+directory+3*i,absolute[i]+(unsigned)extra);
    for(unsigned i=0;i<5;++i){
        const uint16_t *p=sample->rows[i];uint8_t *raw=bytes+insertion+13*i;
        put16(raw,0x8000+i);put16(raw+2,p[0]);put16(raw+4,p[1]);raw[6]=(uint8_t)p[2];
        put16(raw+7,p[3]);raw[9]=(uint8_t)p[4];raw[10]=(uint8_t)p[5];put16(raw+11,p[6]);
    }
    unsigned section=original->sections[8].offset;
    for(unsigned i=1;i<original->offset_table_records[8];++i)put32(bytes+section+4*i,get32(bytes+section+4*i)+(uint32_t)extra);
    put32(bytes + 0x3e + 8*8,original->sections[8].size+(uint32_t)extra);
    for(unsigned i=9;i<original->section_count;++i)put32(bytes+0x3a+8*i,original->sections[i].offset+(uint32_t)extra);
    size_t npcs=(size_t)(map.tail-original->bytes)+80;memset(bytes+npcs,0,260);memset(bytes+(map.tail-original->bytes)+592,0,52);
    if(sample->npc[0]){
        uint8_t *raw=bytes+npcs;put16(raw,sample->npc[0]);raw[2]=(uint8_t)sample->npc[1];raw[3]=(uint8_t)sample->npc[2];
        put16(raw+8,sample->npc[3]);put16(raw+10,sample->npc[4]);
    }
    NpeFile file;char error[128];assert(npe_parse(&file,bytes,original->size+extra,error,sizeof(error))==0);
    if(!strcmp(sample->name,"ignored-side")){NpeItem item;assert(npe_item(&file,sample->id-1,&item)==0);((uint8_t *)item.raw)[36]=1;((uint8_t *)item.raw)[37]=0;}
    return file;
}
static NpeWorld fixture_world(const NpeFile *file,const MapQuestNative *sample){
    NpeWorld world;memset(&world,0xcc,sizeof(world));assert(npe_world_new_game(file,&world)==0);
    assert(!world.mode2_context&&world.x==sample->x&&world.y==sample->y);
    world.game_mode=4;world.direction=sample->direction;world.story_progress=(uint16_t)sample->progress;
    world.mode2_ready=1;world.mode2_followup_mode=5;world.mode2_followup_event_id=99;
    memset(world.condition_words,0,sizeof(world.condition_words));world.condition_words[sample->id]=2;return world;
}
static void check(const NpeWorld *world,const MapQuestNative *sample){
    const uint16_t *p=sample->outcome;assert(world->condition_words[sample->id]==p[4]&&world->story_progress==p[3]);
    if(sample->matched)assert(world->game_mode==p[0]&&world->story_event_id==p[1]&&world->event_context_id==p[2]&&!world->mode2_ready&&world->event_blank_background==(p[0]==5||p[0]==2));
    else assert(world->game_mode==4);
    assert(world->hp[0]==130&&world->qi[0]==60&&world->x==sample->x&&world->y==sample->y);
}
int main(int argc,char **argv){
    assert(argc>=2);SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
    NpeFile original;uint8_t *bytes;size_t size;char error[128];assert(read_file(argv[1],&bytes,&size)==0);
    assert(npe_parse(&original,bytes,size,error,sizeof(error))==0);uint64_t hash=npe_game_fingerprint(&original);
    NpeFont font={0};assert(npe_font_load(&font,"npe-sdl/build/assets/gbk12-native.bin")==0);assert(npe_font_load16(&font,"npe-sdl/build/assets/gbk16-native.bin")==0);
    SDL_Window *window=SDL_CreateWindow("quest-item",0,0,240,120,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    for(unsigned i=0;i<sizeof(map_quest_native)/sizeof(map_quest_native[0]);++i){
        const MapQuestNative *sample=&map_quest_native[i];NpeFile file=fixture_file(&original,sample);NpeMap map;assert(npe_map(&file,0,&map)==0);
        for(unsigned row=0;row<5;++row){NpeMapQuestItem event;assert(npe_map_quest_item(&map,row,&event)==1);
            const uint16_t *p=sample->rows[row];assert(event.record_id==0x8000+row&&event.min_progress==p[0]&&event.item_id==p[1]&&event.next_mode==p[2]&&event.next_id==p[3]&&event.x==p[4]&&event.y==p[5]&&event.npc_id==p[6]);}
        if(!i){
            uint8_t *count=(uint8_t *)map.tail+652+7*3+2;NpeMapQuestItem event;
            *count=6;assert(npe_map_quest_item(&map,0,&event)==-1);
            NpeWorld malformed=fixture_world(&file,sample),unchanged=malformed;NpeItem cached;
            assert(!npe_world_item(&file,&unchanged,sample->id,&cached));
            assert(npe_world_use_item(&file,&malformed,sample->id,0)==-1&&memcmp(&malformed,&unchanged,sizeof(malformed))==0);
            *count=4;assert(npe_map_quest_item(&map,0,&event)==-1);*count=5;
            assert(npe_map_quest_item(&map,5,&event)==-1);
        }
        NpeWorld world=fixture_world(&file,sample),before=world;
        NpeItem cached;assert(!npe_world_item(&file,&before,sample->id,&cached));
        int result=npe_world_use_item(&file,&world,sample->id,0);assert(result==(sample->matched?1:-3));check(&world,sample);
        if(!sample->matched)assert(memcmp(&world,&before,sizeof(world))==0);
        if(!strcmp(sample->name,"equal")){
            assert(npe_world_finish_story(&file,&world)==1);assert(world.game_mode==1&&world.story_progress==3&&world.story_event_id==1&&world.condition_words[146]==1);
            NpeWorld restored;const char *path="tmp/quest-item-followup.npes";
            assert(npe_world_save(&file,&world,path)==0&&npe_world_load(&file,&restored,path)==0);assert(restored.story_progress==3&&restored.condition_words[146]==1);assert(remove(path)==0);
        }
        /* Real SDL entry returns immediately on a matching quest and
           leaves the resource event for the outer event loop. */
        if(sample->matched){
            world=fixture_world(&file,sample);SDL_AtomicSet(&ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
            SDL_Thread *thread=SDL_CreateThread(send_success,"quest-success",NULL);assert(thread);
            assert(show_use_item(window,renderer,texture,&file,&font,&world,0)==1);SDL_WaitThread(thread,NULL);check(&world,sample);
            if(!strcmp(sample->name,"equal")||!strcmp(sample->name,"npc-dialogue")){
                checking_story=1;story_variant=sample->name;story_pages=0;SDL_AtomicSet(&ready,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
                thread=SDL_CreateThread(send_story,"quest-story",NULL);assert(thread);
                advance_world_events(window,renderer,texture,&file,&font,&world,4,0);SDL_WaitThread(thread,NULL);checking_story=0;
                fprintf(stderr,"quest followup %s: pages%u mode%u event%u blank%u item%u context%u progress%u\n",sample->name,story_pages,world.game_mode,world.story_event_id,world.event_blank_background,world.condition_words[146],world.event_context_id,world.story_progress);
                assert(story_pages==3&&world.game_mode==1&&world.story_event_id==1&&!world.event_blank_background&&world.condition_words[146]==1);
                if(!strcmp(sample->name,"equal"))assert(world.story_progress==3);
                else assert(world.story_progress==10&&world.event_context_id==7&&world.npc_state[7]==0);
                fprintf(stderr,"SDL quest %s: three original pages and event followup passed\n",sample->name);
            }
        }
        fprintf(stderr,"Original quest %s: fields, gates, quantity, context and SDL entry passed\n",sample->name);npe_free(&file);
    }
    const unsigned rejected[]={0,2,6,11,12};
    for(unsigned j=0;j<5;++j){
        const MapQuestNative *sample=&map_quest_native[rejected[j]];NpeFile file=fixture_file(&original,sample);NpeWorld world=fixture_world(&file,sample);
        char path[160];snprintf(path,sizeof(path),"tmp/quest-item-%s-selected.idx",sample->name);prompt_reference=path;
        SDL_AtomicSet(&ready,0);SDL_AtomicSet(&prompt_visible,0);SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        SDL_Thread *thread=SDL_CreateThread(send_rejected,"quest-refused",NULL);assert(thread);
        assert(show_use_item(window,renderer,texture,&file,&font,&world,0)==0);SDL_WaitThread(thread,NULL);prompt_reference=NULL;
        assert(world.game_mode==1&&world.condition_words[146]==2&&world.story_progress==sample->progress);
        fprintf(stderr,"SDL quest %s: original refusal and consumed dismissing key passed\n",sample->name);npe_free(&file);
    }
    unsigned maps=0,records=0,bad=0,path_count=0;const char *paths[32];uint8_t *manifest=NULL;
    if(argc==3&&strstr(argv[2],".txt")){
        assert(read_file(argv[2],&bytes,&size)==0);manifest=realloc(bytes,size+1);assert(manifest);manifest[size]=0;
        char *cursor=(char *)manifest;
        while(*cursor){assert(path_count<32);paths[path_count++]=cursor;char *end=strchr(cursor,'\n');if(!end)break;if(end>cursor&&end[-1]=='\r')end[-1]=0;*end=0;cursor=end+1;}
    }else for(int arg=2;arg<argc;++arg){assert(path_count<32);paths[path_count++]=argv[arg];}
    for(unsigned arg=0;arg<path_count;++arg){NpeFile file;assert(read_file(paths[arg],&bytes,&size)==0&&npe_parse(&file,bytes,size,error,sizeof(error))==0);
        for(unsigned i=0;i<file.offset_table_records[8];++i){NpeMap map;if(npe_map(&file,i,&map)){++bad;continue;}int failed=0;
            for(unsigned row=0;row<5;++row){NpeMapQuestItem event;int result=npe_map_quest_item(&map,row,&event);if(result<0){failed=1;break;}if(!result)break;++records;}
            if(failed)++bad;else++maps;}
        npe_free(&file);}
    if(path_count){assert(records==5&&bad==3);fprintf(stderr,"Quest corpus: %u games, %u valid maps, %u records, %u known invalid maps rejected\n",path_count,maps,records,bad);}free(manifest);
    assert(npe_game_fingerprint(&original)==hash);fprintf(stderr,"Quest native screens: %u; %u original rule fixtures passed\n",screen_count,(unsigned)(sizeof(map_quest_native)/sizeof(map_quest_native[0])));
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);npe_font_free(&font);npe_free(&original);SDL_Quit();return 0;
}
