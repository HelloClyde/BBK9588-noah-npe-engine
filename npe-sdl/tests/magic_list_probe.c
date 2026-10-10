#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_icon_grid_native.h"
#include "empty_item_native.h"

static void battle_icon_grid(const NpeFile *file,const NpeFont *font) {
    static const uint8_t body[]={0xcc,0xe5},qi[]={0xc6,0xf8};
    const char *labels[]={"entry","attack-right","attack-up","party","party-down","party-right",
        "flag","flag-left","flag-down","magic","magic-up","magic-left","attack","magic2",
        "flag2","party2","attack2","magic3","magic-back","party3","party-back","magic4",
        "flag3","flag-back","attack-back"};
    const int directions[]={-2,1,0,2,2,1,3,3,2,0,0,3,1,3,2,1,0,3,-1,2,-1,3,2,-1,-1};
    for(unsigned variant=0;variant<4;++variant) {
        NpeWorld world;unsigned icon=1;
        assert(npe_world_init(file,&world,9,29,13)==0);
        world.game_mode=3;world.story_event_id=1;
        assert(npe_world_prepare_scripted_battle(file,&world)==1);
        world.battle_enemy_ids[0]=51;world.battle_enemy_hps[0]=500;
        world.hp[0]=world.max_hp[0]=130;world.qi[0]=world.max_qi[0]=60;
        if(variant) {world.party_active[1]=1;world.party_ids[1]=1;world.party_characters[1]=world.party_characters[0];memcpy(world.party_names[1],world.party_names[0],12);memcpy(world.battle_names[1],world.party_names[0],12);world.battle_characters[1]=world.party_characters[0];world.hp[1]=world.max_hp[1]=130;}
        if(variant==2) world.hp[1]=0;
        if(variant==3) world.battle_party_status[1][1]=-3;
        for(unsigned page=0;page<25;++page) {
            if(directions[page]>=0) icon=(unsigned)npe_world_battle_move_icon(&world,icon,(unsigned)directions[page]);
            else if(directions[page]==-1) icon=1; /* first actor's base Menu */
            assert(icon==native_battle_grid_icons[variant][page]);
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            assert(npe_render_battle_world_scene(file,&world,pixels)==0);
            assert(npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels)==0);
            assert(npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels)==0);
            npe_render_battle_hud_values(130,130,60,60,pixels);
            assert(npe_render_battle_icon_selection(icon,pixels)==0);
            char path[120],header[40];unsigned differences=0;
            snprintf(path,sizeof(path),"tmp/native-battle-grid-%s-%s.pgm",
                variant==0?"1":variant==1?"2":variant==2?"2-fallen":"2-asleep",labels[page]);
            FILE *reference=fopen(path,"rb");
            assert(reference && fgets(header,sizeof(header),reference) &&
                fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
            for(unsigned i=0;i<sizeof(pixels);++i) {
                int value=fgetc(reference);assert(value>=0);
                differences+=value!=255-pixels[i]*17;
            }
            assert(fgetc(reference)==EOF && fclose(reference)==0);
            fprintf(stderr,"Native whole icon grid variant%u %s: %u differences\n",variant,labels[page],differences);
            if(differences) {
                FILE *dump=fopen("tmp/icon-grid-c.pgm","wb");assert(dump);
                fputs("P5\n240 120\n255\n",dump);
                for(unsigned i=0;i<sizeof(pixels);++i) fputc(255-pixels[i]*17,dump);
                fclose(dump);
            }
            assert(!differences);
        }
    }
}

