#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "empty_item_native.h"

static unsigned checked;
static NpeWorld fixture(const NpeFile *file,unsigned party,int two_enemies) {
    NpeWorld w;
    assert(npe_world_init(file,&w,9,29,13)==0);
    w.game_mode=3;w.story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,&w)==1);
    memset(w.condition_words,0,sizeof(w.condition_words));
    for(unsigned i=0;i<party;++i) {
        w.party_active[i]=1;w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];w.hp[i]=w.max_hp[i]=10000;
        w.attack[i]=15;w.defense[i]=w.battle_party_defenses[i]=13;w.other_stats[i][1]=999;
    }
    if(two_enemies) {
#define COPY_ENEMY(field) w.field[0]=w.field[1]
        COPY_ENEMY(battle_enemy_ids);COPY_ENEMY(battle_enemy_hps);
        COPY_ENEMY(battle_enemy_max_hps);COPY_ENEMY(battle_enemy_attacks);
        COPY_ENEMY(battle_enemy_defenses);COPY_ENEMY(battle_enemy_speeds); COPY_ENEMY(battle_enemy_lucks);
        COPY_ENEMY(battle_enemy_damage_floor_stats);COPY_ENEMY(battle_enemy_spell_stats);
#undef COPY_ENEMY
        memcpy(w.battle_enemy_skill_ids[0],w.battle_enemy_skill_ids[1],3);
    }
    npe_world_seed(&w,1);return w;
}
static void phase(const NpeWorld *w,const char *variant,const char *label,unsigned occurrence) {
    const NativeEmptyPhase *p=NULL;
    for(unsigned i=0;i<sizeof(native_empty_phases)/sizeof(*native_empty_phases);++i)
        if(!strcmp(native_empty_phases[i].variant,variant) &&
           !strcmp(native_empty_phases[i].label,label) && native_empty_phases[i].occurrence==occurrence)
            p=&native_empty_phases[i];
    if(!p || w->battle_actor!=p->actor || w->game_mode!=p->mode || npe_world_inventory_word(w,255)!=p->name)
        fprintf(stderr,"Mismatch %s/%s: actor%u mode%u name%u; expected %u/%u/%u\n",
                variant,label,w->battle_actor,w->game_mode,npe_world_inventory_word(w,255),
                p?p->actor:99,p?p->mode:99,p?p->name:99);
    assert(p && w->battle_actor==p->actor && w->game_mode==p->mode && npe_world_inventory_word(w,255)==p->name);
    assert(((uint16_t)((unsigned)w->battle_names[0][0]<<8|w->battle_names[0][1]))==49853);
    for(unsigned i=0;i<255;++i) assert(!w->condition_words[i]);
    ++checked;
    fprintf(stderr,"Native empty phase %s/%s/%u: passed\n",variant,label,occurrence);
}
static void probe(const NpeFile *file) {
    NpeItem item;
    assert(npe_item(file,254,&item)==-1);
    NpeIndexedBitmap icon;
    int icon_result=npe_battle_item_bitmap(file,255,&icon);
    fprintf(stderr,"Native item255 image lookup: %d, %ux%u\n",icon_result,
            icon_result?0:icon.width,icon_result?0:icon.height);
    assert(npe_battle_item(file,255,&item)==0 && !item.name_size &&
           item.raw==file->bytes+file->sections[2].offset);
    assert(native_empty_item[0]==255 && item.effect_type==native_empty_item[1] &&
           item.flags==native_empty_item[2] && item.group==native_empty_item[3] &&
           item.target_side==native_empty_item[4] && item.hp_restore==native_empty_item[5] &&
           item.qi_restore==native_empty_item[6] && item.attack_bonus==(int)native_empty_item[7]);
    for(unsigned id=1;id<=file->offset_table_records[2];++id) {
        NpeItem normal,native;
        assert(npe_item(file,id-1,&normal)==0 && npe_battle_item(file,id,&native)==0);
        assert(normal.raw==native.raw && normal.name_size==native.name_size &&
               normal.description_size==native.description_size);
    }
    NpeFile truncated=*file;truncated.size=file->sections[2].offset+254*4+2;
    assert(npe_battle_item(&truncated,255,&item)==-1);
    assert(npe_battle_item(file,0,&item)==-1 && npe_battle_item(file,256,&item)==-1);
    /* Isolated byte-level decoder fixture: offset255 points at the real herb.
       This proves the implementation reads the file offset, not a type0 stub. */
    NpeFile redirected=*file;
    redirected.bytes=malloc(file->size);assert(redirected.bytes);
    memcpy(redirected.bytes,file->bytes,file->size);
    uint8_t *table=redirected.bytes+file->sections[2].offset;
    memcpy(table+254*4,table+45*4,4);
    assert(npe_battle_item(&redirected,255,&item)==0 && item.effect_type==1 &&
           item.hp_restore==40 && item.target_side==1);
    free(redirected.bytes);

    NpeWorld w=fixture(file,2,1);phase(&w,"cancel","entry",0);
    for(unsigned i=0;i<2;++i) {
        assert(npe_world_battle_reserve_throw_item(file,&w,255)==0);
        phase(&w,"cancel",i?"empty-again":"empty-confirm",0);
        assert(w.battle_item_reservations[0]==255);
        assert(npe_world_battle_cancel_item_selection(&w)==1);
        phase(&w,"cancel",i?"cancel-again":"cancel",0);
    }
    w=fixture(file,2,0);phase(&w,"back","entry",0);
    assert(npe_world_battle_throw_item(file,&w,255,1)==NPE_BATTLE_CONTINUE);
    phase(&w,"back","empty-confirm",0);
    assert(npe_world_battle_previous_actor(&w)==1);phase(&w,"back","previous",0);
    w=fixture(file,2,0);phase(&w,"use","entry",0);
    assert(npe_world_battle_reserve_use_item(file,&w,255)==-2);
    phase(&w,"use","empty-confirm",0);
    const char *variants[]={"victory","success-flee","loss","failed-flee","group","skip"};
    for(unsigned i=0;i<sizeof(variants)/sizeof(*variants);++i) {
        const char *v=variants[i];w=fixture(file,2,0);phase(&w,v,"entry",0);
        assert(npe_world_battle_throw_item(file,&w,255,1)==NPE_BATTLE_CONTINUE);
        phase(&w,v,"empty-confirm",0);
        if(i==3) assert(npe_world_battle_flee(&w)==NPE_BATTLE_CONTINUE);
        else if(i==4) assert(npe_world_battle_group_attack(&w)==NPE_BATTLE_CONTINUE);
        else {if(i==5) w.hp[0]=0;assert(npe_world_battle_defend(&w)==NPE_BATTLE_CONTINUE);}
        phase(&w,v,"settled",0);
        assert(!w.battle_item_reservations[0] && !w.battle_pending_mask);
        if(i==0) {
            w.attack[0]=w.attack[1]=999;
            assert(npe_world_battle_flee(&w)==NPE_BATTLE_WON);
            phase(&w,v,"battle-exited",0);
        }else if(i==1) {
            w.story_event_id=0;npe_world_seed(&w,1);
            assert(npe_world_battle_flee(&w)==NPE_BATTLE_FLED);
            phase(&w,v,"flee-result",0);
        }else if(i==2) {
            w.hp[0]=w.hp[1]=1;w.battle_enemy_base_attacks[1] = w.battle_enemy_attacks[1]=999;w.battle_enemy_speeds[1]=9999;
            int result=NPE_BATTLE_CONTINUE;
            for(unsigned attempt=0;attempt<20 && result==NPE_BATTLE_CONTINUE;++attempt)
                result=npe_world_battle_flee(&w);
            assert(result==NPE_BATTLE_LOST);
            /* Native samples include the dispatcher after battle loss.
               SDL handles this mode21 transition with the same C service. */
            if(w.game_mode==21) assert(npe_world_finish_game(&w)==1);
            phase(&w,v,"loss-result",0);
        }
    }
    w=fixture(file,3,0);phase(&w,"three","entry",0);
    for(unsigned i=0;i<3;++i) {
        assert(npe_world_battle_throw_item(file,&w,255,1)==NPE_BATTLE_CONTINUE);
        phase(&w,"three",i?"empty-next":"empty-confirm",i==2?1:0);
    }
    phase(&w,"three","settled",0);
    assert(w.battle_enemy_hps[1]==500 && !w.battle_pending_mask);
    printf("Native empty-item loader and %u phases: alias, refunds, queue, overwrite, skip and three exits passed\n",checked);
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *input=fopen(argv[1],"rb");assert(input);
    assert(fseek(input,0,SEEK_END)==0);long size=ftell(input);assert(size>0 && fseek(input,0,SEEK_SET)==0);
    uint8_t *bytes=malloc((size_t)size);assert(bytes && fread(bytes,1,(size_t)size,input)==(size_t)size);
    assert(fclose(input)==0);NpeFile file;char error[128];
    assert(npe_parse(&file,bytes,(size_t)size,error,sizeof(error))==0);
    assert(npe_game_fingerprint(&file)==UINT64_C(0x114861a601117d94));
    probe(&file);npe_free(&file);return 0;
}
