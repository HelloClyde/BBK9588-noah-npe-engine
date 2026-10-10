#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "map_spells_native.h"
#include "map_status_native.h"
#include "map_equipment_native.h"
#include "equipment_attributes_native.h"

static void equipment_attributes_check(NpeFile *file) {
    NpeItem item;assert(npe_item(file,84,&item)==0);
    uint8_t *raw=file->bytes+(item.raw-file->bytes),backup[40];memcpy(backup,raw,sizeof(backup));
    uint64_t fingerprint=npe_game_fingerprint(file);
    for(unsigned i=0;i<sizeof(equipment_attributes_native)/sizeof(equipment_attributes_native[0]);++i){
        for(unsigned s=0;s<7;++s){uint16_t value=(uint16_t)equipment_attributes_native[i].deltas[s];raw[13+2*s]=(uint8_t)(value>>8);raw[14+2*s]=(uint8_t)value;}
        NpeWorld world;assert(npe_world_new_game(file,&world)==0);
        const uint16_t *initial=equipment_attributes_native[i].initial;
        world.hp[0]=initial[0];world.max_hp[0]=initial[1];world.qi[0]=initial[2];world.max_qi[0]=initial[3];
        world.attack[0]=initial[4];world.defense[0]=initial[5];memcpy(world.other_stats[0],initial+6,sizeof(world.other_stats[0]));
        memset(world.condition_words,0,sizeof(world.condition_words));world.condition_words[85]=world.condition_words[91]=1;
        for(unsigned phase=0;phase<2;++phase){
            if(i==2&&phase==1){for(unsigned s=0;s<4;++s)world.status_guard[0][s]=(uint16_t)(21+s);world.condition_words[85]=65535;}
            assert(npe_world_equip_item(file,&world,phase?91:85,0)==1);
            uint16_t actual[]={world.hp[0],world.max_hp[0],world.qi[0],world.max_qi[0],world.attack[0],world.defense[0],world.other_stats[0][0],world.other_stats[0][1],world.other_stats[0][2]};
            assert(memcmp(actual,equipment_attributes_native[i].after[phase],sizeof(actual))==0);
            assert(memcmp(world.status_guard[0],equipment_attributes_native[i].status[phase],sizeof(world.status_guard[0]))==0);
            assert(world.condition_words[85]==equipment_attributes_native[i].inventory[phase][0]&&world.condition_words[91]==equipment_attributes_native[i].inventory[phase][1]);
            for(unsigned slot=0;slot<6;++slot)assert(world.equipped_items[0][slot]==(slot==5?(phase?91:85):0));
            /* Preserve low words and separate maxima through the real save codec. */
            NpeWorld restored;const char *path="tmp/equipment-attributes-save.npes";
            assert(npe_world_save(file,&world,path)==0&&npe_world_load(file,&restored,path)==0);
            assert(restored.hp[0]==world.hp[0]&&restored.max_hp[0]==world.max_hp[0]&&restored.qi[0]==world.qi[0]&&restored.max_qi[0]==world.max_qi[0]);
            assert(restored.attack[0]==world.attack[0]&&restored.defense[0]==world.defense[0]&&memcmp(restored.other_stats,world.other_stats,sizeof(world.other_stats))==0);
            assert(memcmp(restored.status_guard,world.status_guard,sizeof(world.status_guard))==0&&memcmp(restored.condition_words,world.condition_words,sizeof(world.condition_words))==0);
            assert(remove(path)==0);
            fprintf(stderr,"Original equipment attributes %s phase %u and save passed\n",equipment_attributes_native[i].name,phase);
        }
    }
    memcpy(raw,backup,sizeof(backup));assert(npe_game_fingerprint(file)==fingerprint);
}

static void map_equipment_check(const NpeFile *file) {
    NpeWorld world;assert(npe_world_new_game(file,&world)==0);
    for(unsigned s=0;s<4;++s)world.status_guard[0][s]=(uint16_t)(s+1);
    const unsigned items[]={26,85,91};
    memset(world.condition_words,0,sizeof(world.condition_words));
    for(unsigned i=0;i<3;++i)world.condition_words[items[i]]=1;
    for(unsigned i=0;i<sizeof(map_equipment_native)/sizeof(map_equipment_native[0]);++i){
        assert(npe_world_equip_item(file,&world,map_equipment_native[i].id,0)==1);
        assert(world.hp[0]==map_equipment_native[i].hp&&world.qi[0]==map_equipment_native[i].qi);
        assert(world.attack[0]==map_equipment_native[i].attack&&world.defense[0]==map_equipment_native[i].defense);
        assert(memcmp(world.status_guard[0],map_equipment_native[i].status,sizeof(world.status_guard[0]))==0);
        assert(memcmp(world.equipped_items[0],map_equipment_native[i].equipment,sizeof(world.equipped_items[0]))==0);
        for(unsigned j=0;j<3;++j)assert(world.condition_words[items[j]]==map_equipment_native[i].inventory[j]);
        fprintf(stderr,"Original equipment %u sequence state passed\n",map_equipment_native[i].id);
    }
}