static void battle_qi_prompt(const NpeFile *file, const NpeFont *font) {
  for(unsigned variant=0;variant<2;++variant) {
    NpeWorld world;
    unsigned ids[18] = {variant?4:5};
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    char header[40];
    unsigned differences = 0;
    FILE *reference = fopen(variant?"tmp/native-enemy-qi-single-target.pgm":"tmp/native-enemy-qi-target.pgm", "rb");
    assert(reference && fgets(header,sizeof(header),reference) &&
        fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
    assert(npe_world_new_game(file, &world) == 0);
    world.qi[0] = 0;
    assert(npe_render_native_magic_list_screen(file,font,&world,0,ids,1,0,0,pixels) == 0);
    assert(npe_render_battle_insufficient_qi(font,pixels) == 0);
    for(unsigned i=0;i<sizeof(pixels);++i) {
        int value=fgetc(reference); assert(value>=0);
        differences += value!=255-pixels[i]*17;
    }
    assert(fgetc(reference)==EOF && fclose(reference)==0);
    fprintf(stderr,"Native whole battle insufficient QI spell%u: %u differences\n",ids[0],differences);
    assert(!differences);
  }
}

static void battle_enemy_targets(const NpeFile *file, const NpeFont *font) {
    static const uint8_t body[] = {0xcc,0xe5}, qi[] = {0xc6,0xf8};
    const char *labels[] = {"target","down","up","down2","down3","wrap","up2","right","left2","back"};
    const int directions[] = {0,1,-1,1,1,1,-1,0,0,0};
    for(unsigned kind=0;kind<3;++kind) {
        for(unsigned count=2;count<=3;++count) {
            NpeWorld world;
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            assert(npe_world_init(file,&world,9,29,13)==0);
            world.game_mode=3;world.story_event_id=1;
            assert(npe_world_prepare_scripted_battle(file,&world)==1);
            world.battle_enemy_ids[0]=51;world.battle_enemy_hps[0]=500;
            if(count==3){world.battle_enemy_ids[2]=51;world.battle_enemy_hps[2]=500;}
            world.hp[0]=world.max_hp[0]=130;
            unsigned current_qi=kind==1?60:999;
            world.qi[0]=world.max_qi[0]=current_qi;
            world.battle_target=0;
            assert(npe_render_battle_enemy_target(file,&world,pixels)==0);
            assert(npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels)==0);
            assert(npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels)==0);
            npe_render_battle_hud_values(130,130,current_qi,current_qi,pixels);
            assert(npe_render_battle_icon_selection(kind,pixels)==0);
            for(unsigned page=0;page<10;++page) {
                char path[100],header[40];unsigned differences=0;
                if(directions[page]) {
                    unsigned previous=world.battle_target;
                    assert(npe_world_battle_cycle_target(&world,directions[page])>=0);
                    assert(npe_update_battle_enemy_target(file,&world,previous,pixels)==0);
                }
                snprintf(path,sizeof(path),"tmp/native-enemy-%s%s-%s.pgm",count==2?"two":"three",
                    kind==1?"-attack":kind==2?"-throw":"",labels[page]);
                FILE *reference=fopen(path,"rb");
                assert(reference && fgets(header,sizeof(header),reference) &&
                    fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
                for(unsigned i=0;i<sizeof(pixels);++i) {
                    int value=fgetc(reference);assert(value>=0);
                    differences+=value!=255-pixels[i]*17;
                }
                assert(fgetc(reference)==EOF && fclose(reference)==0);
                fprintf(stderr,"Native whole kind%u %u enemies %s: %u differences\n",kind,count,labels[page],differences);
                if(differences) {
                    FILE *dump=fopen("tmp/enemy-target-c.pgm","wb");assert(dump);
                    fputs("P5\n240 120\n255\n",dump);
                    for(unsigned i=0;i<sizeof(pixels);++i)fputc(255-pixels[i]*17,dump);
                    fclose(dump);
                }
                assert(!differences);
            }
        }
    }
}

