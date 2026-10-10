#define NPE_MAP_DEFAULT_SKILL
#include "sdl_map_skill_record_probe.c"
#ifdef _WIN32
#include <windows.h>
#endif

static void check_current_default_records(const char *directory){
#ifdef _WIN32
    /* This console probe uses CRT main, not SDL2main's UTF-8 argv. */
    wchar_t wide[1024];char utf8[4096];
    assert(MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,directory,-1,wide,1024)>0);
    assert(WideCharToMultiByte(CP_UTF8,0,wide,-1,utf8,sizeof(utf8),NULL,NULL)>0);
    directory=utf8;
#endif
    const char *names[]={"异域传说1.npe","异域传说2.npe","异域传说3.npe",
                         "新仙剑后传.npe","英雄传奇.npe"};
    for(unsigned i=0;i<sizeof(names)/sizeof(*names);++i){
        char path[1024],error[128];uint8_t *bytes;size_t size;NpeFile file;
        assert(snprintf(path,sizeof(path),"%s/%s",directory,names[i])>0);
        assert(!read_file(path,&bytes,&size)&&!npe_parse(&file,bytes,size,error,sizeof(error)));
        NpeWorld w;assert(!npe_world_new_game(&file,&w));
        assert(!npe_world_resources_open(&file,&w));
        w.game_mode=6;w.party_active[0]=1;w.party_active[1]=w.party_active[2]=0;
        w.level[0]=99;w.hp[0]=50;w.qi[0]=100;
        memset(w.skill_slots[0],0,sizeof(w.skill_slots[0]));w.skill_slots[0][0]=1;
        NpeSkill skill;assert(!npe_world_skill(&file,&w,1,&skill));
        assert(skill.raw[14]==0&&skill.raw[37]==0&&skill.raw[38]==1&&skill.qi_cost==80);
        NpeWorld before=w;
        assert(npe_world_menu_cast_skill(&file,&w,0,0,1)==1);
        assert(w.qi[0]==20&&!memcmp(w.qi+1,before.qi+1,2*sizeof(*w.qi)));
        assert(!memcmp(w.hp,before.hp,sizeof(w.hp))&&
               !memcmp(w.max_hp,before.max_hp,sizeof(w.max_hp))&&
               !memcmp(w.max_qi,before.max_qi,sizeof(w.max_qi))&&
               !memcmp(w.attack,before.attack,sizeof(w.attack))&&
               !memcmp(w.defense,before.defense,sizeof(w.defense))&&
               !memcmp(w.other_stats,before.other_stats,sizeof(w.other_stats))&&
               !memcmp(w.status_guard,before.status_guard,sizeof(w.status_guard))&&
               !memcmp(w.condition_words,before.condition_words,sizeof(w.condition_words)));
        w.qi[0]=79;
        assert(npe_world_menu_cast_skill(&file,&w,0,0,1)==-3&&w.qi[0]==79);
        printf("Current NPE map default type: %s, cast100->20 and fee79 refused\n",names[i]);
        npe_free(&file);
    }
}