static uint64_t checksum(const uint8_t *bytes, size_t size) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<size;++i)hash=(hash^bytes[i])*UINT64_C(1099511628211);
    return hash;
}

static void map_status_save(const NpeFile *file) {
    const char *path="tmp/map-status-save-probe.npes";
    NpeWorld original,loaded;
    assert(npe_world_new_game(file,&original)==0);
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)
        original.status_guard[i][j]=(uint16_t)(65530+i*10+j);
    original.condition_words[212]=28;original.condition_words[213]=(uint16_t)-20;
    assert(npe_world_save(file,&original,path)==0);
    assert(npe_world_load(file,&loaded,path)==0);
    assert(memcmp(original.status_guard,loaded.status_guard,sizeof(original.status_guard))==0);
    assert(loaded.condition_words[212]==28&&loaded.condition_words[213]==(uint16_t)-20);
    uint8_t bytes[4096];FILE *stream=fopen(path,"rb");assert(stream);
    size_t size=fread(bytes,1,sizeof(bytes),stream);assert(fclose(stream)==0);
    assert(size>50&&memcmp(bytes,"NPESAV22",8)==0);
    /* A genuine V14 layout has no fourth-state tail. */
    size-=144+NPE_CHARACTER_RECORD_SAVE_BYTES;memcpy(bytes,"NPESAV14",8);
    for(unsigned i=0;i<4;++i)bytes[24+i]=(uint8_t)((size-36)>>(8*i));
    uint64_t hash=checksum(bytes+36,size-36);
    for(unsigned i=0;i<8;++i)bytes[28+i]=(uint8_t)(hash>>(8*i));
    stream=fopen(path,"wb");assert(stream&&fwrite(bytes,1,size,stream)==size&&fclose(stream)==0);
    assert(npe_world_load(file,&loaded,path)==0);
    for(unsigned i=0;i<3;++i){
        for(unsigned j=0;j<3;++j)assert(loaded.status_guard[i][j]==original.status_guard[i][j]);
        assert(loaded.status_guard[i][3]==0);
    }
    assert(loaded.condition_words[212]==28&&loaded.condition_words[213]==(uint16_t)-20);
    /* A V15 header on a V14 payload is malformed even with a valid checksum. */
    memcpy(bytes,"NPESAV15",8);
    stream=fopen(path,"wb");assert(stream&&fwrite(bytes,1,size,stream)==size&&fclose(stream)==0);
    NpeWorld unchanged=loaded;
    assert(npe_world_load(file,&loaded,path)==-1&&memcmp(&loaded,&unchanged,sizeof(loaded))==0);
    assert(remove(path)==0);
    fprintf(stderr,"Map fourth state V18 roundtrip, V14 migration and malformed-tail refusal passed\n");
}

