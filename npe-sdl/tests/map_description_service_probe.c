#include "../npe.h"
#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "map_description_probe_helpers.h"
int main(int argc,char **argv){
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0&&!fseek(in,0,SEEK_SET));
    uint8_t *data=malloc((size_t)size);assert(data&&fread(data,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile file;char error[128];assert(!npe_parse(&file,data,(size_t)size,error,sizeof(error)));
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));NpeWorld world;unsigned total=0,ignored=0;
    for(unsigned c=0;c<sizeof(map_description_cases)/sizeof(*map_description_cases);++c){const MapDescriptionCase *current=map_description_cases+c;
        assert(!npe_world_new_game(&file,&world));map_description_seed(&world,&current->before);assert(!npe_world_resources_open(&file,&world));
        uint8_t pixels[28800],native[28800],bitmap[32],after_bitmap[32];map_description_frame(current->name,-1,pixels);
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32));
        for(unsigned phase=0;phase<current->count;++phase){const MapDescriptionPhase *p=current->phases+phase;map_description_patch(&world,p);
            int result=1;
            if(!phase){unsigned ids[18]={current->id},quantities[1]={2};
                if(current->skill){assert(npe_world_available_cached_skills(&file,&world,0,ids)==1);assert(!npe_render_map_magic_list(&file,&font,&world,0,ids,1,0,0,0,0,1,pixels));}
                else{world.game_mode=4;world.story_event_id=2;assert(!npe_render_map_item_list(&file,&font,&world,ids,quantities,1,0,0,1,pixels));}
            }else{
                result=current->skill?npe_render_map_magic_description(&file,&font,&world,current->id,p->key==0x85?1:-1,pixels):npe_render_map_item_description(&file,&font,&world,p->key==0x85?1:-1,pixels);
                assert(result>=0);
            }
            unsigned bad=map_description_differences(&world,&p->after);fprintf(stderr,"%s phase%u state differences%u\n",current->name,phase,bad);assert(!bad&&result==(int)p->count);
            if(result){map_description_frame(current->name,(int)p->index,native);bad=0;for(unsigned i=0;i<28800;++i)bad+=pixels[i]!=native[i];fprintf(stderr,"%s frame%u differences%u\n",current->name,p->index,bad);assert(!bad);++total;}else ++ignored;
        }
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,after_bitmap,32)&&!memcmp(bitmap,after_bitmap,32));npe_rpg_runtime_close(&file);
    }
    printf("map descriptions: %u original frames/states, %u boundary stops, heap bitmap restored\n",total,ignored);npe_font_free(&font);npe_free(&file);return 0;
}
