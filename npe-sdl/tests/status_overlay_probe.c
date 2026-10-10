#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "status_overlay_native.h"

int main(int argc,char **argv) {
    assert(argc==2);FILE *input=fopen(argv[1],"rb");assert(input);
    assert(!fseek(input,0,SEEK_END));long size=ftell(input);assert(size>0&&!fseek(input,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,input)==(size_t)size&&!fclose(input));
    NpeFile file;NpeFont font={0};char error[128];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    for(unsigned sample=0;sample<sizeof(overlay_cases)/sizeof(*overlay_cases);++sample){
        const OverlayCase *c=&overlay_cases[sample];NpeWorld w;
        assert(!npe_world_init(&file,&w,9,29,13));w.game_mode=3;w.story_event_id=1;
        /* The native fixture copies map members before entering battle.
           Entry fixes the physical party prefix, portraits and coordinates. */
        memset(w.party_active,0,sizeof(w.party_active));
        for(unsigned i=0;i<c->party;++i){
            w.party_active[i]=w.party_ids[i]=1;
            w.party_characters[i]=w.party_characters[0];
            memcpy(w.party_names[i],w.party_names[0],12);
        }
        assert(npe_world_prepare_scripted_battle(&file,&w)==1);
        memset(w.battle_party_status,0,sizeof(w.battle_party_status));
        memset(w.battle_enemy_status,0,sizeof(w.battle_enemy_status));
        for(unsigned i=0;i<c->party;++i){
            w.party_active[i]=w.party_ids[i]=1;w.party_characters[i]=w.party_characters[0];memcpy(w.party_names[i],w.party_names[0],12);memcpy(w.battle_names[i],w.party_names[0],12);w.battle_characters[i]=w.party_characters[0];w.max_hp[i]=1000;w.max_qi[i]=999;
            w.hp[i]=(uint16_t)c->before[i].hp;w.qi[i]=(uint16_t)c->before[i].qi;
            w.other_stats[i][1]=(uint16_t)(999-i);
            memcpy(w.battle_party_status[i],c->before[i].status,8);
        }
        w.battle_enemy_count=(uint8_t)c->enemies;
        for(unsigned i=0;i<c->enemies;++i){
            const OverlayRecord *r=&c->before[c->party+i];
            w.battle_enemy_ids[i]=51;w.battle_enemy_max_hps[i]=10000;w.battle_enemy_max_qis[i]=300;
            /* Native formation loads enemy51 into every live physical slot;
               its image word is independent of the later lookup ID. */
            NpeEnemy enemy;assert(!npe_enemy(&file,50,&enemy));
            w.battle_record_residue[c->party+i][46]=(uint8_t)(enemy.graphic>>8);
            w.battle_record_residue[c->party+i][47]=(uint8_t)enemy.graphic;
            w.battle_enemy_hps[i]=(uint16_t)r->hp;w.battle_enemy_qis[i]=(uint16_t)r->qi;w.battle_enemy_speeds[i]=0;
            memcpy(w.battle_enemy_status[i],r->status,8);
        }
        w.battle_actor=0;w.battle_enemy_hp=w.battle_enemy_hps[0];
        for(unsigned i=0;i<c->party;++i){
            if((int16_t)w.hp[i]<=0||w.battle_party_status[i][1]<0||w.battle_party_status[i][2]<0)continue;
            assert(w.battle_actor==i);assert(npe_world_battle_defend(&w)==NPE_BATTLE_CONTINUE);
        }
        assert(!w.battle_pending_mask&&w.battle_actor==c->actor);
        for(unsigned i=0;i<c->party+c->enemies;++i){
            OverlayRecord r;unsigned enemy=i>=c->party?i-c->party:0;
            r.hp=i<c->party?w.hp[i]:w.battle_enemy_hps[enemy];r.qi=i<c->party?w.qi[i]:w.battle_enemy_qis[enemy];
            memcpy(r.status,i<c->party?w.battle_party_status[i]:w.battle_enemy_status[enemy],8);
            assert(r.hp==c->after[i].hp&&r.qi==c->after[i].qi&&!memcmp(r.status,c->after[i].status,8));
        }
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],reference[NPE_WIDTH*NPE_HEIGHT];
        assert(!npe_render_battle_world_scene(&file,&w,pixels));
        static const uint8_t body[]={0xcc,0xe5},qi[]={0xc6,0xf8};
        assert(!npe_draw_gbk_text(&font,body,2,70,88,pixels)&&!npe_draw_gbk_text(&font,qi,2,70,104,pixels));
        npe_render_battle_hud_values(w.hp[c->actor],w.max_hp[c->actor],w.qi[c->actor],w.max_qi[c->actor],pixels);
        assert(!npe_render_battle_icon_selection(c->icon,pixels));
        npe_render_battle_status_overlay(&w,pixels);char path[160];
        if(!strcmp(c->name,"empty-fallen-3-3"))snprintf(path,sizeof(path),"tmp/native-fallen-empty-defend-fallen-3-3.idx");
        else if(!strncmp(c->name,"third-",6))snprintf(path,sizeof(path),"tmp/native-third-status-overlay-%s.idx",c->name+6);
        else snprintf(path,sizeof(path),"tmp/native-status-overlay-%s.idx",c->name);input=fopen(path,"rb");assert(input);
        assert(fread(reference,1,sizeof(reference),input)==sizeof(reference)&&fgetc(input)==EOF&&!fclose(input));
        unsigned differences=0;for(unsigned i=0;i<sizeof(pixels);++i)differences+=pixels[i]!=reference[i];
        fprintf(stderr,"Original status overlay %s: %u differences\n",c->name,differences);
        if(differences){input=fopen("tmp/status-overlay-c.idx","wb");assert(input);assert(fwrite(pixels,1,sizeof(pixels),input)==sizeof(pixels)&&!fclose(input));}
        assert(!differences);
#ifdef OVERLAY_FRAME_CHECK
        OVERLAY_FRAME_CHECK(&file,&font,&w,c,reference);
#endif
    }
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<(size_t)size;++i){hash^=bytes[i];hash*=UINT64_C(1099511628211);}
    assert(hash==UINT64_C(0x114861a601117d94));npe_font_free(&font);npe_free(&file);return 0;
}
