/* Physical148-byte character records. Host shared fields represent the active
   party view; the map copy retains its own bytes while that view is battle. */
static uint16_t resident_record_word(const uint8_t *r,unsigned o){
    return (uint16_t)((unsigned)r[o]<<8|r[o+1]);
}
static int16_t resident_record_signed(const uint8_t *r,unsigned o){
    uint16_t v=resident_record_word(r,o);
    return v<0x8000u?(int16_t)v:(int16_t)((int32_t)v-0x10000);
}
static int resident_record_touched(unsigned offset,unsigned count,unsigned field,unsigned width){
    return count && offset<field+width && field<offset+count;
}
int npe_world_map_record_image(const NpeWorld *w,unsigned m,uint8_t r[148]){
    if(!w||!r||m>=3)return -1;
    memcpy(r,w->battle_party_flags_saved?w->battle_map_records[m]:w->party_record_residue[m],148);
    memcpy(r,w->party_names[m],12);r[12]=w->party_ids[m];
    r[14]=(uint8_t)(m?w->companion_directions[m-1]:w->direction);
    if(!m){
        r[15]=(uint8_t)w->view.screen_x;r[16]=(uint8_t)w->view.screen_y;
        /*124/125 start as NPE spawn offsets, then retain copied battle
          sprite coordinates. Current map scroll lives at7A2E/7A2C. */
    }
    const NpeCharacterMetadata *c=&w->party_characters[m];r[17]=c->sex;
    r[43]=w->battle_party_flags_saved?w->battle_map_party_active[m]:w->party_active[m];
    r[44]=w->party_attack_groups[m];
    for(unsigned i=0;i<8;++i)record_word(r,46+2*i,c->pose_ids[i]);
    for(unsigned i=0;i<4;++i)record_word(r,62+2*i,w->status_guard[m][i]);
    memcpy(r+136,c->animation_ids,3);
    if(!w->battle_party_flags_saved){
        memcpy(r+18,w->equipped_items[m],6);r[24]=w->level[m];
        memcpy(r+25,w->skill_slots[m],18);
        record_word(r,74,w->experience_to_next[m]);record_word(r,76,w->experience[m]);
        record_word(r,78,w->max_hp[m]);record_word(r,80,w->hp[m]);
        record_word(r,82,w->max_qi[m]);record_word(r,84,w->qi[m]);
        for(unsigned i=0;i<3;++i)record_word(r,86+2*i,w->other_stats[m][i]);
        record_word(r,92,w->attack[m]);record_word(r,94,w->defense[m]);
        memcpy(r+139,w->growth[m],7);
    }
    return 0;
}
static int resident_records_location(uint32_t address,unsigned count,unsigned *offset,int *battle){
    if(address>=UINT32_C(0x526450) && address<UINT32_C(0x5267c8)){
        *battle=1;*offset=(unsigned)(address-UINT32_C(0x526450));
        return count<=6u*148u-*offset?0:-1;
    }
    if(address>=UINT32_C(0x5269c6) && address<UINT32_C(0x526b82)){
        *battle=0;*offset=(unsigned)(address-UINT32_C(0x5269c6));
        return count<=3u*148u-*offset?0:-1;
    }
    return -1;
}
int npe_world_resident_records_read(const NpeWorld *w,uint32_t address,uint8_t *bytes,unsigned count){
    unsigned offset;int battle;
    if(!w||(!bytes&&count)||resident_records_location(address,count,&offset,&battle))return -1;
    while(count){
        uint8_t r[148];unsigned n=148-offset%148;if(n>count)n=count;
        int result=battle?npe_world_battle_record_image(w,offset/148,r):npe_world_map_record_image(w,offset/148,r);
        if(result)return -1;
        memcpy(bytes,r+offset%148,n);offset+=n;bytes+=n;count-=n;
    }
    return 0;
}
int npe_world_resident_records_write(NpeWorld *w,uint32_t address,const uint8_t *bytes,unsigned count){
    unsigned offset;int battle;
    if(!w||(!bytes&&count)||resident_records_location(address,count,&offset,&battle))return -1;
    while(count){
        unsigned n=148-offset%148;if(n>count)n=count;
        if(npe_world_record_write(w,battle,offset/148,offset%148,bytes,n))return -1;
        offset+=n;bytes+=n;count-=n;
    }
    return 0;
}
int npe_world_record_write(NpeWorld *w,int battle,unsigned slot,unsigned offset,
                           const uint8_t *bytes,unsigned count){
    if(!w||(!bytes&&count)||(battle!=0&&battle!=1)||slot>=(battle?6u:3u)||
       offset>148||count>148-offset)return -1;
    if(!count)return 0;
    uint8_t r[148];
    int result=battle?npe_world_battle_record_image(w,slot,r):npe_world_map_record_image(w,slot,r);
    if(result)return -1;
    memcpy(r+offset,bytes,count);
    memcpy(battle?w->battle_record_residue[slot]:w->party_record_residue[slot],r,148);
    if(!battle&&w->battle_party_flags_saved)memcpy(w->battle_map_records[slot],r,148);
#define TOUCHED(o,n) resident_record_touched(offset,count,o,n)
#define WORD(field,o) do{if(TOUCHED(o,2))field=resident_record_word(r,o);}while(0)
#define BYTE(field,o) do{if(TOUCHED(o,1))field=r[o];}while(0)
    unsigned party_count=world_record_party_count(w);
    if(!battle||slot<party_count){
        unsigned m=slot;NpeCharacterMetadata *c=battle?&w->battle_characters[m]:&w->party_characters[m];
        if(TOUCHED(0,12))memcpy(battle?w->battle_names[m]:w->party_names[m],r,12);
        if(battle){
            BYTE(w->battle_record_ids[m],12);
            BYTE(w->battle_party_record_active[m],43);BYTE(w->battle_attack_groups[m],44);
        }else{
            BYTE(w->party_ids[m],12);
            if(m)BYTE(w->companion_directions[m-1],14);
            else{
                BYTE(w->direction,14);BYTE(w->view.screen_x,15);BYTE(w->view.screen_y,16);
                if(TOUCHED(15,1))w->x=w->view.offset_x+w->view.screen_x;
                if(TOUCHED(16,1))w->y=w->view.offset_y+w->view.screen_y;
            }
            if(w->battle_party_flags_saved)BYTE(w->battle_map_party_active[m],43);
            else BYTE(w->party_active[m],43);
            BYTE(w->party_attack_groups[m],44);
        }
        BYTE(c->sex,17);
        for(unsigned i=0;i<8;++i)WORD(c->pose_ids[i],46+2*i);
        if(TOUCHED(136,3))memcpy(c->animation_ids,r+136,3);
        /* Shared numeric fields describe the active party. Writes into
           the dormant battle bank must not modify the map party. */
        if(battle?w->battle_party_flags_saved:!w->battle_party_flags_saved){
            if(TOUCHED(18,6))memcpy(w->equipped_items[m],r+18,6);
            BYTE(w->level[m],24);
            if(TOUCHED(25,18))memcpy(w->skill_slots[m],r+25,18);
            WORD(w->experience_to_next[m],74);WORD(w->experience[m],76);
            WORD(w->max_hp[m],78);WORD(w->hp[m],80);
            WORD(w->max_qi[m],82);WORD(w->qi[m],84);
            for(unsigned i=0;i<3;++i)WORD(w->other_stats[m][i],86+2*i);
            WORD(w->attack[m],92);WORD(w->defense[m],94);
            if(TOUCHED(139,7))memcpy(w->growth[m],r+139,7);
        }
        for(unsigned i=0;i<4;++i){
            if(battle){if(TOUCHED(62+2*i,2))w->battle_party_status[m][i]=resident_record_signed(r,62+2*i);}
            else WORD(w->status_guard[m][i],62+2*i);
        }
        if(battle){
            WORD(w->battle_attack_turns[m],70);WORD(w->battle_defense_turns[m],72);
            if(TOUCHED(92,2)||TOUCHED(96,2))
                w->battle_skill_attack_bonus[m]=(uint16_t)(resident_record_word(r,96)-w->attack[m]);
            if(TOUCHED(98,2)){
                w->battle_party_defenses[m]=resident_record_word(r,98);w->battle_skill_defense_bonus[m]=0;
            }
            BYTE(w->battle_native_targets[m],126);BYTE(w->battle_native_kinds[m],127);
            BYTE(w->battle_magic_types[m],128);BYTE(w->battle_item_types[m],129);
            BYTE(w->battle_item_type_copies[m],130);BYTE(w->battle_item_groups[m],131);
            BYTE(w->battle_item_sides[m],132);BYTE(w->battle_item_ids[m],133);BYTE(w->battle_skill_ids[m],134);
        }
    }else if(slot-party_count<3){
        unsigned e=slot-party_count;
        BYTE(w->battle_enemy_damage_floor_stats[e],24);
        if(TOUCHED(25,3))memcpy(w->battle_enemy_skill_ids[e],r+25,3);
        for(unsigned i=0;i<4;++i)if(TOUCHED(62+2*i,2))w->battle_enemy_status[e][i]=resident_record_signed(r,62+2*i);
        WORD(w->battle_enemy_attack_turns[e],70);WORD(w->battle_enemy_defense_turns[e],72);
        WORD(w->battle_enemy_experience_rewards[e],76);WORD(w->battle_enemy_max_hps[e],78);
        WORD(w->battle_enemy_hps[e],80);WORD(w->battle_enemy_max_qis[e],82);WORD(w->battle_enemy_qis[e],84);
        WORD(w->battle_enemy_spell_stats[e],86);WORD(w->battle_enemy_speeds[e],88);WORD(w->battle_enemy_lucks[e],90);
        WORD(w->battle_enemy_base_attacks[e],92);WORD(w->battle_enemy_base_defenses[e],94);
        WORD(w->battle_enemy_attacks[e],96);WORD(w->battle_enemy_defenses[e],98);WORD(w->battle_enemy_gold_rewards[e],100);
        if(TOUCHED(122,2))memcpy(w->battle_enemy_drop_ids[e],r+122,2);
        if(!e){w->battle_enemy_id=w->battle_enemy_ids[e];w->battle_enemy_hp=w->battle_enemy_hps[e];}
    }
    if(battle&&slot==5&&TOUCHED(146,2))w->inventory_prefix_word=resident_record_word(r,146);
#undef TOUCHED
#undef WORD
#undef BYTE
    return 0;
}
static void world_set_record_flag(NpeWorld *w,unsigned slot,uint8_t value){
    w->battle_record_residue[slot][146]=value;
    if(slot==5)w->inventory_prefix_word=(uint16_t)((unsigned)value<<8|(w->inventory_prefix_word&255u));
}
