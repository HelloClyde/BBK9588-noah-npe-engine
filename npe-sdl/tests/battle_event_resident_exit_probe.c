#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef main
#undef main
#endif
#define main npe_app_main
#include "../main.c"
#undef main
typedef struct {const char *name;unsigned win,event,address;uint8_t battle[32],monster[56],alias[8],changed[32],changed_alias[8];unsigned mode,next;} BattleEventExitCase;
#include "battle_event_resident_exit_native.h"
int main(int argc,char **argv){
    assert(argc==2);NpeFile f;uint8_t *bytes;size_t size;char error[128];
    assert(!read_file(argv[1],&bytes,&size)&&!npe_parse(&f,bytes,size,error,sizeof(error)));
    uint64_t fingerprint=npe_game_fingerprint(&f);
    for(unsigned i=0;i<6;++i){const BattleEventExitCase *c=&battle_event_exit_cases[i];NpeWorld w;
        assert(!npe_world_init(&f,&w,9,29,13));w.game_mode=3;w.story_event_id=(uint16_t)c->event;w.story_progress=6;w.music_enabled=0;
        w.party_active[0]=1;w.party_active[1]=w.party_active[2]=0;
        assert(!npe_world_resources_open(&f,&w));
        assert(!npe_rpg_runtime_write(&f,0x5277fc,c->battle,32)&&!npe_rpg_runtime_write(&f,0x52761a,c->monster,56));
        assert(!npe_rpg_runtime_write(&f,0x520000+c->address,c->alias,8));
        assert(npe_world_prepare_scripted_battle(&f,&w)==1&&w.battle_win_event_id==0x1234&&w.battle_lose_event_id==0x4321);
        assert(!npe_rpg_runtime_write(&f,0x5277fc,c->changed,32)&&!npe_rpg_runtime_write(&f,0x520000+c->address,c->changed_alias,8));
        w.hp[0]=(uint16_t)(c->win?130:1);w.attack[0]=(uint16_t)(c->win?1000:8);w.other_stats[0][1]=(uint16_t)(c->win?999:1);w.defense[0]=0;
        for(unsigned e=0;e<3;++e){w.battle_enemy_hps[e]=(uint16_t)(e==1?(c->win?1:30000):0);
            w.battle_enemy_attacks[e]=(uint16_t)(c->win?1:500);w.battle_enemy_defenses[e]=0;w.battle_enemy_speeds[e]=(uint16_t)(c->win?1:999);
            memset(w.battle_enemy_skill_ids[e],0,3);w.battle_enemy_experience_rewards[e]=w.battle_enemy_gold_rewards[e]=0;memset(w.battle_enemy_drop_ids[e],0,2);}
        int result=c->win?npe_world_battle_attack(&w,1):npe_world_battle_defend(&w);
        fprintf(stderr,"battle event %s result%d mode%u event%u\n",c->name,result,w.game_mode,w.story_event_id);
        assert(result==(c->win?NPE_BATTLE_WON:NPE_BATTLE_LOST)&&w.game_mode==c->mode&&w.story_event_id==c->next&&w.map_init_pending==1&&!w.battle_enemy_count);
        uint8_t raw[32],alias[8];assert(!npe_rpg_runtime_read(&f,0x5277fc,raw,32)&&!memcmp(raw,c->changed,32));
        assert(!npe_rpg_runtime_read(&f,0x520000+c->address,alias,8)&&!memcmp(alias,c->changed_alias,8));
        assert(!npe_rpg_runtime_failures(&f));npe_rpg_runtime_close(&f);
    }
    assert(npe_game_fingerprint(&f)==fingerprint);npe_free(&f);
    puts("Battle event exit:6 original real-command endpoints,live win/loss branches,full event words,unchanged resident table passed");return 0;
}
