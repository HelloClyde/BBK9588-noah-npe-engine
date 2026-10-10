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
typedef struct {unsigned id,x,y,direction;} AdjustNpc;
typedef struct {unsigned mode,event,context,map,progress,init,sx,sy,vx,vy,direction,enabled,slot,menu_count;
    uint8_t actors[3][148];AdjustNpc npc[20];} AdjustState;
typedef struct {unsigned character,mode,next;} AdjustCache;
typedef struct {const char *name;unsigned commits,changed;AdjustCache cache;AdjustState before,after;} AdjustCase;
#include "party_adjust_native.h"
typedef struct {const char *name;unsigned map,restart;AdjustCache boot,before,after;} AdjustCacheCase;
#include "party_adjust_cache_native.h"
static unsigned case_index,submits,total,active,close_after_submit;
static NpeWorld *live;
static void probe_present(SDL_Renderer *);
#define SDL_RenderPresent probe_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent

static unsigned native_word(const uint8_t *raw,unsigned offset){return (unsigned)raw[offset]*256+raw[offset+1];}
static void actor_from_native(NpeWorld *w,unsigned i,const uint8_t *r){
    memcpy(w->party_names[i],r,12);
    w->party_ids[i]=r[12];w->party_active[i]=r[43];w->level[i]=r[24];
    memcpy(w->equipped_items[i],r+18,6);memcpy(w->skill_slots[i],r+25,18);
    for(unsigned j=0;j<4;++j)w->status_guard[i][j]=(uint16_t)native_word(r,62+j*2);
    w->experience_to_next[i]=(uint16_t)native_word(r,74);w->experience[i]=(uint16_t)native_word(r,76);
    w->max_hp[i]=(uint16_t)native_word(r,78);w->hp[i]=(uint16_t)native_word(r,80);
    w->max_qi[i]=(uint16_t)native_word(r,82);w->qi[i]=(uint16_t)native_word(r,84);
    for(unsigned j=0;j<3;++j)w->other_stats[i][j]=(uint16_t)native_word(r,86+j*2);
    w->attack[i]=(uint16_t)native_word(r,92);w->defense[i]=(uint16_t)native_word(r,94);
    w->damage_floor_stat[i]=r[24];
    if(i)w->companion_directions[i-1]=r[14];else w->direction=r[14];
}
static void check_actors(const NpeWorld *w,const AdjustState *s){
    for(unsigned i=0;i<3;++i){const uint8_t *r=s->actors[i];
        assert(w->party_ids[i]==r[12]&&w->party_active[i]==r[43]&&w->level[i]==r[24]);
        assert(!memcmp(w->equipped_items[i],r+18,6)&&!memcmp(w->skill_slots[i],r+25,18));
        for(unsigned j=0;j<4;++j)assert(w->status_guard[i][j]==native_word(r,62+j*2));
        assert(w->experience_to_next[i]==native_word(r,74)&&w->experience[i]==native_word(r,76));
        assert(w->max_hp[i]==native_word(r,78)&&w->hp[i]==native_word(r,80));
        assert(w->max_qi[i]==native_word(r,82)&&w->qi[i]==native_word(r,84));
        for(unsigned j=0;j<3;++j)assert(w->other_stats[i][j]==native_word(r,86+j*2));
        assert(w->attack[i]==native_word(r,92)&&w->defense[i]==native_word(r,94));
        assert((i?w->companion_directions[i-1]:w->direction)==r[14]);
    }
}
static void seed_cache(NpeWorld *w,AdjustCache c){
    w->party_adjust_event=(NpeMapPartyAdjustEvent){.character_id=(uint8_t)c.character,.next_mode=(uint8_t)c.mode,.next_id=(uint16_t)c.next};
}
static void check_cache(const NpeWorld *w,AdjustCache c){NpeMapPartyAdjustEvent e;
    assert(npe_world_party_adjust_event(w,1,&e)==1&&!e.raw&&e.character_id==c.character&&e.next_mode==c.mode&&e.next_id==c.next);
}
static void check_state(const NpeFile *file,const NpeWorld *w,const AdjustState *s){
    assert(w->game_mode==s->mode&&w->story_event_id==s->event&&w->event_context_id==s->context);
    assert(w->map_number==s->map&&w->story_progress==s->progress&&w->map_init_pending==s->init);
    assert(w->x==s->sx+s->vx&&w->y==s->sy+s->vy&&w->view.screen_x==s->sx&&w->view.screen_y==s->sy);
    assert(w->view.offset_x==s->vx&&w->view.offset_y==s->vy&&w->direction==s->direction);
    assert(w->music_enabled==s->enabled&&w->map_music_slot==s->slot&&w->menu_party_count==s->menu_count&&w->random_state==11);
    check_actors(w,s);NpeMap map;assert(!npe_map(file,s->map-1,&map));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){const uint8_t *npc;unsigned x,y;
        assert(!npe_map_npc(&map,i,&npc));npe_world_npc_position(w,npc,i,&x,&y);
        unsigned direction=w->npc_override_direction[i]!=255?w->npc_override_direction[i]:npc[6];
        assert(x==s->npc[i].x&&y==s->npc[i].y&&direction==s->npc[i].direction);}
}
static void read_frame(int before,unsigned index,uint8_t *pixels){char path[170];
    if(before)snprintf(path,sizeof(path),"tmp/native-party-adjust-%s-before.idx",adjust_cases[case_index].name);
    else snprintf(path,sizeof(path),"tmp/native-party-adjust-%s-%u.idx",adjust_cases[case_index].name,index);
    FILE *in=fopen(path,"rb");assert(in&&fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static void probe_present(SDL_Renderer *r){SDL_RenderPresent(r);if(!active)return;
    const AdjustCase *c=&adjust_cases[case_index];assert(submits<c->commits);
    uint8_t native[NPE_WIDTH*NPE_HEIGHT];uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];read_frame(0,submits,native);
    assert(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    unsigned d=0,sd=0;for(unsigned i=0;i<sizeof(native);++i){unsigned v=native[i];d+=last_pixel_frame[i]!=v;
        sd+=rgba[i]!=(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15));}
    fprintf(stderr,"party-adjust %s commit%u C=%u SDL=%u\n",c->name,submits,d,sd);
    if(d){char path[180];snprintf(path,sizeof(path),"tmp/party-adjust-failed-%s-%u.idx",c->name,submits);
        FILE *out=fopen(path,"wb");assert(out&&fwrite(last_pixel_frame,1,sizeof(native),out)==sizeof(native)&&!fclose(out));}
    assert(!d&&!sd);
    if(c->changed&&!submits){assert(live->game_mode==13&&live->story_event_id==c->before.event&&live->map_init_pending==1);check_actors(live,&c->after);}
    ++submits;++total;
    if(close_after_submit){SDL_Event e={0};e.type=SDL_QUIT;assert(SDL_PushEvent(&e)==1);}
}
static void configure(const NpeFile *file,NpeWorld *w,const NpeWorld *baseline,const AdjustCase *c){
    const AdjustState *s=&c->before;*w=*baseline;
    if(s->map!=1){NpeWorld saved;const char *path="tmp/party-adjust-config.npes";
        assert(!npe_world_init(file,&saved,s->map,1,1)&&!npe_world_save(file,&saved,path)&&!npe_world_restore(file,w,path)&&!remove(path));}
    for(unsigned i=0;i<3;++i)actor_from_native(w,i,s->actors[i]);
    w->game_mode=13;w->story_event_id=(uint16_t)s->event;w->event_context_id=(uint16_t)s->context;
    w->story_progress=(uint16_t)s->progress;w->map_init_pending=(uint8_t)s->init;
    w->view=(NpeViewport){s->sx,s->sy,s->vx,s->vy};w->x=s->sx+s->vx;w->y=s->sy+s->vy;
    w->music_enabled=(uint8_t)s->enabled;w->map_music_slot=(uint8_t)s->slot;w->menu_party_count=(uint8_t)s->menu_count;
    w->lcd_background=0;npe_world_seed(w,11);
    assert(!npe_world_resources_open((NpeFile *)file,w));
    uint8_t bytes[]={(uint8_t)c->cache.character,(uint8_t)c->cache.mode,(uint8_t)(c->cache.next>>8),(uint8_t)c->cache.next};
    assert(!npe_rpg_runtime_write(file,0x5279a2,bytes,4));
    for(unsigned i=0;i<20&&s->npc[i].id;++i){w->npc_override_x[i]=(uint16_t)s->npc[i].x;w->npc_override_y[i]=(uint16_t)s->npc[i].y;
        w->npc_override_direction[i]=s->npc[i].direction<4?(uint8_t)s->npc[i].direction:255;}
    check_state(file,w,s);
}
static uint64_t checksum(const uint8_t *b,size_t n){uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;++i)h=(h^b[i])*UINT64_C(1099511628211);return h;}
static void write_bytes(const char *path,uint8_t *b,size_t n){
    for(unsigned i=0;i<4;++i)b[24+i]=(uint8_t)((n-36)>>(i*8));uint64_t h=checksum(b+36,n-36);
    for(unsigned i=0;i<8;++i)b[28+i]=(uint8_t)(h>>(i*8));FILE *out=fopen(path,"wb");assert(out&&fwrite(b,1,n,out)==n&&!fclose(out));
}
static void cache_checks(const NpeFile *file,const NpeWorld *baseline){
    const char *path="tmp/party-adjust-cache.npes";
    for(unsigned i=0;i<8;++i){const AdjustCacheCase *c=&adjust_cache_cases[i];NpeWorld w=*baseline,saved,cold;
        check_cache(baseline,c->boot);seed_cache(&w,c->before);
        if(c->restart){assert(!npe_world_restart_game(file,&w));check_cache(&w,c->after);continue;}
        assert(!npe_world_init(file,&saved,c->map,1,1)&&!npe_world_save(file,&saved,path));
        assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);
        assert(!npe_world_restore(file,&w,path));check_cache(&w,c->after);
        assert(!npe_world_load(file,&cold,path));check_cache(&cold,c->boot);
        NpeWorld before=w;FILE *out=fopen(path,"wb");assert(out&&fwrite("NPESAV18",1,8,out)==8&&!fclose(out));
        assert(npe_world_restore(file,&w,path)<0&&!memcmp(&before,&w,sizeof(w))&&!remove(path));
        NpeMap map;assert(!npe_map(file,c->map-1,&map));uint8_t tail[717]={0};tail[652+3*3+2]=2;
        NpeMap bad=map;bad.tail=tail;bad.tail_size=sizeof(tail);
        assert(npe_world_load_party_adjust_event(&w,&bad)<0&&!memcmp(&before,&w,sizeof(w)));
    }
    /* Real group3 wire layout; the first three bytes are discarded by29:DB1A/DB40. */
    uint8_t tail[710]={0};tail[652+3*3+2]=1;memcpy(tail+703,"\0\0\0\2\1\0\1",7);
    NpeMap map;assert(!npe_map(file,0,&map));map.tail=tail;map.tail_size=sizeof(tail);
    NpeWorld w=*baseline;seed_cache(&w,(AdjustCache){3,25,789});assert(!npe_world_load_party_adjust_event(&w,&map));
    check_cache(&w,(AdjustCache){2,1,1});tail[703]=255;tail[704]=128;tail[705]=73;
    assert(!npe_world_load_party_adjust_event(&w,&map));check_cache(&w,(AdjustCache){2,1,1});
    NpeWorld a=*baseline,b=a;seed_cache(&b,(AdjustCache){2,1,789});
    assert(!npe_world_save(file,&a,"tmp/party-adjust-wire-a.npes")&&!npe_world_save(file,&b,"tmp/party-adjust-wire-b.npes"));
    FILE *fa=fopen("tmp/party-adjust-wire-a.npes","rb"),*fb=fopen("tmp/party-adjust-wire-b.npes","rb");assert(fa&&fb);
    int ca,cb;do{ca=fgetc(fa);cb=fgetc(fb);assert(ca==cb);}while(ca!=EOF);
    assert(!fclose(fa)&&!fclose(fb)&&!remove("tmp/party-adjust-wire-a.npes")&&!remove("tmp/party-adjust-wire-b.npes"));
}
static void old_save_checks(const NpeFile *file,const NpeWorld *baseline){
    const char *path="tmp/party-adjust-legacy.npes";NpeWorld w=*baseline,loaded;
    w.companion_directions[0]=2;w.companion_directions[1]=1;assert(!npe_world_save(file,&w,path));
    uint8_t bytes[4096];FILE *in=fopen(path,"rb");assert(in);size_t size=fread(bytes,1,sizeof(bytes),in);
    assert(!fclose(in)&&size>40&&!memcmp(bytes,"NPESAV22",8));
    assert(!npe_world_load(file,&loaded,path)&&loaded.companion_directions[0]==2&&loaded.companion_directions[1]==1);
    size-=128+NPE_CHARACTER_RECORD_SAVE_BYTES;assert(bytes[size-2]==2&&bytes[size-1]==1);memcpy(bytes,"NPESAV17",8);write_bytes(path,bytes,size-2);
    assert(!npe_world_load(file,&loaded,path));assert(!memcmp(loaded.companion_directions,baseline->companion_directions,2));
    w.party_ids[1]=3;w.party_ids[2]=2;w.companion_directions[0]=1;w.companion_directions[1]=2;
    assert(!npe_world_save(file,&w,path));in=fopen(path,"rb");assert(in);size=fread(bytes,1,sizeof(bytes),in);assert(!fclose(in));size-=128+NPE_CHARACTER_RECORD_SAVE_BYTES;
    memcpy(bytes,"NPESAV17",8);write_bytes(path,bytes,size-2);assert(!npe_world_load(file,&loaded,path));
    assert(loaded.companion_directions[0]==baseline->companion_directions[1]&&loaded.companion_directions[1]==baseline->companion_directions[0]);
    assert(!remove(path));
}
static void provided_record_checks(const char *path,unsigned map_number,unsigned character,unsigned next_mode){
    NpeFile file;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(path,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    {NpeMap map;NpeMapPartyAdjustEvent event;NpeWorld w,loaded;
        assert(!npe_map(&file,map_number-1,&map)&&npe_map_party_adjust_event(&map,1,&event)==1);
        assert(event.character_id==character&&event.next_mode==next_mode&&event.next_id==1);
        assert(!npe_world_init(&file,&w,map_number,1,1));check_cache(&w,(AdjustCache){character,next_mode,1});
        unsigned original_direction=w.direction,second_direction=w.companion_directions[0];
        uint16_t hp0=w.hp[0],hp1=w.hp[1];uint8_t skills0[18],skills1[18];
        memcpy(skills0,w.skill_slots[0],18);memcpy(skills1,w.skill_slots[1],18);
        w.party_active[0]=w.party_active[1]=1;w.game_mode=13;w.story_event_id=257;w.defer_story_events=1;
        unsigned changed=character==2;
        assert(npe_world_apply_mode13_event(&file,&w)==(int)changed&&w.game_mode==(changed?next_mode:0)&&w.story_event_id==(changed?1:257)&&w.map_init_pending==1);
        assert(w.party_ids[0]==(changed?2:1)&&w.party_ids[1]==(changed?1:2)&&w.direction==(changed?second_direction:original_direction)&&w.companion_directions[0]==(changed?original_direction:second_direction));
        assert(w.hp[0]==(changed?hp1:hp0)&&w.hp[1]==(changed?hp0:hp1)&&!memcmp(w.skill_slots[0],changed?skills1:skills0,18)&&!memcmp(w.skill_slots[1],changed?skills0:skills1,18));
        w.game_mode=1;w.defer_story_events=0;
        const char *save="tmp/party-adjust-cross-game.npes";
        assert(!npe_world_save(&file,&w,save)&&!npe_world_load(&file,&loaded,save));
        assert(loaded.party_ids[0]==w.party_ids[0]&&loaded.party_ids[1]==w.party_ids[1]&&loaded.direction==w.direction&&loaded.companion_directions[0]==w.companion_directions[0]);
        assert(!npe_world_restore(&file,&loaded,save));check_cache(&loaded,(AdjustCache){character,next_mode,1});assert(!remove(save));
        /* An actual empty group must keep the preceding resident slot. */
        unsigned empty_map=1;NpeMap candidate;
        assert(!npe_map(&file,empty_map-1,&candidate)&&!npe_map_party_adjust_event(&candidate,1,&event));
        assert(!npe_world_load_party_adjust_event(&w,&candidate));check_cache(&w,(AdjustCache){character,next_mode,1});
        printf("Real group3 map%u:target%u,next%u/1,changed%u,record directions,NPESAV19 and empty map%u retention passed\n",map_number,character,next_mode,changed,empty_map);
    }
    npe_free(&file);
}
int main(int argc,char **argv){assert(argc==2||argc==4);NpeFile file;uint8_t *bytes;size_t size;char error[128];NpeFont font={0};NpeWorld baseline,w;
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
    SDL_Window *window=SDL_CreateWindow("party adjustment",0,0,NPE_WIDTH,NPE_HEIGHT,0);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin")&&!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    assert(!npe_world_new_game(&file,&baseline));live=&w;cache_checks(&file,&baseline);old_save_checks(&file,&baseline);
    unsigned saves=0,rejected=0;
    for(case_index=0;case_index<21;++case_index){const AdjustCase *c=&adjust_cases[case_index];configure(&file,&w,&baseline,c);
        NpeWorld numeric=w;unsigned id=numeric.story_event_id;int result=npe_world_apply_mode13_event(&file,&numeric);
        assert(result==(int)c->changed&&numeric.map_init_pending==1);check_actors(&numeric,&c->after);
        assert(numeric.story_event_id==(c->changed?c->cache.next:id));
        unsigned expected_mode=c->changed?(c->cache.mode==14?0:c->cache.mode):0;
        assert(numeric.game_mode==expected_mode);check_cache(&numeric,c->cache);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);active=0;draw_pixels(renderer,texture,pixels);
        save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&w,renderer,pixels,1);submits=0;active=1;
        assert(advance_world_events(window,renderer,texture,&file,&font,&w,13,0)>=0&&submits==c->commits);
        check_state(&file,&w,&c->after);check_cache(&w,c->cache);
        active=0;const char *path="tmp/party-adjust-state.npes";assert(!npe_world_save(&file,&w,path));NpeWorld loaded;
        /* Literal flag2 is a resident byte, retained by V18+ saves. */
        if(!strcmp(c->name,"duplicate")){assert(npe_world_load(&file,&loaded,path)<0);++rejected;}
        else{assert(!npe_world_load(&file,&loaded,path));check_actors(&loaded,&c->after);++saves;}
        assert(!remove(path));
        assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
    }
    assert(saves==20&&rejected==1);
    case_index=0;configure(&file,&w,&baseline,&adjust_cases[0]);uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];read_frame(1,0,pixels);
    active=0;draw_pixels(renderer,texture,pixels);submits=0;active=close_after_submit=1;
    assert(advance_world_events(window,renderer,texture,&file,&font,&w,13,0)==-2&&submits==1&&w.game_mode==13&&w.story_event_id==1&&w.map_init_pending==1&&!w.defer_story_events);
    check_actors(&w,&adjust_cases[0].after);active=0;SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);npe_rpg_runtime_close(&file);
    if(argc==4){provided_record_checks(argv[2],52,2,1);provided_record_checks(argv[3],21,2,14);provided_record_checks(argv[3],31,1,10);}
    npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();npe_free(&file);
    printf("Party adjustment:21 original short paths,%u C/SDL commits,8 original UI cache loads,strict mode13 flags/low-byte/record directions,20 saves including literal flag2,1 duplicate-ID rejection,V17 migration and close passed\n",total-1);return 0;
}
