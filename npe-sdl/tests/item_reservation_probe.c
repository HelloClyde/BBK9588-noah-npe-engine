#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "item_reservation_native.h"

static NpeWorld fixture(const NpeFile *file, unsigned party, unsigned quantity) {
    NpeWorld world;
    assert(npe_world_init(file,&world,9,29,13)==0);
    /* Native fixtures copy the party records before battle entry.
       Entry now fixes the physical party prefix for subsequent copies. */
    for(unsigned i=0;i<3;++i)world.party_active[i]=(uint8_t)(i<party);
    world.game_mode=3;world.story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,&world)==1);
#define COPY_ENEMY(field) world.field[0]=world.field[1]
    COPY_ENEMY(battle_enemy_ids);COPY_ENEMY(battle_enemy_hps);
    COPY_ENEMY(battle_enemy_max_hps);COPY_ENEMY(battle_enemy_attacks);
    COPY_ENEMY(battle_enemy_defenses);COPY_ENEMY(battle_enemy_speeds); COPY_ENEMY(battle_enemy_lucks);
    COPY_ENEMY(battle_enemy_damage_floor_stats);COPY_ENEMY(battle_enemy_spell_stats);
#undef COPY_ENEMY
    memcpy(world.battle_enemy_skill_ids[0],world.battle_enemy_skill_ids[1],3);
    for(unsigned i=0;i<party;++i) {
        world.party_active[i]=1;world.hp[i]=world.max_hp[i]=10000;
        world.other_stats[i][1]=999;
    }
    world.condition_words[159]=(uint16_t)quantity;
    npe_world_seed(&world,1);
    return world;
}
static void phase(const NpeWorld *world,const char *variant,unsigned party,
                  unsigned initial,const char *label,unsigned occurrence,int enemy_check) {
    const NativeItemPhase *expected=NULL;
    for(unsigned i=0;i<sizeof(native_item_phases)/sizeof(native_item_phases[0]);++i) {
        const NativeItemPhase *p=&native_item_phases[i];
        if(!strcmp(p->variant,variant) && !strcmp(p->label,label) &&
            p->party==party && p->initial==initial && p->occurrence==occurrence) expected=p;
    }
    assert(expected && world->battle_actor==expected->actor &&
        world->condition_words[159]==expected->quantity);
    if(enemy_check) assert(world->battle_enemy_hps[0]==expected->enemies[0] &&
        world->battle_enemy_hps[1]==expected->enemies[1]);
    fprintf(stderr,"Native item phase %s party%u qty%u %s/%u: passed\n",
        variant,party,initial,label,occurrence);
}
static void probe(const NpeFile *file) {
    NpeWorld world=fixture(file,2,1);
    phase(&world,"cancel",2,1,"entry",0,1);
    for(unsigned i=0;i<2;++i) {
        assert(npe_world_battle_reserve_throw_item(file,&world,159)==0);
        phase(&world,"cancel",2,1,i?"target-again":"target",0,1);
        assert(npe_world_battle_reserve_throw_item(file,&world,159)==-1);
        assert(npe_world_battle_cancel_item_selection(&world)==1);
        phase(&world,"cancel",2,1,i?"target-cancel-again":"target-cancel",0,1);
        assert(npe_world_battle_cancel_item_selection(&world)==0);
    }
    assert(!world.battle_pending_mask);
    for(unsigned party=2;party<=3;++party) {
        for(unsigned quantity=party==2?1:3;quantity<=party;++quantity) {
            world=fixture(file,party,quantity);
            for(unsigned i=0;i<party;++i) {
                if(i<quantity) {
                    assert(npe_world_battle_reserve_throw_item(file,&world,159)==0);
                    phase(&world,"settle",party,quantity,i?"target-next":"target",i?i-1:0,1);
                    assert(npe_world_battle_throw_item(file,&world,159,0)==NPE_BATTLE_CONTINUE);
                    if(i+1<party) phase(&world,"settle",party,quantity,i?"queued-next":"queued",i?i-1:0,1);
                } else {
                    assert(npe_world_battle_reserve_throw_item(file,&world,159)==-1);
                    assert(npe_world_battle_throw_item(file,&world,159,0)==-1);
                    assert(npe_world_battle_defend(&world)==NPE_BATTLE_CONTINUE);
                }
            }
            phase(&world,"settle",party,quantity,"settled",0,1);
            assert(!world.battle_pending_mask);
            for(unsigned i=0;i<3;++i) assert(!world.battle_item_reservations[i]);
        }
    }
    for(unsigned party=2;party<=3;++party) {
        world=fixture(file,party,party-1);
        for(unsigned i=0;i<party-1;++i)
            assert(npe_world_battle_throw_item(file,&world,159,0)==NPE_BATTLE_CONTINUE);
        assert(npe_world_battle_previous_actor(&world)==1);
        phase(&world,"actor-back",party,party-1,"actor-menu-back",0,1);
        if(party==3) assert(npe_world_battle_previous_actor(&world)==1);
        else assert(npe_world_battle_previous_actor(&world)==0);
        phase(&world,"actor-back",party,party-1,"actor-previous",0,1);
        assert(npe_world_battle_previous_actor(&world)==0);
        phase(&world,"actor-back",party,party-1,"actor-previous2",0,1);
        assert(!world.battle_pending_mask);
    }
    world=fixture(file,3,2);
    assert(npe_world_battle_throw_item(file,&world,159,0)==NPE_BATTLE_CONTINUE);
    assert(npe_world_battle_throw_item(file,&world,159,0)==NPE_BATTLE_CONTINUE);
    world.hp[1]=0;
    assert(npe_world_battle_previous_actor(&world)==1);
    phase(&world,"actor-back-dead",3,2,"actor-menu-back",0,1);
    assert(world.battle_item_reservations[1]==159 && !world.battle_item_reservations[0]);
    assert(npe_world_battle_previous_actor(&world)==0);
    phase(&world,"actor-back-dead",3,2,"actor-previous",0,1);

    world=fixture(file,2,1);
    assert(npe_world_battle_throw_item(file,&world,159,0)==NPE_BATTLE_CONTINUE);
    assert(npe_world_battle_group_attack(&world)==NPE_BATTLE_CONTINUE);
    phase(&world,"group",2,1,"group-settled",0,0); /* Attack RNG not synchronized. */
    assert(!world.battle_item_reservations[0]);
    world=fixture(file,2,1);
    assert(npe_world_battle_throw_item(file,&world,159,0)==NPE_BATTLE_CONTINUE);
    assert(npe_world_battle_flee(&world)==NPE_BATTLE_CONTINUE);
    phase(&world,"flee",2,1,"flee-settled",0,0); /* Only inventory audited here. */
    assert(!world.battle_item_reservations[0]);
    world=fixture(file,2,1);
    assert(npe_world_battle_throw_item(file,&world,159,0)==NPE_BATTLE_CONTINUE);
    world.hp[0]=0;
    assert(npe_world_battle_defend(&world)==NPE_BATTLE_CONTINUE);
    phase(&world,"skip",2,1,"skipped-settled",0,1);
    assert(!world.battle_item_reservations[0]);

    /* Native map saves are available after combat, not while an item is
       reserved or commands are pending. Existing NPESAV14 remains valid. */
    const char *path="tmp/item-reservation-complete.npes";
    world=fixture(file,2,1);
    assert(npe_world_battle_reserve_throw_item(file,&world,159)==0);
    assert(npe_world_save(file,&world,path)==-1 && world.battle_item_reservations[0]==159);
    assert(npe_world_battle_throw_item(file,&world,159,0)==NPE_BATTLE_CONTINUE);
    assert(npe_world_save(file,&world,path)==-1 && world.condition_words[159]==0);
    world.battle_enemy_hps[0]=40;world.battle_enemy_hps[1]=0;
    /* Victory re-reads52:77FC rather than the host's entry-time copy.
       This final check needs a map branch so it can exercise own saves. */
    const uint8_t map_win=1;
    assert(!npe_world_resident_write(&world,0x5277fd,&map_win,1));
    int result=npe_world_battle_defend(&world);
    fprintf(stderr,"Reservation save exit: result%d mode%u win-mode%u event%u enemyHP%u/%u/%u\n",
        result,world.game_mode,world.battle_win_mode,world.story_event_id,
        world.battle_enemy_hps[0],world.battle_enemy_hps[1],world.battle_enemy_hps[2]);
    assert(result==NPE_BATTLE_WON && world.game_mode==1);
    assert(!world.battle_pending_mask && !world.battle_item_reservations[0]);
    assert(npe_world_save(file,&world,path)==0);
    NpeWorld loaded;
    /*33:C13E..C23A refunds from the killing actor's record, inclusive.
      The original drop-same/refund-middle postwin fixtures confirm this. */
    int loaded_result=npe_world_load(file,&loaded,path);
    fprintf(stderr,"Reservation save read: result%d world-quantity%u loaded-quantity%u flags%u/%u/%u IDs%u/%u/%u\n",
        loaded_result,world.condition_words[159],loaded_result?0:loaded.condition_words[159],
        world.party_active[0],world.party_active[1],world.party_active[2],world.party_ids[0],world.party_ids[1],world.party_ids[2]);
    assert(loaded_result==0 && loaded.game_mode==1 && loaded.condition_words[159]==1);
    assert(!loaded.battle_pending_mask && !loaded.battle_item_reservations[0]);
    puts("Native throw reservation, cancellation, backtracking, discarded commands and map save passed");
}
int main(int argc,char **argv) {
    assert(argc==2);
    FILE *input=fopen(argv[1],"rb");assert(input && fseek(input,0,SEEK_END)==0);
    long size=ftell(input);assert(size>0 && fseek(input,0,SEEK_SET)==0);
    uint8_t *bytes=malloc((size_t)size);assert(bytes && fread(bytes,1,(size_t)size,input)==(size_t)size);
    assert(fclose(input)==0);
    NpeFile file;char error[128];
    assert(npe_parse(&file,bytes,(size_t)size,error,sizeof(error))==0);
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    probe(&file);free(bytes);return 0;
}
