#ifdef NPE_NPC_BRANCH_MATRIX
#include "npc_condition_dialogue_native.h"
#define NPC_TEXT_FRAME_PREFIX "tmp/native-npc-branch-"
#elif defined(NPE_NPC_IGNORED_KEYS)
#include "npc_ignored_keys_native.h"
#define NPC_TEXT_FRAME_PREFIX "tmp/native-npc-ignored-"
#else
#include "npc_text_native.h"
#define NPC_TEXT_FRAME_PREFIX "tmp/native-npc-text-"
#endif
static void npc_frame(const char *path,uint8_t *pixels){
    FILE *in=fopen(path,"rb");assert(in);
    assert(fread(pixels,1,NPE_WIDTH*NPE_HEIGHT,in)==NPE_WIDTH*NPE_HEIGHT&&fgetc(in)==EOF&&!fclose(in));
}
static unsigned npc_state_differences(const NpeWorld *w,const NpcTextState *s,unsigned npc){
    unsigned differences=(w->game_mode!=s->mode)+(w->story_event_id!=s->event)+(w->story_progress!=s->progress)+
        (w->event_context_id!=s->context)+(w->mode2_context!=s->npc_context)+(w->npc_state[npc]!=s->npc_state)+
        (w->mode2_followup_mode!=s->followup)+(w->mode2_followup_event_id!=s->next)+
        (w->map_init_pending!=s->map_init)+(w->text_file_position!=s->pointer)+
        (w->text_offset!=s->offset)+(w->text_more!=s->more)+(w->text_style!=s->style)+
        (memcmp(w->text_rect,s->rect,8)!=0);
#ifdef NPE_NPC_BRANCH_MATRIX
    differences+=(w->money!=s->money)+(w->npc_state[32]!=s->npc32)+
        (w->npc_guard_index!=s->guard_index)+(w->npc_guard_word_index!=s->guard_word_index)+
        (w->npc_guard_a!=s->guard_a)+(w->npc_guard_b!=s->guard_b);
#endif
    return differences;
}
static void npc_patch(NpeFile *file,NpeWorld *w,const NpcTextState *s,unsigned npc){
    uint8_t bytes[8];
    for(unsigned i=0;i<4;++i){bytes[2*i]=(uint8_t)(s->rect[i]>>8);bytes[2*i+1]=(uint8_t)s->rect[i];}
    assert(!npe_rpg_runtime_write(file,0x52791e,bytes,8));
    for(unsigned i=0;i<4;++i)bytes[i]=(uint8_t)(s->pointer>>(24u-8u*i));
    assert(!npe_rpg_runtime_write(file,0x52797a,bytes,4));
    bytes[0]=(uint8_t)(s->offset>>8);bytes[1]=(uint8_t)s->offset;bytes[2]=(uint8_t)s->more;
    assert(!npe_rpg_runtime_write(file,0x527a1a,bytes,3));
    bytes[0]=(uint8_t)s->style;assert(!npe_rpg_runtime_write(file,0x527ace,bytes,1));
    bytes[0]=(uint8_t)s->npc_context;assert(!npe_rpg_runtime_write(file,0x527a68,bytes,1));
    w->game_mode=(uint8_t)s->mode;w->story_event_id=(uint16_t)s->event;w->story_progress=(uint16_t)s->progress;
    w->event_context_id=(uint16_t)s->context;w->npc_state[npc]=(uint8_t)s->npc_state;
    w->mode2_followup_mode=(uint8_t)s->followup;w->mode2_followup_event_id=(uint16_t)s->next;
    w->map_init_pending=(uint16_t)s->map_init;
#ifdef NPE_NPC_BRANCH_MATRIX
    w->money=s->money;w->npc_state[32]=(uint8_t)s->npc32;
    w->npc_guard_index=s->guard_index;w->npc_guard_word_index=s->guard_word_index;
    w->npc_guard_a=(uint8_t)s->guard_a;w->npc_guard_b=(uint8_t)s->guard_b;
#endif
    assert(!npc_state_differences(w,s,npc));
}