static void battle_throw_lists(const NpeFile *file, const NpeFont *font) {
    const unsigned ids[] = {159,160}, quantities[] = {1,1};
    const char *labels[] = {"list","second","menu-back"};
    for(unsigned page=0;page<3;++page) {
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        char path[120],header[40];unsigned differences=0;
        assert(npe_render_item_list_screen(file,font,ids,quantities,2,page?1:0,pixels)==0);
        snprintf(path,sizeof(path),"tmp/native-enemy-two-throw-second-%s.pgm",labels[page]);
        FILE *reference=fopen(path,"rb");
        assert(reference && fgets(header,sizeof(header),reference) &&
            fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
        for(unsigned i=0;i<sizeof(pixels);++i) {
            int value=fgetc(reference);assert(value>=0);
            differences+=value!=255-pixels[i]*17;
        }
        assert(fgetc(reference)==EOF && fclose(reference)==0);
        fprintf(stderr,"Native whole throw list %s: %u differences\n",labels[page],differences);
        assert(!differences);
    }
}

static void battle_throw_returns(const NpeFile *file,const NpeFont *font) {
    static const uint8_t body[]={0xcc,0xe5},qi[]={0xc6,0xf8};
    const char *labels[]={"list-back","item-back","commands-back"};
    NpeWorld world;
    assert(npe_world_init(file,&world,9,29,13)==0);
    world.game_mode=3;world.story_event_id=1;
    assert(npe_world_prepare_scripted_battle(file,&world)==1);
    world.battle_enemy_ids[0]=51;world.battle_enemy_hps[0]=500;
    world.hp[0]=world.max_hp[0]=130;world.qi[0]=world.max_qi[0]=999;
    for(unsigned page=0;page<3;++page) {
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        char path[120],header[40];unsigned differences=0;
        assert(npe_render_battle_world_scene(file,&world,pixels)==0);
        assert(npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels)==0);
        assert(npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels)==0);
        npe_render_battle_hud_values(130,130,999,999,pixels);
        assert(npe_render_battle_icon_selection(2,pixels)==0);
        if(page<2) assert(npe_render_battle_command_menu(file,1,1,pixels)==0);
        if(page==0) assert(npe_render_battle_command_menu(file,2,1,pixels)==0);
        snprintf(path,sizeof(path),"tmp/native-enemy-two-throw-second-%s.pgm",labels[page]);
        FILE *reference=fopen(path,"rb");
        assert(reference && fgets(header,sizeof(header),reference) &&
            fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
        for(unsigned i=0;i<sizeof(pixels);++i) {
            int value=fgetc(reference);assert(value>=0);
            differences+=value!=255-pixels[i]*17;
        }
        assert(fgetc(reference)==EOF && fclose(reference)==0);
        fprintf(stderr,"Native whole throw return %s: %u differences\n",labels[page],differences);
        if(differences) {
            FILE *dump=fopen("tmp/throw-return-c.pgm","wb");assert(dump);
            fputs("P5\n240 120\n255\n",dump);
            for(unsigned i=0;i<sizeof(pixels);++i) fputc(255-pixels[i]*17,dump);
            fclose(dump);
        }
        assert(!differences);
    }
}

