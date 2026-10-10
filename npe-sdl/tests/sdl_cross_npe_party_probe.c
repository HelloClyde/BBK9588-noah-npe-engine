/* Actual current-NPE party services and their SDL event followups.
   Diagnostic active flags select changed paths; no NPE bytes are rewritten.
   SDL/C pixel consistency is checked, not original pixels or full gameplay. */
#include "../npe.h"
#include "../sound_sdl.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
static Uint32 probe_clock;
static unsigned frames, scene_frames;
static int no_input(SDL_Event *event){(void)event;return 0;}
static int next_key(SDL_Event *event){memset(event,0,sizeof(*event));event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_SPACE;return 1;}
static Uint32 ticks(void){return probe_clock;}
static void delay(Uint32 ms){probe_clock+=ms;}
static void present(SDL_Renderer *);
#define SDL_PollEvent no_input
#define SDL_WaitEvent next_key
#define SDL_GetTicks ticks
#define SDL_Delay delay
#define SDL_RenderPresent present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_WaitEvent
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_RenderPresent
#include "special_party_native.h"
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);uint32_t rgba[NPE_WIDTH*NPE_HEIGHT];
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,NPE_WIDTH*4));
    for(unsigned i=0;i<NPE_WIDTH*NPE_HEIGHT;++i){unsigned v=last_pixel_frame[i];assert(v<16);
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    ++frames;assert(++scene_frames<2000);
}
static void check_identity(const NpeWorld *w,const NpeWorld *before){
    for(unsigned i=0;i<3;++i){unsigned source=0;
        while(source<3&&before->party_ids[source]!=w->party_ids[i])++source;
        assert(source<3);
#define SAME_FIELD(field) assert(!memcmp(&w->field[i],&before->field[source],sizeof(w->field[i])))
        SAME_FIELD(party_names);SAME_FIELD(party_attack_groups);SAME_FIELD(party_characters);
        SAME_FIELD(party_record_residue);SAME_FIELD(skill_slots);SAME_FIELD(learned_skill_count);
        SAME_FIELD(equipped_items);SAME_FIELD(hp);SAME_FIELD(max_hp);SAME_FIELD(qi);SAME_FIELD(max_qi);
        SAME_FIELD(attack);SAME_FIELD(defense);SAME_FIELD(experience);SAME_FIELD(experience_to_next);
        SAME_FIELD(level);SAME_FIELD(growth);SAME_FIELD(other_stats);SAME_FIELD(damage_floor_stat);SAME_FIELD(status_guard);
#undef SAME_FIELD
        assert((i?w->companion_directions[i-1]:w->direction)==
               (source?before->companion_directions[source-1]:before->direction));
    }
}
static void state(FILE *report,const char *phase,unsigned origin_map,unsigned mode,unsigned event,int result,const NpeWorld *w){
    fprintf(report,"{\"phase\":\"%s\",\"origin_map\":%u,\"service\":%u,\"slot\":%u,\"result\":%d,\"frames\":%u,\"map\":%u,\"mode\":%u,\"event\":%u,\"ids\":[%u,%u,%u],\"active\":[%u,%u,%u]}\n",
        phase,origin_map,mode,event,result,frames,w->map_number,w->game_mode,w->story_event_id,
        w->party_ids[0],w->party_ids[1],w->party_ids[2],w->party_active[0],w->party_active[1],w->party_active[2]);
    fflush(report);fprintf(stderr,"%s map=%u service=%u slot=%u ->%u/%u frames=%u\n",phase,origin_map,mode,event,w->game_mode,w->story_event_id,frames);
}
int main(int argc,char **argv){
    assert(argc==3||argc==4);char source[1024];FILE *manifest=fopen(argv[1],"rb");
    assert(manifest&&fgets(source,sizeof(source),manifest)&&!fclose(manifest));source[strcspn(source,"\r\n")]=0;
    FILE *report=fopen(argv[2],"wb");assert(report);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(source,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&file);
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    NpeFont font={0};assert(!load_native_fonts(&font));
    SDL_Window *win=SDL_CreateWindow("current NPE party",0,0,NPE_WIDTH,NPE_HEIGHT,SDL_WINDOW_HIDDEN);assert(win);
    SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
    unsigned services=0,adjustments=0,unresolved=0,battles=0;
    for(unsigned index=0;index<file.offset_table_records[8];++index){
        NpeMap map;assert(!npe_map(&file,index,&map));unsigned party_count=0,adjust_count=0;
        if((!map.width||!map.height)&&argc!=4){
            fprintf(report,"{\"phase\":\"unresolved-map\",\"map\":%u,\"reason\":\"zero-dimension needs original state reference\"}\n",index+1);++unresolved;continue;
        }
        if(npe_map_group_count(&map,6,&party_count)||npe_map_group_count(&map,3,&adjust_count)){
            fprintf(report,"{\"phase\":\"unresolved-map\",\"map\":%u}\n",index+1);++unresolved;continue;
        }
        unsigned unverified=0;
        for(unsigned slot=1;slot<=party_count;++slot){NpeMapPartyEvent e;
            assert(npe_map_party_event(&map,slot,&e)==1);
            unverified|=!e.character_id||e.character_id>3||e.add_member>1;}
        for(unsigned slot=1;slot<=adjust_count;++slot){NpeMapPartyAdjustEvent e;
            assert(npe_map_party_adjust_event(&map,slot,&e)==1);unverified|=!e.character_id||e.character_id>3;}
        /* These records need separate original-reference assertions: the
           normal character-identity contract cannot certify RAM aliases.
           Preserve a nonzero exit and count them as unresolved. */
        if(unverified&&argc!=4){fprintf(report,"{\"phase\":\"unresolved-map\",\"map\":%u,\"reason\":\"nonstandard party record needs original state reference\"}\n",index+1);++unresolved;continue;}
        unsigned special=unverified||!map.width||!map.height;
        for(unsigned mode=10;mode<=13;mode+=3)for(unsigned slot=1;slot<=(mode==10?party_count:adjust_count);++slot){
            NpeMapPartyEvent event={0};NpeMapPartyAdjustEvent adjust={0};
            if(mode==10)assert(npe_map_party_event(&map,slot,&event)==1);
            else assert(npe_map_party_adjust_event(&map,slot,&adjust)==1);
            NpeWorld world;scene_frames=0;
            state(report,"begin",index+1,mode,slot,0,&(NpeWorld){0});
            assert(!npe_world_init(&file,&world,index+1,2,2)&&!npe_world_resources_open(&file,&world));
            world.music_enabled=0;world.defer_story_events=1;
            world.party_active[0]=1;world.party_active[1]=world.party_active[2]=(uint8_t)(mode==13||!event.add_member);
            if(special)world.hp[0]=world.qi[0]=1;
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];assert(!npe_render_world_load(&file,&world,pixels));
            draw_pixels(renderer,texture,pixels);save_map_edges(&file,renderer,pixels);retain_world_frame(&file,&world,renderer,pixels,1);
            NpeWorld before=world;world.game_mode=(uint16_t)mode;world.story_event_id=(uint16_t)slot;
            char native_name[80];snprintf(native_name,sizeof(native_name),"map%u-mode%u-slot%u-v0",index+1,mode,slot);
            if(special)check_special_party_native(argv[3],native_name,&world,1);
            int result=mode==10?show_party_event(renderer,texture,&file,&font,&world):show_party_adjust_event(renderer,texture,&file,&world);
            state(report,"service",index+1,mode,slot,result,&world);assert(result>=0);
            if(special){check_special_party_native(argv[3],native_name,&world,0);state(report,"special-original",index+1,mode,slot,result,&world);}
            else check_identity(&world,&before);
            if(mode==10){
                if(!special)assert(world.game_mode==event.next_mode&&world.story_event_id==event.next_id);
                if(event.character_id==2||event.character_id==3){
                    assert(world.party_ids[0]==1);
                    if(event.add_member){assert(world.party_ids[1]==event.character_id&&world.party_active[1]==1&&!world.party_active[2]);}
                    else{assert(world.party_ids[2]==event.character_id&&!world.party_active[2]&&world.party_active[1]==1);}
                }
                ++services;
            }else{
                unsigned changed=adjust.character_id==2||adjust.character_id==3;
                assert(result==(int)changed);
                assert(world.game_mode==(changed?adjust.next_mode:0)&&world.story_event_id==(changed?adjust.next_id:slot));
                if(changed)assert(world.party_ids[0]==adjust.character_id);
                ++adjustments;
            }
            result=advance_world_events_dispatch(win,renderer,texture,&file,&font,&world,mode,0);
            state(report,"followup",index+1,mode,slot,result,&world);assert(result>=0);
            if(world.game_mode==3){
                assert(!draw_battle_entry(renderer,texture,&file,&world,&font));
                /* Some raw followups use an empty random table and retain
                   the preceding formation. This isolated scene has no
                   prior battle, so do not invent enemies or a count. */
                if(!world.battle_enemy_count){state(report,"empty-battle-entry",index+1,mode,slot,0,&world);
                    assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);continue;}
                BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
                world.battle_animation_observer=play_battle_animation;world.battle_animation_context=&display;
                for(unsigned i=0;i<3;++i)world.hp[i]=world.max_hp[i]=world.qi[i]=world.max_qi[i]=30000;
                unsigned actors=0;
                for(unsigned action=0;action<3;++action){unsigned actor=world.battle_actor;assert(actor<3&&world.party_active[actor]);actors|=1u<<actor;
                    world.battle_menu_phase=31;result=npe_world_battle_defend(&world);assert(result==NPE_BATTLE_CONTINUE);
                    assert(draw_battle_animation_commands(renderer,texture,&file,&world,&font,2)>=0);
                    if(world.battle_actor==0)break;
                }
                state(report,"battle-round",index+1,mode,slot,(int)actors,&world);++battles;
            }
            assert(!npe_rpg_runtime_failures(&file));npe_rpg_runtime_close(&file);
        }
    }
    assert(fingerprint==npe_game_fingerprint(&file));
    fprintf(report,"{\"phase\":\"complete\",\"frames\":%u,\"party_services\":%u,\"leader_services\":%u,\"battle_rounds\":%u,\"unresolved_maps\":%u,\"npe_unchanged\":true}\n",frames,services,adjustments,battles,unresolved);
    assert(!fclose(report));SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);
    npe_font_free(&font);npe_free(&file);SDL_Quit();return unresolved?2:0;
}
