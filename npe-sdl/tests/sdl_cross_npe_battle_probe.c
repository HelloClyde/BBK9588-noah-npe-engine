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
static unsigned frames;
static const char *phase;
static NpeWorld *probe_world;
static FILE *report;
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
static void present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);uint32_t rgba[28800];
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,rgba,240*4));
    for(unsigned i=0;i<28800;++i){unsigned v=last_pixel_frame[i];assert(v<16);
        assert(rgba[i]==(UINT32_C(0xff000000)|((183-v*142/15)<<16)|((200-v*146/15)<<8)|(161-v*119/15)));}
    assert(++frames<3000);
}
static void state(int result){
    NpeWorld *w=probe_world;
    fprintf(report,"{\"phase\":\"%s\",\"result\":%d,\"frames\":%u,\"mode\":%u,\"event\":%u,\"map\":%u,\"actor\":%u,\"enemies\":%u,\"hp\":%u,\"qi\":%u,\"runtime_failed\":%d}\n",
        phase,result,frames,w->game_mode,w->story_event_id,w->map_number,w->battle_actor,w->battle_enemy_count,w->hp[0],w->qi[0],npe_rpg_runtime_failures(w->battle_file));fflush(report);
    fprintf(stderr,"phase=%s result=%d frames=%u mode=%u event=%u hp=%u\n",phase,result,frames,w->game_mode,w->story_event_id,w->hp[0]);
}
int main(int argc,char **argv){
    assert(argc==3);char source[1024];FILE *manifest=fopen(argv[1],"rb");
    assert(manifest&&fgets(source,sizeof(source),manifest)&&!fclose(manifest));source[strcspn(source,"\r\n")]=0;
    report=fopen(argv[2],"wb");assert(report);uint8_t *bytes;size_t size;NpeFile file;char error[128];
    assert(!read_file(source,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&file);unsigned chosen_map=0,chosen_event=0;NpeMapBattleEvent expected={0};
    for(unsigned index=0;index<file.offset_table_records[8]&&!chosen_map;++index){NpeMap map;if(npe_map(&file,index,&map))continue;
        for(unsigned id=1;id<=4&&!chosen_map;++id){NpeMapBattleEvent event;
            if(npe_map_battle_event(&map,id,&event)!=1||!event.formation_key)continue;
            for(unsigned j=0;j<4;++j){NpeMapMonster monster;assert(!npe_map_monster(&map,j,&monster));
                if(monster.enemy_id==event.formation_key){chosen_map=index+1;chosen_event=id;expected=event;break;}}
        }
    }
    if(!chosen_map)for(unsigned index=0;index<file.offset_table_records[8];++index){
        NpeMap map;if(!npe_map(&file,index,&map)&&map.tail_size>=652&&map.tail[644]>0&&map.tail[644]<128){
            chosen_map=index+1;expected.win_mode=1;break;}
    }
    assert(chosen_map);NpeWorld world;probe_world=&world;
    fprintf(stderr,"source map=%u battle=%u win=%u/%u lose=%u/%u\n",chosen_map,chosen_event,
        expected.win_mode,expected.win_event_id,expected.lose_mode,expected.lose_event_id);
    assert(!npe_world_init(&file,&world,chosen_map,2,2)&&!npe_world_resources_open(&file,&world));
    world.music_enabled=0;
    /* Diagnostic attributes shorten the battle; all records and artwork
       still come from this NPE. This is not a natural story playthrough. */
    world.hp[0]=world.max_hp[0]=30000;world.qi[0]=world.max_qi[0]=30000;
    if(chosen_event){
        world.game_mode=3;world.story_event_id=(uint16_t)chosen_event;
        assert(npe_world_prepare_scripted_battle(&file,&world)==1);
        assert(world.battle_enemy_count==3&&world.battle_win_mode==expected.win_mode&&world.battle_win_event_id==expected.win_event_id);
    }else{
        for(unsigned attempt=0;attempt<8&&world.game_mode==1;++attempt){
            world.encounter_steps=1000;npe_world_seed(&world,1);
            assert(npe_world_step(&file,&world,attempt%4)>=0);
        }
        assert(world.game_mode==3&&world.battle_enemy_count&&world.story_event_id==0);
    }
    assert(!SDL_setenv("SDL_VIDEODRIVER","dummy",1)&&!SDL_Init(SDL_INIT_VIDEO));
    NpeFont font={0};assert(!load_native_fonts(&font));
    SDL_Window *win=SDL_CreateWindow("current NPE battle",0,0,240,120,SDL_WINDOW_HIDDEN);assert(win);
    SDL_Renderer *renderer=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    phase="entry";assert(!draw_battle_entry(renderer,texture,&file,&world,&font));state(1);
    BattleAnimationDisplay display={0};display.renderer=renderer;display.texture=texture;display.font=&font;
    world.battle_animation_observer=play_battle_animation;world.battle_animation_context=&display;
    world.defer_story_events=1;
    phase="defense";world.battle_menu_phase=31;int result=npe_world_battle_defend(&world);state(result);
    assert(result==NPE_BATTLE_CONTINUE&&world.game_mode==3);
    assert(draw_battle_animation_commands(renderer,texture,&file,&world,&font,2)>=0);
    unsigned skill_id=0;
    for(unsigned i=0;i<18&&!skill_id;++i)if(world.skill_slots[0][i]){
        NpeSkill skill;if(!npe_skill(&file,world.skill_slots[0][i]-1,&skill)&&skill.effect_type<16)skill_id=world.skill_slots[0][i];}
    if(skill_id){phase="skill";memset(world.skill_slots[0],0,18);world.skill_slots[0][0]=(uint8_t)skill_id;world.level[0]=99;
        world.battle_menu_phase=20;show_battle_skill(win,renderer,texture,&file,&font,&world);state(1);
        assert(world.game_mode==3&&world.battle_actor==0&&!world.battle_pending_mask);
        assert(draw_battle_animation_commands(renderer,texture,&file,&world,&font,1)>=0);
    }
    phase="victory";world.level[0]=99;world.attack[0]=30000;
    for(unsigned round=0;round<8&&world.game_mode==3;++round){
        unsigned target=0;while(target<world.battle_enemy_count&&(int16_t)world.battle_enemy_hps[target]<=0)++target;
        assert(target<world.battle_enemy_count);world.battle_menu_phase=7;
        result=npe_world_battle_attack(&world,target);state(result);assert(result>=0);
        if(world.game_mode==3)assert(draw_battle_animation_commands(renderer,texture,&file,&world,&font,0)>=0);
    }
    assert(result==NPE_BATTLE_WON&&world.game_mode==(expected.win_mode?expected.win_mode:1)&&world.story_event_id==expected.win_event_id);
    phase="followup";result=advance_world_events_dispatch(win,renderer,texture,&file,&font,&world,3,0);
    state(result);assert(result>=0);
    assert(world.game_mode==1);
    char save_path[1024];assert(snprintf(save_path,sizeof(save_path),"%s.npes",argv[2])>0);
    NpeWorld restored;phase="save-load";
    assert(!npe_world_save(&file,&world,save_path));npe_rpg_runtime_close(&file);
    assert(!npe_world_load(&file,&restored,save_path)&&!remove(save_path));
    assert(restored.map_number==world.map_number&&restored.story_progress==world.story_progress&&restored.game_mode==world.game_mode&&restored.story_event_id==world.story_event_id);
    assert(restored.x==world.x&&restored.y==world.y&&!memcmp(&restored.view,&world.view,sizeof(world.view))&&restored.money==world.money);
    assert(!memcmp(restored.party_ids,world.party_ids,sizeof(world.party_ids))&&
           !memcmp(restored.hp,world.hp,sizeof(world.hp))&&!memcmp(restored.qi,world.qi,sizeof(world.qi))&&
           !memcmp(restored.max_hp,world.max_hp,sizeof(world.max_hp))&&!memcmp(restored.max_qi,world.max_qi,sizeof(world.max_qi))&&
           !memcmp(restored.attack,world.attack,sizeof(world.attack))&&!memcmp(restored.defense,world.defense,sizeof(world.defense))&&
           !memcmp(restored.level,world.level,sizeof(world.level))&&!memcmp(restored.experience,world.experience,sizeof(world.experience))&&
           !memcmp(restored.equipped_items,world.equipped_items,sizeof(world.equipped_items))&&
           !memcmp(restored.skill_slots,world.skill_slots,sizeof(world.skill_slots))&&
           !memcmp(restored.other_stats,world.other_stats,sizeof(world.other_stats))&&
           !memcmp(restored.status_guard,world.status_guard,sizeof(world.status_guard))&&
           !memcmp(restored.condition_words,world.condition_words,sizeof(world.condition_words)));
    assert(!npe_world_resources_open(&file,&restored)&&npe_rpg_runtime_owner(&file)==&restored);
    probe_world=&restored;state(0);
    uint8_t map_pixels[28800];assert(!npe_render_world_load(&file,&restored,map_pixels));
    draw_pixels(renderer,texture,map_pixels);
    phase="reentry";restored.defer_story_events=1;restored.music_enabled=0;
    if(chosen_event){restored.game_mode=3;restored.story_event_id=(uint16_t)chosen_event;
        assert(npe_world_prepare_scripted_battle(&file,&restored)==1);
    }else{
        for(unsigned attempt=0;attempt<8&&restored.game_mode==1;++attempt){
            restored.encounter_steps=1000;npe_world_seed(&restored,1);
            assert(npe_world_step(&file,&restored,attempt%4)>=0);
        }
    }
    assert(restored.game_mode==3&&restored.battle_enemy_count&&restored.battle_actor==0);
    assert(!draw_battle_entry(renderer,texture,&file,&restored,&font));state(1);
    assert(!npe_rpg_runtime_failures(&file)&&fingerprint==npe_game_fingerprint(&file));
    fprintf(report,"{\"phase\":\"complete\",\"frames\":%u,\"scripted_event\":%u,\"skill\":%u,\"npe_unchanged\":true}\n",frames,chosen_event,skill_id);
    assert(!fclose(report));SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);
    npe_font_free(&font);npe_free(&file);SDL_Quit();return 0;
}