static void map_status_check(NpeFile *file) {
    unsigned states=0;uint64_t fingerprint=npe_game_fingerprint(file);
    NpeSkill skill;assert(npe_skill(file,39,&skill)==0);
    uint8_t *raw=file->bytes+(skill.raw-file->bytes),backup[41];memcpy(backup,raw,sizeof(backup));
    for(unsigned i=0;i<sizeof(map_status_native)/sizeof(map_status_native[0]);++i){
        memcpy(raw,backup,sizeof(backup));
        const unsigned vi=map_status_native[i].variant;
        if(vi>=4){
            static const unsigned offsets[]={31,33,29,35};
            static const uint16_t effects[]={7,(uint16_t)-5,11,(uint16_t)-13};
            for(unsigned s=0;s<4;++s){raw[offsets[s]]=(uint8_t)(effects[s]>>8);raw[offsets[s]+1]=(uint8_t)effects[s];}
            if(vi>=5)raw[37]=1;
        }
        NpeWorld world;assert(npe_world_new_game(file,&world)==0);world.game_mode=6;
        for(unsigned actor=0;actor<map_status_native[i].count;++actor){
            world.party_active[actor]=1;world.party_ids[actor]=1;world.party_characters[actor]=world.party_characters[0];memcpy(world.party_names[actor],world.party_names[0],12);memcpy(world.battle_names[actor],world.party_names[0],12);world.battle_characters[actor]=world.party_characters[0];world.level[actor]=99;
            world.hp[actor]=actor==1?0:actor==2?190:50;world.max_hp[actor]=200;
            world.qi[actor]=100;world.max_qi[actor]=200;
            for(unsigned s=0;s<4;++s)world.status_guard[actor][s]=(uint16_t)(vi==1?65530:actor*10+s);
        }
        if(vi==1){world.hp[0]=32700;world.max_hp[0]=32767;}
        if(vi==6)world.party_active[1]=0;
        memset(world.skill_slots[0],0,18);world.skill_slots[0][0]=40;
        uint16_t attack[3],defense[3];memcpy(attack,world.attack,sizeof(attack));memcpy(defense,world.defense,sizeof(defense));
        for(unsigned n=0;n<map_status_native[i].casts;++n)
            assert(npe_world_menu_cast_skill(file,&world,0,vi==3?1:0,40)==1);
        for(unsigned actor=0;actor<map_status_native[i].count;++actor){
            assert(world.hp[actor]==map_status_native[i].hp[actor]&&world.qi[actor]==map_status_native[i].qi[actor]);
            assert(memcmp(world.status_guard[actor],map_status_native[i].states[actor],sizeof(world.status_guard[actor]))==0);
            ++states;
        }
        for(unsigned s=0;s<4;++s)assert(world.condition_words[212+s]==map_status_native[i].aliases[s]);
        assert(memcmp(attack,world.attack,sizeof(attack))==0&&memcmp(defense,world.defense,sizeof(defense))==0);
        fprintf(stderr,"Original map status variant %u cast %u passed\n",vi,map_status_native[i].casts);
    }
    memcpy(raw,backup,sizeof(backup));assert(npe_game_fingerprint(file)==fingerprint);
    fprintf(stderr,"%u original map status party states and inventory aliases passed\n",states);
    map_status_save(file);
}

