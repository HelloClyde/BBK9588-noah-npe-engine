#include "../npe.h"
#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shop_text_probe_helpers.h"
int main(int argc,char **argv){
    assert(argc==2);FILE *in=fopen(argv[1],"rb");assert(in&&!fseek(in,0,SEEK_END));long size=ftell(in);assert(size>0&&!fseek(in,0,SEEK_SET));
    uint8_t *data=malloc((size_t)size);assert(data&&fread(data,1,(size_t)size,in)==(size_t)size&&!fclose(in));
    NpeFile file;char error[128];assert(!npe_parse(&file,data,(size_t)size,error,sizeof(error)));
    NpeFont font={0};assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    NpeWorld world;unsigned total=0,ignored=0;
    for(unsigned c=0;c<sizeof(shop_text_cases)/sizeof(*shop_text_cases);++c){const ShopTextCase *current=shop_text_cases+c;
        assert(!npe_world_init(&file,&world,1,7,10)&&!npe_world_resources_open(&file,&world));shop_text_seed(&world,&current->before);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],base[sizeof(pixels)],native[sizeof(pixels)],bitmap[32],after_bitmap[32];
        shop_text_frame(current->name,-1,base);memcpy(pixels,base,sizeof(pixels));
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,bitmap,32));
        for(unsigned phase=0;phase<current->count;++phase){const ShopTextPhase *p=current->phases+phase;
            shop_text_patch(&world,p);int submitted=1;
            uint32_t cursor=npe_rpg_runtime_cursor(&file);unsigned reads=npe_rpg_runtime_reads(&file);
            if(!phase){world.shop_ui=2;world.shop_quantity=1;world.shop_choice=0;
                assert(!npe_render_shop_quantity(&file,&font,&world,pixels));
            }else if(p->key==0x84||p->key==0x85){submitted=npe_render_shop_description(&file,&font,&world,p->key==0x85?1:-1,pixels);assert(submitted>=0);
            }else if(p->key==0x82){assert(!npe_world_shop_quantity(&world,world.shop_selected_item,1));assert(!npe_render_shop_quantity_counter(&font,&world,pixels));
            }else if(p->key==0x80){world.shop_choice=(uint8_t)(((unsigned)world.shop_choice+1u)%2u);npe_render_shop_choice_move(pixels);
            }else{assert(p->key==0x8e);world.shop_ui=1;memcpy(pixels,base,sizeof(pixels));}
            if(p->key==0x82||p->key==0x80||!submitted)assert(cursor==npe_rpg_runtime_cursor(&file)&&reads==npe_rpg_runtime_reads(&file));
            assert(submitted==(int)p->count);
            unsigned bad=shop_text_difference(&world,&p->after);fprintf(stderr,"%s phase%u state differences%u\n",current->name,phase,bad);assert(!bad);
            if(submitted){shop_text_frame(current->name,(int)p->index,native);bad=0;for(unsigned i=0;i<sizeof(pixels);++i)bad+=pixels[i]!=native[i];
                fprintf(stderr,"%s frame%u differences%u\n",current->name,p->index,bad);assert(!bad);++total;
            }else ++ignored;
        }
        assert(!npe_rpg_runtime_read(&file,NPE_RESIDENT_HEAP_BITMAP,after_bitmap,32)&&!memcmp(bitmap,after_bitmap,32));npe_rpg_runtime_close(&file);
    }
    printf("shop text: %u original frame/state comparisons, %u ignored boundaries, temporary heap bitmap restored\n",total,ignored);
    npe_font_free(&font);npe_free(&file);return 0;
}