static void battle_targets(const NpeFile *file, const NpeFont *font) {
    static const uint8_t body[] = {0xcc,0xe5}, qi[] = {0xc6,0xf8};
    const char *labels[] = {"entry","target","down","up","up2"};
    for (unsigned count = 2; count <= 3; ++count) {
      for (unsigned variant = 0; variant < (count == 3 ? 3u : 1u); ++variant) {
        NpeWorld world;
        assert(npe_world_init(file, &world, 9, 29, 13) == 0);
        world.game_mode = 3; world.story_event_id = 1;
        assert(npe_world_prepare_scripted_battle(file, &world) == 1);
        for (unsigned i = 0; i < count; ++i) {
            world.party_active[i] = 1;
            world.hp[i] = world.max_hp[i] = 130;
            world.qi[i] = world.max_qi[i] = 60;
        }
        if (variant) world.hp[1] = variant == 1 ? 20 : 0;
        for (unsigned page = 0; page < 5; ++page) {
            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
            unsigned target = page == 2 ? count - 1 : page == 4 ? 1 : 0, differences = 0;
            char path[100], header[40];
            FILE *reference;
            world.qi[0] = world.max_qi[0] = page ? 999 : 60;
            assert((page ? npe_render_battle_party_target(file, &world, target, pixels) :
                           npe_render_battle_world_scene(file, &world, pixels)) == 0);
            assert(npe_draw_gbk_text(font, body, sizeof(body), 70,88,pixels) == 0);
            assert(npe_draw_gbk_text(font, qi, sizeof(qi), 70,104,pixels) == 0);
            npe_render_battle_hud_values(world.hp[target],world.max_hp[target],
                world.qi[target],world.max_qi[target],pixels);
            assert(npe_render_battle_icon_selection(page ? 0 : 1,pixels) == 0);
            snprintf(path,sizeof(path),"tmp/native-party-%u%s-%s.pgm",count,
                variant == 1 ? "-low" : variant == 2 ? "-fallen" : "",labels[page]);
            reference=fopen(path,"rb");
            assert(reference && fgets(header,sizeof(header),reference) &&
                fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
            for (unsigned i=0;i<sizeof(pixels);++i) {
                int value=fgetc(reference); assert(value>=0);
                differences += value != 255-pixels[i]*17;
            }
            assert(fgetc(reference)==EOF && fclose(reference)==0);
            fprintf(stderr,"Native whole party%u variant%u %s: %u differences\n",count,variant,labels[page],differences);
            if (differences) {
                FILE *dump=fopen("tmp/whole-party-c.pgm","wb");assert(dump);
                fputs("P5\n240 120\n255\n",dump);
                for(unsigned i=0;i<sizeof(pixels);++i)fputc(255-pixels[i]*17,dump);
                fclose(dump);
            }
            assert(!differences);
        }
      }
    }
}

static void battle_use_item_pages(const NpeFile *file, const NpeFont *font) {
    static const uint8_t body[] = {0xcc,0xe5}, qi[] = {0xc6,0xf8};
    const char *labels[] = {"target","down","up","down2","down3","right","left","escape","target-again"};
    const unsigned targets[2][9] = {{0,1,0,1,0,0,0,0,0},{0,2,0,2,1,1,1,1,0}};
    for (unsigned variant = 0; variant < 3; ++variant) {
        NpeWorld world;
        assert(npe_world_init(file, &world, 9, 29, 13) == 0);
        world.game_mode = 3; world.story_event_id = 1;
        assert(npe_world_prepare_scripted_battle(file, &world) == 1);
        unsigned party = variant == 1 ? 3 : 2;
        for (unsigned i = 0; i < party; ++i) {
            world.party_active[i] = 1; world.party_ids[i] = 1;world.party_characters[i]=world.party_characters[0];memcpy(world.party_names[i],world.party_names[0],12);memcpy(world.battle_names[i],world.party_names[0],12);world.battle_characters[i]=world.party_characters[0];
            world.hp[i] = 200 + 10*i; world.max_hp[i] = world.max_qi[i] = 999;
            world.qi[i] = 20 + 10*i;
        }
        if (variant == 1) world.hp[2] = 0;
        if (variant == 2) world.battle_actor = 1;
        for (unsigned page = 0; page < (variant == 2 ? 1u : 9u); ++page) {
            unsigned target = variant == 2 ? 1 : targets[variant][page];
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            assert(npe_render_battle_party_target(file,&world,target,pixels) == 0);
            assert(npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels) == 0);
            assert(npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels) == 0);
            npe_render_battle_hud_values(world.hp[target],world.max_hp[target],
                                        world.qi[target],world.max_qi[target],pixels);
            assert(npe_render_battle_icon_selection(2,pixels) == 0);
            char path[160],header[40]; unsigned differences = 0;
            snprintf(path,sizeof(path),"tmp/native-use-%u-46-%s-%s.pgm",party,
                     variant == 0 ? "cancel" : variant == 1 ? "fallen" : "settle",
                     variant == 2 ? "target-next" : labels[page]);
            FILE *reference = fopen(path,"rb");
            assert(reference && fgets(header,sizeof(header),reference) &&
                   fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
            for (unsigned i = 0; i < sizeof(pixels); ++i) {
                int value = fgetc(reference); assert(value >= 0);
                differences += value != 255-pixels[i]*17;
            }
            assert(fgetc(reference) == EOF && fclose(reference) == 0);
            fprintf(stderr,"Native whole use item %s: %u differences\n",path,differences);
            if (differences) {
                FILE *dump = fopen("tmp/use-item-c.pgm","wb"); assert(dump);
                fputs("P5\n240 120\n255\n",dump);
                for(unsigned i=0;i<sizeof(pixels);++i) fputc(255-pixels[i]*17,dump);
                fclose(dump);
            }
            assert(!differences);
        }
    }
    unsigned ids[] = {27,46,159}, quantities[] = {1,1,1};
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]; char header[40]; unsigned differences=0;
    assert(npe_render_item_list_screen(file,font,ids,quantities,3,0,pixels)==0);
    assert(npe_render_battle_item_rejection(font,0,pixels)==0);
    FILE *reference=fopen("tmp/native-use-2-46-mixed-target.pgm","rb");
    assert(reference && fgets(header,sizeof(header),reference) &&
           fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
    for(unsigned i=0;i<sizeof(pixels);++i) {
        int value=fgetc(reference); assert(value>=0); differences+=value!=255-pixels[i]*17;
    }
    assert(fgetc(reference)==EOF && fclose(reference)==0);
    fprintf(stderr,"Native whole item use rejection: %u differences\n",differences);
    assert(!differences);
    for (unsigned throwing=0;throwing<2;++throwing) {
        unsigned id=throwing?46:65, quantity=1;
        assert(npe_render_item_list_screen(file,font,&id,&quantity,1,0,pixels)==0);
        assert(npe_render_battle_item_rejection(font,throwing,pixels)==0);
        reference=fopen(throwing?"tmp/native-use-2-46-throw-target.pgm":
                        "tmp/native-use-2-65-revive-all-alive-target.pgm","rb");
        differences=0;
        assert(reference && fgets(header,sizeof(header),reference) &&
               fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
        for(unsigned i=0;i<sizeof(pixels);++i) {
            int value=fgetc(reference); assert(value>=0); differences+=value!=255-pixels[i]*17;
        }
        assert(fgetc(reference)==EOF && fclose(reference)==0);
        fprintf(stderr,"Native whole item rejection variant%u: %u differences\n",throwing,differences);
        assert(!differences);
    }
}

static void battle_empty_item_pages(const NpeFile *file,const NpeFont *font) {
    const char *paths[]={"tmp/native-empty-cancel-2-list.pgm",
                         "tmp/native-empty-cancel-2-cancel.pgm",
                         "tmp/native-empty-use-2-empty-confirm.pgm",
                         "tmp/native-empty-cancel-2-empty-confirm.pgm"};
    for(unsigned page=0;page<4;++page) {
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];char header[40];unsigned differences=0;
        if(page<3) {
            if(page==1) assert(npe_render_battle_empty_item_return(file,font,pixels)==0);
            else assert(npe_render_empty_inventory_screen(font,pixels)==0);
            if(page==2) assert(npe_render_battle_item_rejection(font,0,pixels)==0);
        }else {
            static const uint8_t body[]={0xcc,0xe5},qi[]={0xc6,0xf8};
            NpeWorld world;
            assert(npe_world_init(file,&world,9,29,13)==0);
            world.game_mode=3;world.story_event_id=1;
            assert(npe_world_prepare_scripted_battle(file,&world)==1);
            world.battle_enemy_ids[0]=51;world.battle_enemy_hps[0]=500;
            for(unsigned i=0;i<2;++i) {
                world.party_active[i]=1;world.party_ids[i]=1;world.party_characters[i]=world.party_characters[0];memcpy(world.party_names[i],world.party_names[0],12);memcpy(world.battle_names[i],world.party_names[0],12);world.battle_characters[i]=world.party_characters[0];world.hp[i]=world.max_hp[i]=10000;
                world.qi[i]=(uint16_t)native_empty_target_qi[0];world.max_qi[i]=(uint16_t)native_empty_target_qi[1];
            }
            world.battle_target=0;
            assert(npe_render_battle_enemy_target(file,&world,pixels)==0);
            assert(npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels)==0);
            assert(npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels)==0);
            npe_render_battle_hud_values(10000,10000,world.qi[0],world.max_qi[0],pixels);
            assert(npe_render_battle_icon_selection(2,pixels)==0);
        }
        FILE *reference=fopen(paths[page],"rb");
        assert(reference && fgets(header,sizeof(header),reference) &&
               fgets(header,sizeof(header),reference) && fgets(header,sizeof(header),reference));
        for(unsigned i=0;i<sizeof(pixels);++i) {
            int value=fgetc(reference);assert(value>=0);differences+=value!=255-pixels[i]*17;
        }
        assert(fgetc(reference)==EOF && fclose(reference)==0);
        fprintf(stderr,"Native whole empty item %s: %u differences\n",paths[page],differences);
        if(differences) {
            FILE *dump=fopen("tmp/empty-target-c.pgm","wb");assert(dump);
            fputs("P5\n240 120\n255\n",dump);
            for(unsigned i=0;i<sizeof(pixels);++i) fputc(255-pixels[i]*17,dump);
            fclose(dump);
        }
        assert(!differences);
    }
}

