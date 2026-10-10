/* Additional guest RAM owners. Cache bytes are authoritative; raw item/skill
   structs remain derived views and are rebuilt by their existing readers. */
#include "resident_ui_controls.inc"
/*6FBA has250 independent bytes. IDs250..255 alias the first viewport
  words at70B4 and remain runtime RAM, not duplicate host owners. */
static uint8_t *resident_npc_conditions(NpeWorld *w,uint32_t a,unsigned n){
    if(a>=UINT32_C(0x526fba)&&a<UINT32_C(0x5270b4)&&n<=UINT32_C(0x5270b4)-a)
        return w->npc_state+(a-UINT32_C(0x526fba));
    if(a>=UINT32_C(0x527186)&&a<UINT32_C(0x52724e)&&n<=UINT32_C(0x52724e)-a)
        return w->condition5_bytes+(a-UINT32_C(0x527186));
    return NULL;
}
static uint32_t resident_resource_end(uint32_t address){
    uint32_t ui_end=resident_ui_control_end(address);if(ui_end)return ui_end;
    if(address>=UINT32_C(0x526450)&&address<UINT32_C(0x526eb9))return UINT32_C(0x526eb9);
    if(address>=UINT32_C(0x52724e)&&address<UINT32_C(0x5275de))return UINT32_C(0x5275de);
    if(address>=UINT32_C(0x52761a)&&address<UINT32_C(0x527684))return UINT32_C(0x527684);
    if(address>=UINT32_C(0x5276b0)&&address<UINT32_C(0x5276d8))return UINT32_C(0x5276d8);
    if(address>=UINT32_C(0x5276da)&&address<UINT32_C(0x527702))return UINT32_C(0x527702);
    if(address>=UINT32_C(0x527728)&&address<UINT32_C(0x527750))return UINT32_C(0x527750);
    if(address>=UINT32_C(0x5277fc)&&address<UINT32_C(0x527856))return UINT32_C(0x527856);
    if(address>=UINT32_C(0x52788e)&&address<UINT32_C(0x5278b4))return UINT32_C(0x5278b4);
    if(address>=UINT32_C(0x52790a)&&address<UINT32_C(0x52791e))return UINT32_C(0x52791e);
    if(address>=UINT32_C(0x52792e)&&address<UINT32_C(0x527934))return UINT32_C(0x527934);
    if(address==UINT32_C(0x527a9a)||address==UINT32_C(0x527af0))return address+1;
    if(address==UINT32_C(0x527a9c)||address==UINT32_C(0x527aa2))return address+1;
    if(address==UINT32_C(0x527a9e)||address==UINT32_C(0x527aae))return address+1;
    if(address==UINT32_C(0x527ac8))return address+1;
    if(address==UINT32_C(0x527a04))return address+1;
    if(address>=UINT32_C(0x527934)&&address<UINT32_C(0x527936))return UINT32_C(0x527936);
    if(address>=UINT32_C(0x527ab4)&&address<UINT32_C(0x527ab6))return UINT32_C(0x527ab6);
    if(address==UINT32_C(0x527a54)||address==UINT32_C(0x527a8a))return address+1;
    if(address>=UINT32_C(0x527a50)&&address<UINT32_C(0x527a52))return UINT32_C(0x527a52);
    if(address==UINT32_C(0x527ac0))return address+1;
    if(address==UINT32_C(0x527aac))return address+1;
    if(address==UINT32_C(0x527aa8)||address==UINT32_C(0x527acc))return address+1;
    if(address>=UINT32_C(0x527aa4)&&address<UINT32_C(0x527aa6))return UINT32_C(0x527aa6);
    if(address>=UINT32_C(0x527aa0)&&address<UINT32_C(0x527aa2))return UINT32_C(0x527aa2);
    if(address>=UINT32_C(0x527aa6)&&address<UINT32_C(0x527aa8))return UINT32_C(0x527aa8);
    if(address>=UINT32_C(0x527aaa)&&address<UINT32_C(0x527aac))return UINT32_C(0x527aac);
    if(address>=UINT32_C(0x52799e)&&address<UINT32_C(0x5279a6))return UINT32_C(0x5279a6);
    if(address>=UINT32_C(0x527a02)&&address<UINT32_C(0x527a04))return UINT32_C(0x527a04);
    if(address>=UINT32_C(0x527a5c)&&address<UINT32_C(0x527a5e))return UINT32_C(0x527a5e);
    if(address>=UINT32_C(0x527a66)&&address<UINT32_C(0x527a68))return UINT32_C(0x527a68);
    if(address==UINT32_C(0x527a72)||address==UINT32_C(0x527a74))return address+1;
    if(address>=UINT32_C(0x527aba)&&address<UINT32_C(0x527abc))return UINT32_C(0x527abc);
    return 0;
}
static unsigned resident_resource_span(uint32_t address){
    if(address<UINT32_C(0x5267c8))return (unsigned)(UINT32_C(0x5267c8)-address);
    if(address<UINT32_C(0x5269c6))return 2u-(address&1u);
    if(address<UINT32_C(0x526b82))return (unsigned)(UINT32_C(0x526b82)-address);
    if(address<UINT32_C(0x526ca2))return (unsigned)(UINT32_C(0x526ca2)-address);
    if(address<UINT32_C(0x526dba))return (unsigned)(UINT32_C(0x526dba)-address);
    if(address<UINT32_C(0x526eb9))return (unsigned)(UINT32_C(0x526eb9)-address);
    if(address<UINT32_C(0x52730e))return (unsigned)(UINT32_C(0x52730e)-address);
    if(address<UINT32_C(0x52739e))return (unsigned)(UINT32_C(0x52739e)-address);
    if(address<UINT32_C(0x52742c))return (unsigned)(UINT32_C(0x52742c)-address);
    if(address<UINT32_C(0x5274f4))return (unsigned)(UINT32_C(0x5274f4)-address);
    if(address<UINT32_C(0x52754e))return (unsigned)(UINT32_C(0x52754e)-address);
    if(address<UINT32_C(0x52759e))return (unsigned)(UINT32_C(0x52759e)-address);
    if(address<UINT32_C(0x5275de))return (unsigned)(UINT32_C(0x5275de)-address);
    if(address<UINT32_C(0x527652))return (unsigned)(UINT32_C(0x527652)-address);
    if(address<UINT32_C(0x527684))return (unsigned)(UINT32_C(0x527684)-address);
    if(address<UINT32_C(0x5276d8))return (unsigned)(UINT32_C(0x5276d8)-address);
    if(address<UINT32_C(0x527702))return (unsigned)(UINT32_C(0x527702)-address);
    if(address<UINT32_C(0x527750))return (unsigned)(UINT32_C(0x527750)-address);
    if(address<UINT32_C(0x52781c))return (unsigned)(UINT32_C(0x52781c)-address);
    if(address<UINT32_C(0x52783a))return (unsigned)(UINT32_C(0x52783a)-address);
    if(address<UINT32_C(0x527856))return (unsigned)(UINT32_C(0x527856)-address);
    if(address<UINT32_C(0x5278b4))return (unsigned)(UINT32_C(0x5278b4)-address);
    if(address<UINT32_C(0x527916))return (unsigned)(UINT32_C(0x527916)-address);
    if(address<UINT32_C(0x52791e))return (unsigned)(UINT32_C(0x52791e)-address);
    if(address>=UINT32_C(0x52792e)&&address<UINT32_C(0x527934))return 2u-(address&1u);
    if((address>=UINT32_C(0x527934)&&address<UINT32_C(0x527936))||
       (address>=UINT32_C(0x527ab4)&&address<UINT32_C(0x527ab6)))return 2u-(address&1u);
    if(address>=UINT32_C(0x527aa4)&&address<UINT32_C(0x527aa6))return 2u-(address&1u);
    if((address>=UINT32_C(0x527aa0)&&address<UINT32_C(0x527aa2))||
       (address>=UINT32_C(0x527aa6)&&address<UINT32_C(0x527aa8))||
       (address>=UINT32_C(0x527aaa)&&address<UINT32_C(0x527aac)))return 2u-(address&1u);
    if(address<UINT32_C(0x5279a2))return (unsigned)(UINT32_C(0x5279a2)-address);
    if(address<UINT32_C(0x5279a6))return (unsigned)(UINT32_C(0x5279a6)-address);
    return 1;
}
int npe_world_resident_read(const NpeWorld *w,uint32_t address,uint8_t *bytes,unsigned count){
    if(!w||(!bytes&&count))return -1;
    if(!count)return 0;
    const uint8_t *npc=resident_npc_conditions((NpeWorld *)w,address,count);
    if(npc){memcpy(bytes,npc,count);return 0;}
    if(resident_ui_control_end(address))return resident_ui_control_read(w,address,bytes,count);
    uint32_t end=resident_resource_end(address);
    if(!end||count>end-address)return -1;
    while(count){
        unsigned n=resident_resource_span(address);if(n>count)n=count;
        if(address<UINT32_C(0x5267c8)||(address>=UINT32_C(0x5269c6)&&address<UINT32_C(0x526b82))){
            if(npe_world_resident_records_read(w,address,bytes,n))return -1;
        }else if(address<UINT32_C(0x5269c6)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5267c8));
            uint16_t word=w->condition_words[offset/2];
            for(unsigned i=0;i<n;++i)bytes[i]=(uint8_t)(((offset+i)&1u)?word:word>>8);
        }else if(address<UINT32_C(0x526ca2)){
            unsigned offset=(unsigned)(address-UINT32_C(0x526b82));
            for(unsigned i=0;i<n;++i)bytes[i]=w->exit_records[(offset+i)/24][(offset+i)%24];
        }else if(address<UINT32_C(0x526dba)){
            for(unsigned i=0;i<n;++i)bytes[i]=npc_record_byte(w,(unsigned)(address-UINT32_C(0x526ca2))+i);
        }else if(address<UINT32_C(0x526eb9)){
            memcpy(bytes,w->shop_catalog_present+address-UINT32_C(0x526dba),n);
        }else if(address<UINT32_C(0x52730e)){
            memcpy(bytes,w->tile_record_bytes+address-UINT32_C(0x52724e),n);
        }else if(address<UINT32_C(0x52739e)){
            memcpy(bytes,w->skill_record_bytes+address-UINT32_C(0x52730e),n);
        }else if(address<UINT32_C(0x52742c)){
            memcpy(bytes,w->item_record_bytes+address-UINT32_C(0x52739e),n);
        }else if(address<UINT32_C(0x5274f4)){
            memcpy(bytes,w->object_done+address-UINT32_C(0x52742c),n);
        }else if(address<UINT32_C(0x52754e)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5274f4));
            for(unsigned i=0;i<n;++i)bytes[i]=story_event_byte(w,offset+i);
        }else if(address<UINT32_C(0x5275de)){
            if(address<UINT32_C(0x52759e)){memcpy(bytes,(const uint8_t *)w->scene_event_records+address-UINT32_C(0x52754e),n);}
            else{
            unsigned offset=(unsigned)(address-UINT32_C(0x52759e));
            for(unsigned i=0;i<n;++i)bytes[i]=w->object_records[(offset+i)/8][(offset+i)%8];
            }
        }else if(address<UINT32_C(0x527652)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52761a));
            for(unsigned i=0;i<n;++i)bytes[i]=w->monster_records[(offset+i)/14][(offset+i)%14];
        }else if(address<UINT32_C(0x527684)){
            unsigned offset=(unsigned)(address-UINT32_C(0x527652));
            for(unsigned i=0;i<n;++i)bytes[i]=move_event_byte(w,0,offset+i);
        }else if(address<UINT32_C(0x5276d8)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5276b0));
            for(unsigned i=0;i<n;++i)bytes[i]=money_item_event_byte(w,0,offset+i);
        }else if(address<UINT32_C(0x527702)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5276da));
            for(unsigned i=0;i<n;++i)bytes[i]=money_item_event_byte(w,1,offset+i);
        }else if(address<UINT32_C(0x527750)){
            unsigned offset=(unsigned)(address-UINT32_C(0x527728));
            for(unsigned i=0;i<n;++i)bytes[i]=move_event_byte(w,1,offset+i);
        }else if(address<UINT32_C(0x52781c)){
            memcpy(bytes,(const uint8_t *)w->battle_event_records+address-UINT32_C(0x5277fc),n);
        }else if(address<UINT32_C(0x52783a)){
            memcpy(bytes,(const uint8_t *)w->choice_event_records+address-UINT32_C(0x52781c),n);
        }else if(address<UINT32_C(0x527856)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52783a));
            for(unsigned i=0;i<n;++i)bytes[i]=chase_record_byte(w,offset+i);
        }else if(address<UINT32_C(0x5278b4)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52788e));
            for(unsigned i=0;i<n;++i)bytes[i]=party_teleport_byte(w,offset+i);
        }else if(address<UINT32_C(0x527916)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52790a));
            for(unsigned i=0;i<n;++i)bytes[i]=skill_adjust_event_byte(w,0,offset+i);
        }else if(address<UINT32_C(0x52791e)){
            memcpy(bytes,w->random_enemy_records+address-UINT32_C(0x527916),n);
        }else if(address>=UINT32_C(0x52792e)&&address<UINT32_C(0x527934)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52792e));uint16_t v=w->battle_selected_ids[offset/2];
            for(unsigned i=0;i<n;++i)bytes[i]=(uint8_t)(((offset+i)&1u)?v:v>>8);
        }else if(address>=UINT32_C(0x527934)&&address<UINT32_C(0x527936)){
            for(unsigned i=0;i<n;++i)bytes[i]=(uint8_t)(((address+i)&1u)?w->game_mode:w->game_mode>>8);
        }else if(address<UINT32_C(0x5279a2)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52799e));
            for(unsigned i=0;i<n;++i)bytes[i]=(uint8_t)(w->money>>((3-offset-i)*8));
        }else if(address<UINT32_C(0x5279a6)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5279a2));
            for(unsigned i=0;i<n;++i)bytes[i]=skill_adjust_event_byte(w,1,offset+i);
        }else if(address>=UINT32_C(0x527a02)&&address<UINT32_C(0x527a04)){
            bytes[0]=(uint8_t)((address&1u)?w->encounter_steps:w->encounter_steps>>8);
        }else if(address==UINT32_C(0x527a04)){
            bytes[0]=w->map_music_slot;
        }else if(address>=UINT32_C(0x527a50)&&address<UINT32_C(0x527a52)){
            bytes[0]=(uint8_t)((address&1u)?w->music_enabled:w->music_enabled>>8);
        }else if(address>=UINT32_C(0x527a5c)&&address<UINT32_C(0x527a5e)){
            bytes[0]=(uint8_t)((address&1u)?w->encounters_enabled:w->encounters_enabled>>8);
        }else if(address>=UINT32_C(0x527a66)&&address<UINT32_C(0x527a68)){
            bytes[0]=(uint8_t)((address&1u)?w->story_progress:w->story_progress>>8);
        }else if(address>=UINT32_C(0x527ab4)&&address<UINT32_C(0x527ab6)){
            for(unsigned i=0;i<n;++i)bytes[i]=(uint8_t)(((address+i)&1u)?w->map_init_pending:w->map_init_pending>>8);
        }else if(address==UINT32_C(0x527a54))bytes[0]=w->system_menu_stage;
        else if(address==UINT32_C(0x527a8a))bytes[0]=w->music_choice;
        else if(address==UINT32_C(0x527a9a))bytes[0]=w->battle_scripted_entry;
        else if(address==UINT32_C(0x527a9c))bytes[0]=w->battle_menu_row;
        else if(address==UINT32_C(0x527aa2))bytes[0]=w->battle_item_row;
        else if(address==UINT32_C(0x527a9e))bytes[0]=w->battle_actor;
        else if(address==UINT32_C(0x527aae))bytes[0]=w->battle_menu_previous_kind;
        else if(address==UINT32_C(0x527ac8))bytes[0]=w->battle_menu_phase;
        else if(address==UINT32_C(0x527ac0))bytes[0]=w->battle_display_target;
        else if(address==UINT32_C(0x527aac))bytes[0]=w->battle_status_member;
        else if(address==UINT32_C(0x527aa8))bytes[0]=w->battle_action_ready;
        else if(address==UINT32_C(0x527acc))bytes[0]=w->battle_execution_slot;
        else if((address>=UINT32_C(0x527aa0)&&address<UINT32_C(0x527aa2))||
                (address>=UINT32_C(0x527aa6)&&address<UINT32_C(0x527aa8))||
                (address>=UINT32_C(0x527aaa)&&address<UINT32_C(0x527aac))){
            uint16_t word=address<UINT32_C(0x527aa2)?w->battle_selection_item_type:
                address<UINT32_C(0x527aa8)?w->battle_selection_magic_type:w->battle_selection_type_copy;
            for(unsigned i=0;i<n;++i)bytes[i]=(uint8_t)(((address+i)&1u)?word:word>>8);
        }
        else if(address>=UINT32_C(0x527aa4)&&address<UINT32_C(0x527aa6)){
            for(unsigned i=0;i<n;++i)bytes[i]=(uint8_t)(((address+i)&1u)?w->battle_resource_flags:w->battle_resource_flags>>8);
        }
        else if(address==UINT32_C(0x527af0))bytes[0]=w->battle_formation_count;
        else if(address==UINT32_C(0x527a72))bytes[0]=w->item_record_id;
        else if(address==UINT32_C(0x527a74))bytes[0]=w->skill_record_id;
        else bytes[0]=(uint8_t)((address&1u)?w->item_description_length:w->item_description_length>>8);
        address+=n;bytes+=n;count-=n;
    }
    return 0;
}
int npe_world_resident_write(NpeWorld *w,uint32_t address,const uint8_t *bytes,unsigned count){
    if(!w||(!bytes&&count))return -1;
    if(!count)return 0;
    uint8_t *npc=resident_npc_conditions(w,address,count);
    if(npc){memcpy(npc,bytes,count);return 0;}
    if(resident_ui_control_end(address))return resident_ui_control_write(w,address,bytes,count);
    uint32_t end=resident_resource_end(address);
    if(!end||count>end-address)return -1;
    while(count){
        unsigned n=resident_resource_span(address);if(n>count)n=count;
        if(address<UINT32_C(0x5267c8)||(address>=UINT32_C(0x5269c6)&&address<UINT32_C(0x526b82))){
            if(npe_world_resident_records_write(w,address,bytes,n))return -1;
        }else if(address<UINT32_C(0x5269c6)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5267c8));
            uint16_t word=w->condition_words[offset/2];
            for(unsigned i=0;i<n;++i)word=(uint16_t)(((offset+i)&1u)?
                (word&0xff00u)|bytes[i]:(word&255u)|((unsigned)bytes[i]<<8));
            w->condition_words[offset/2]=word;
        }else if(address<UINT32_C(0x526ca2)){
            unsigned offset=(unsigned)(address-UINT32_C(0x526b82));
            for(unsigned i=0;i<n;++i)w->exit_records[(offset+i)/24][(offset+i)%24]=bytes[i];
            w->exit_records_ready=1;
        }else if(address<UINT32_C(0x526dba)){
            for(unsigned i=0;i<n;++i)npc_record_byte_write(w,(unsigned)(address-UINT32_C(0x526ca2))+i,bytes[i]);
        }else if(address<UINT32_C(0x526eb9)){
            memcpy(w->shop_catalog_present+address-UINT32_C(0x526dba),bytes,n);
            w->shop_catalog_ready=1;
        }else if(address<UINT32_C(0x52730e)){
            memcpy(w->tile_record_bytes+address-UINT32_C(0x52724e),bytes,n);
        }else if(address<UINT32_C(0x52739e)){
            memcpy(w->skill_record_bytes+address-UINT32_C(0x52730e),bytes,n);
        }else if(address<UINT32_C(0x52742c)){
            memcpy(w->item_record_bytes+address-UINT32_C(0x52739e),bytes,n);
        }else if(address<UINT32_C(0x5274f4)){
            memcpy(w->object_done+address-UINT32_C(0x52742c),bytes,n);
        }else if(address<UINT32_C(0x52754e)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5274f4));
            for(unsigned i=0;i<n;++i)story_event_byte_write(w,offset+i,bytes[i]);
        }else if(address<UINT32_C(0x5275de)){
            if(address<UINT32_C(0x52759e)){memcpy((uint8_t *)w->scene_event_records+address-UINT32_C(0x52754e),bytes,n);}
            else{
            unsigned offset=(unsigned)(address-UINT32_C(0x52759e));
            for(unsigned i=0;i<n;++i)w->object_records[(offset+i)/8][(offset+i)%8]=bytes[i];
            w->object_records_ready=1;
            }
        }else if(address<UINT32_C(0x527652)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52761a));
            for(unsigned i=0;i<n;++i){unsigned o=offset+i;
                w->monster_records[o/14][o%14]=bytes[i];monster_record_view(w,o/14);
            }
            w->map_encounter_records_ready=1;
        }else if(address<UINT32_C(0x527684)){
            unsigned offset=(unsigned)(address-UINT32_C(0x527652));
            for(unsigned i=0;i<n;++i)move_event_byte_write(w,0,offset+i,bytes[i]);
        }else if(address<UINT32_C(0x5276d8)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5276b0));
            for(unsigned i=0;i<n;++i)money_item_event_byte_write(w,0,offset+i,bytes[i]);
        }else if(address<UINT32_C(0x527702)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5276da));
            for(unsigned i=0;i<n;++i)money_item_event_byte_write(w,1,offset+i,bytes[i]);
        }else if(address<UINT32_C(0x527750)){
            unsigned offset=(unsigned)(address-UINT32_C(0x527728));
            for(unsigned i=0;i<n;++i)move_event_byte_write(w,1,offset+i,bytes[i]);
        }else if(address<UINT32_C(0x52781c)){
            memcpy((uint8_t *)w->battle_event_records+address-UINT32_C(0x5277fc),bytes,n);
        }else if(address<UINT32_C(0x52783a)){
            memcpy((uint8_t *)w->choice_event_records+address-UINT32_C(0x52781c),bytes,n);
        }else if(address<UINT32_C(0x527856)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52783a));
            for(unsigned i=0;i<n;++i)chase_record_byte_write(w,offset+i,bytes[i]);
        }else if(address<UINT32_C(0x5278b4)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52788e));
            for(unsigned i=0;i<n;++i)party_teleport_byte_write(w,offset+i,bytes[i]);
        }else if(address<UINT32_C(0x527916)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52790a));
            for(unsigned i=0;i<n;++i)skill_adjust_event_byte_write(w,0,offset+i,bytes[i]);
        }else if(address<UINT32_C(0x52791e)){
            memcpy(w->random_enemy_records+address-UINT32_C(0x527916),bytes,n);
        }else if(address>=UINT32_C(0x52792e)&&address<UINT32_C(0x527934)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52792e));uint16_t v=w->battle_selected_ids[offset/2];
            for(unsigned i=0;i<n;++i)v=(uint16_t)(((offset+i)&1u)?(v&0xff00u)|bytes[i]:(v&255u)|((unsigned)bytes[i]<<8));
            w->battle_selected_ids[offset/2]=v;
        }else if(address>=UINT32_C(0x527934)&&address<UINT32_C(0x527936)){
            for(unsigned i=0;i<n;++i)w->game_mode=(uint16_t)(((address+i)&1u)?
                (w->game_mode&0xff00u)|bytes[i]:(w->game_mode&255u)|((unsigned)bytes[i]<<8));
        }else if(address<UINT32_C(0x5279a2)){
            unsigned offset=(unsigned)(address-UINT32_C(0x52799e));
            for(unsigned i=0;i<n;++i){unsigned shift=(3-offset-i)*8;
                w->money=(w->money&~(UINT32_C(255)<<shift))|((uint32_t)bytes[i]<<shift);}
        }else if(address<UINT32_C(0x5279a6)){
            unsigned offset=(unsigned)(address-UINT32_C(0x5279a2));
            for(unsigned i=0;i<n;++i)skill_adjust_event_byte_write(w,1,offset+i,bytes[i]);
        }else if(address>=UINT32_C(0x527a02)&&address<UINT32_C(0x527a04)){
            w->encounter_steps=(uint16_t)((address&1u)?(w->encounter_steps&0xff00u)|bytes[0]:
                (w->encounter_steps&255u)|((unsigned)bytes[0]<<8));
        }else if(address==UINT32_C(0x527a04)){
            w->map_music_slot=bytes[0];
        }else if(address>=UINT32_C(0x527a50)&&address<UINT32_C(0x527a52)){
            w->music_enabled=(uint16_t)((address&1u)?(w->music_enabled&0xff00u)|bytes[0]:
                (w->music_enabled&255u)|((unsigned)bytes[0]<<8));
        }else if(address>=UINT32_C(0x527a5c)&&address<UINT32_C(0x527a5e)){
            w->encounters_enabled=(uint16_t)((address&1u)?(w->encounters_enabled&0xff00u)|bytes[0]:
                (w->encounters_enabled&255u)|((unsigned)bytes[0]<<8));
        }else if(address>=UINT32_C(0x527a66)&&address<UINT32_C(0x527a68)){
            w->story_progress=(uint16_t)((address&1u)?(w->story_progress&0xff00u)|bytes[0]:
                (w->story_progress&255u)|((unsigned)bytes[0]<<8));
        }else if(address>=UINT32_C(0x527ab4)&&address<UINT32_C(0x527ab6)){
            for(unsigned i=0;i<n;++i)w->map_init_pending=(uint16_t)(((address+i)&1u)?
                (w->map_init_pending&0xff00u)|bytes[i]:(w->map_init_pending&255u)|((unsigned)bytes[i]<<8));
        }else if(address==UINT32_C(0x527a54))w->system_menu_stage=bytes[0];
        else if(address==UINT32_C(0x527a8a))w->music_choice=bytes[0];
        else if(address==UINT32_C(0x527a9a))w->battle_scripted_entry=bytes[0];
        else if(address==UINT32_C(0x527a9c))w->battle_menu_row=bytes[0];
        else if(address==UINT32_C(0x527aa2))w->battle_item_row=bytes[0];
        else if(address==UINT32_C(0x527a9e))w->battle_actor=bytes[0];
        else if(address==UINT32_C(0x527aae))w->battle_menu_previous_kind=bytes[0];
        else if(address==UINT32_C(0x527ac8))w->battle_menu_phase=bytes[0];
        else if(address==UINT32_C(0x527ac0))w->battle_display_target=bytes[0];
        else if(address==UINT32_C(0x527aac))w->battle_status_member=bytes[0];
        else if(address==UINT32_C(0x527aa8))w->battle_action_ready=bytes[0];
        else if(address==UINT32_C(0x527acc))w->battle_execution_slot=bytes[0];
        else if((address>=UINT32_C(0x527aa0)&&address<UINT32_C(0x527aa2))||
                (address>=UINT32_C(0x527aa6)&&address<UINT32_C(0x527aa8))||
                (address>=UINT32_C(0x527aaa)&&address<UINT32_C(0x527aac))){
            uint16_t *word=address<UINT32_C(0x527aa2)?&w->battle_selection_item_type:
                address<UINT32_C(0x527aa8)?&w->battle_selection_magic_type:&w->battle_selection_type_copy;
            for(unsigned i=0;i<n;++i)*word=(uint16_t)(((address+i)&1u)?
                (*word&0xff00u)|bytes[i]:(*word&255u)|((unsigned)bytes[i]<<8));
        }
        else if(address>=UINT32_C(0x527aa4)&&address<UINT32_C(0x527aa6)){
            for(unsigned i=0;i<n;++i)w->battle_resource_flags=(uint16_t)(((address+i)&1u)?
                (w->battle_resource_flags&0xff00u)|bytes[i]:(w->battle_resource_flags&255u)|((unsigned)bytes[i]<<8));
        }
        else if(address==UINT32_C(0x527af0))w->battle_formation_count=bytes[0];
        else if(address==UINT32_C(0x527a72))w->item_record_id=bytes[0];
        else if(address==UINT32_C(0x527a74))w->skill_record_id=bytes[0];
        else w->item_description_length=(uint16_t)((address&1u)?
            (w->item_description_length&0xff00u)|bytes[0]:
            (w->item_description_length&255u)|((unsigned)bytes[0]<<8));
        address+=n;bytes+=n;count-=n;
    }
    return 0;
}