static void pixels(const char *path, const uint8_t *frame, unsigned left, unsigned top, unsigned width, unsigned height) {
    FILE *f=fopen(path,"rb");assert(f);
    uint8_t ref[NPE_WIDTH*NPE_HEIGHT];assert(fread(ref,1,sizeof(ref),f)==sizeof(ref));assert(fgetc(f)==EOF);fclose(f);
    unsigned differences=0;
    for(unsigned y=top;y<top+height;++y)
        for(unsigned x=left;x<left+width;++x) differences+=ref[y*240+x]!=frame[y*240+x];
    fprintf(stderr,"Original map crop %s: %u differences\n",path,differences);
    if(differences){f=fopen("tmp/map-spell-c.idx","wb");assert(f);fwrite(frame,1,sizeof(ref),f);fclose(f);}
    assert(!differences);
}
int main(int argc,char **argv) {
    assert(argc==2);NpeFile file;char error[256];FILE *input=fopen(argv[1],"rb");assert(input);
    assert(fseek(input,0,SEEK_END)==0);long size=ftell(input);assert(size>0&&fseek(input,0,SEEK_SET)==0);
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,input)==(size_t)size);fclose(input);
    assert(npe_parse(&file,bytes,(size_t)size,error,sizeof(error))==0);
    map_status_check(&file);
    map_equipment_check(&file);
    equipment_attributes_check(&file);
    NpeFont font={0};assert(npe_font_load(&font,"npe-sdl/build/assets/gbk12-native.bin")==0);
    assert(npe_font_load16(&font,"npe-sdl/build/assets/gbk16-native.bin")==0);
    unsigned states=0;
    for(unsigned i=0;i<sizeof(map_native)/sizeof(map_native[0]);++i) {
        NpeWorld world;assert(npe_world_new_game(&file,&world)==0);world.game_mode=6;
        world.level[0]=99;world.hp[0]=50;world.max_hp[0]=200;world.qi[0]=100;world.max_qi[0]=200;
        memset(world.skill_slots[0],0,18);world.skill_slots[0][0]=(uint8_t)map_native[i].id;
        if(map_native[i].variant==1)world.hp[0]=0;
        if(map_native[i].variant==2){world.hp[0]=32700;world.max_hp[0]=32767;}
        if(map_native[i].variant==4)world.qi[0]=0;
        if(map_native[i].variant==3)for(unsigned j=1;j<3;++j){world.party_active[j]=1;world.party_ids[j]=1;world.party_characters[j]=world.party_characters[0];memcpy(world.party_names[j],world.party_names[0],12);memcpy(world.battle_names[j],world.party_names[0],12);world.battle_characters[j]=world.party_characters[0];world.hp[j]=j==1?0:190;world.max_hp[j]=200;world.qi[j]=100;world.max_qi[j]=200;}
        uint16_t old_attack[3],old_defense[3];memcpy(old_attack,world.attack,sizeof(old_attack));memcpy(old_defense,world.defense,sizeof(old_defense));
        for(unsigned n=0;n<map_native[i].casts;++n)assert(npe_world_menu_cast_skill(&file,&world,0,0,map_native[i].id)==map_native[i].result);
        for(unsigned j=0;j<(map_native[i].variant==3?3u:1u);++j){assert(world.hp[j]==map_native[i].hp[j]);assert(world.qi[j]==map_native[i].qi[j]);++states;}
        assert(memcmp(old_attack,world.attack,sizeof(old_attack))==0);assert(memcmp(old_defense,world.defense,sizeof(old_defense))==0);
        fprintf(stderr,"Original map spell %u variant%u passed\n",map_native[i].id,map_native[i].variant);
    }
    NpeWorld world;uint8_t frame[NPE_WIDTH*NPE_HEIGHT];assert(npe_world_new_game(&file,&world)==0);world.game_mode=6;
    for(unsigned i=1;i<3;++i){world.party_active[i]=1;world.party_ids[i]=1;world.party_characters[i]=world.party_characters[0];memcpy(world.party_names[i],world.party_names[0],12);memcpy(world.battle_names[i],world.party_names[0],12);world.battle_characters[i]=world.party_characters[0];}
    memset(frame,0,sizeof(frame));assert(npe_render_magic_member_overlay(&file,&font,&world,0,frame)==0);
    pixels("tmp/map-spell-14-multi-choose.idx",frame,42,37,72,67);
    for(unsigned i=1;i<3;++i){char path[100];memset(frame,0,sizeof(frame));assert(npe_render_magic_member_overlay(&file,&font,&world,i,frame)==0);snprintf(path,sizeof(path),"tmp/map-spell-14-multi-choose%u.idx",i);pixels(path,frame,42,37,72,67);}
    for(unsigned i=1;i<3;++i){char path[100];memset(frame,0,sizeof(frame));assert(npe_render_magic_member_overlay(&file,&font,&world,2-i,frame)==0);snprintf(path,sizeof(path),"tmp/map-spell-14-multi-choose-up%u.idx",i);pixels(path,frame,42,37,72,67);}
    world.party_active[2]=0;
    for(unsigned i=0;i<2;++i){const char *path=i?"tmp/map-spell-14-two-choose1.idx":"tmp/map-spell-14-two-choose.idx";memset(frame,0,sizeof(frame));assert(npe_render_magic_member_overlay(&file,&font,&world,i,frame)==0);pixels(path,frame,42,37,72,52);}
    memset(frame,0,sizeof(frame));assert(npe_render_magic_member_overlay(&file,&font,&world,0,frame)==0);pixels("tmp/map-spell-14-two-choose-up1.idx",frame,42,37,72,52);
    world.party_active[1]=world.party_active[2]=0;
    memset(frame,0,sizeof(frame));assert(npe_render_magic_member_overlay(&file,&font,&world,0,frame)==0);
    pixels("tmp/map-spell-6-insufficient-choose.idx",frame,42,37,72,37);
    world.level[0]=99;world.qi[0]=0;
    unsigned ids[18]={6};
    assert(npe_render_native_magic_list_screen(&file,&font,&world,0,ids,1,0,0,frame)==0);
    pixels("tmp/map-spell-6-insufficient-list.idx",frame,0,0,240,120);
    assert(npe_render_map_magic_message(&font,0,frame)==0);
    pixels("tmp/map-spell-6-insufficient-target.idx",frame,0,0,240,120);
    world.qi[0]=100;ids[0]=4;
    assert(npe_render_native_magic_list_screen(&file,&font,&world,0,ids,1,0,0,frame)==0);
    assert(npe_render_map_magic_message(&font,1,frame)==0);
    pixels("tmp/map-spell-4-target.idx",frame,0,0,240,120);
    fprintf(stderr,"%u native map party states passed\n",states);
    npe_font_free(&font);npe_free(&file);return 0;
}