int main(int argc, char **argv) {
    NpeFile file;
    NpeFont font = {0};
    NpeWorld world;
    unsigned ids[18], count;
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    FILE *input;
    long size;
    uint8_t *bytes;
    char error[128];
    const unsigned selections[] = {0, 3, 4, 7, 7};
    const char *labels[] = {"list", "down", "right", "down2", "down3"};
    assert(argc == 3);
    input = fopen(argv[1], "rb");
    assert(input && fseek(input, 0, SEEK_END) == 0);
    size = ftell(input);
    assert(size > 0 && fseek(input, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)size);
    assert(bytes && fread(bytes, 1, (size_t)size, input) == (size_t)size);
    fclose(input);
    assert(npe_parse(&file, bytes, (size_t)size, error, sizeof(error)) == 0);
    assert(npe_font_load(&font, argv[2]) == 0);
    assert(npe_font_load16(&font, "npe-sdl/assets/gbk16-native.bin") == 0);
    battle_icon_grid(&file, &font);
    battle_qi_prompt(&file, &font);
    battle_enemy_targets(&file, &font);
    battle_throw_lists(&file, &font);
    battle_throw_returns(&file, &font);
    battle_targets(&file, &font);
    battle_use_item_pages(&file, &font);
    battle_empty_item_pages(&file, &font);
    assert(npe_world_new_game(&file, &world) == 0);
    world.level[0] = 99;
    world.qi[0] = 999;
    count = npe_world_available_skills(&file, &world, 0, ids);
    assert(count == 9 && ids[0] == 4 && ids[7] == 16);
    for (unsigned scenario = 0; scenario < 2; ++scenario) {
    const unsigned selections18[] = {0, 3, 6, 9, 10, 7};
    const unsigned first18[] = {0, 0, 0, 3, 3, 3};
    const char *labels18[] = {"list", "down", "down2", "scroll", "right", "up"};
    if (scenario) {
        NpeCharacter character;
        assert(npe_character(&file, 1, &character) == 0);
        memcpy(world.skill_slots[0], character.raw + 58, 18);
        count = npe_world_available_skills(&file, &world, 0, ids);
        assert(count == 18 && ids[0] == 2 && ids[17] == 52);
    }
    for (unsigned sample = 0; sample < (scenario ? 6 : 5); ++sample) {
        char path[100], header[32];
        unsigned differences = 0;
        unsigned selection = scenario ? selections18[sample] : selections[sample];
        unsigned first = scenario ? first18[sample] : 0;
        const char *label = scenario ? labels18[sample] : labels[sample];
        if (!sample) assert(npe_render_native_magic_list_screen(&file, &font, &world, 0,
               ids, count, first, selection, pixels) == 0);
        else
            assert(npe_update_native_magic_list_screen(&file, &font, &world, 0,
               ids, count, first, selection, scenario ? first18[sample - 1] : 0,
               scenario ? selections18[sample - 1] : selections[sample - 1], pixels) == 0);
        snprintf(path, sizeof(path), "tmp/native-spell-%s%s.pgm", scenario ? "list18-" : "", label);
        input = fopen(path, "rb");
        assert(input && fgets(header, sizeof(header), input) &&
               fgets(header, sizeof(header), input) && fgets(header, sizeof(header), input));
        for (unsigned i = 0; i < sizeof(pixels); ++i) {
            int reference = fgetc(input);
            assert(reference >= 0);
            differences += reference != 255 - pixels[i] * 17;
        }
        assert(fgetc(input) == EOF && fclose(input) == 0);
        fprintf(stderr, "native magic list %u/%s: %u pixel differences\n", count, label, differences);
        if (differences) {
            snprintf(path, sizeof(path), "tmp/c-spell-%u-%s.pgm", count, label);
            FILE *output = fopen(path, "wb");
            assert(output && fprintf(output, "P5\n240 120\n255\n") > 0);
            for (unsigned i = 0; i < sizeof(pixels); ++i)
                assert(fputc(255 - pixels[i] * 17, output) != EOF);
            assert(fclose(output) == 0);
        }
        assert(differences == 0);
    }
    }
    npe_font_free(&font);
    npe_free(&file);
    return 0;
}
