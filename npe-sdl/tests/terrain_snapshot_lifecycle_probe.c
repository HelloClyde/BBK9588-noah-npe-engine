#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");assert(f&&!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0&&!fseek(f,0,SEEK_SET));
    uint8_t *p=malloc((size_t)n);assert(p&&fread(p,1,(size_t)n,f)==(size_t)n&&!fclose(f));*size=(size_t)n;return p;
}
static void word(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static unsigned compare(const NpeFile *f,const NpeWorld *w,unsigned number,const uint16_t *grid,unsigned count){
    (void)w;
    unsigned differences=0;for(unsigned i=0;i<count;++i){uint16_t tile;
        assert(npe_rpg_runtime_map_tile(f,number,(uint16_t)i,&tile)==1);differences+=tile!=grid[i];
    }return differences;
}
int main(int argc,char **argv){
    assert(argc==4);size_t length;uint8_t *bytes=read_file(argv[1],&length);NpeFile f;NpeMap map;char error[128],path[512];
    NpeWorld *w=calloc(1,sizeof(*w)),*cold=calloc(1,sizeof(*cold));assert(w&&cold);
    assert(!npe_parse(&f,bytes,length,error,sizeof(error))&&!npe_map(&f,107,&map));unsigned count=map.width*map.height;
    assert(count==450);uint16_t *grid=malloc(count*sizeof(*grid));assert(grid);unsigned failures=0;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],after[NPE_WIDTH*NPE_HEIGHT],records[192],actual[192];
    for(unsigned kind=0;kind<4;++kind){
        if(kind!=1&&kind!=3)assert(!npe_world_load(&f,w,argv[2]));
        else{assert(!npe_world_init(&f,w,108,4,13));w->story_progress=115;}
        assert(!npe_world_resources_open(&f,w));w->exit_choice=1;w->music_enabled=0;
        if(kind!=3){
            assert(npe_rpg_runtime_map_tile_write(&f,108,(uint16_t)(count-1),11)==1);
            memset(records,0,192);word(records,1);word(records+2,80);word(records+32,11);records[63]=0xa5;records[126]=2;records[190]=1;
            assert(!npe_world_resident_write(w,0x52724e,records,192));
            assert(npe_rpg_runtime_map_update(&f,NULL,NULL)==1);
        }
        assert(!npe_world_resident_read(w,0x52724e,records,192));
        uint8_t width=(uint8_t)map.width,height=(uint8_t)map.height;
        if(kind==2){width=map.width==30?25:30;height=(uint8_t)(count/width);
            assert(!npe_rpg_runtime_write(&f,0x527a56,&width,1)&&!npe_rpg_runtime_write(&f,0x527a5a,&height,1));
            assert(npe_rpg_runtime_map_tile_write(&f,108,(uint16_t)(13*width+24),10)==1);
            assert(npe_rpg_runtime_map_tile_write(&f,108,(uint16_t)(13*map.width+24),1)==1);
        }
        for(unsigned i=0;i<count;++i)assert(npe_rpg_runtime_map_tile(&f,108,(uint16_t)i,grid+i)==1);
        assert(!npe_render_world_load(&f,w,pixels));
        snprintf(path,sizeof(path),"%s.%u.pixels",argv[3],kind);
        FILE *out=fopen(path,"wb");assert(out&&fwrite(pixels,1,sizeof(pixels),out)==sizeof(pixels)&&!fclose(out));
        uint8_t *canonical=NULL;size_t canonical_size=0;
        if(kind==3){snprintf(path,sizeof(path),"%s.canonical-live",argv[3]);assert(!npe_world_save(&f,w,path));
            canonical=read_file(path,&canonical_size);assert(!memcmp(canonical,"NPESAV22",8));}
        if(kind==2)assert(!npe_rpg_runtime_shutdown(&f)&&!npe_rpg_runtime_map_current(&f,108));
        npe_rpg_runtime_close(&f);
        snprintf(path,sizeof(path),"%s.%u",argv[3],kind);assert(!npe_world_save(&f,w,path));
        if(kind==3){size_t closed_size;uint8_t *closed=read_file(path,&closed_size);
            assert(closed_size==canonical_size&&!memcmp(closed,canonical,closed_size));free(closed);free(canonical);
            puts("canonical live/closed save:V22 wire identical");}
        assert(!npe_world_load(&f,cold,path));
        unsigned shadow=0;NpeMap view=map;view.width=width;view.height=height;
        for(unsigned i=0;i<count;++i)shadow+=npe_world_tile(cold,&view,i%width,i/width)!=grid[i];
        unsigned shape=kind==3?cold->terrain_snapshot_ready:
            !cold->terrain_snapshot_ready||cold->terrain_snapshot_width!=width||cold->terrain_snapshot_height!=height;
        assert(!npe_world_resources_open(&f,w));unsigned live=compare(&f,w,108,grid,count);
        assert(!npe_world_resident_read(w,0x52724e,actual,192));unsigned metadata=memcmp(actual,records,192)!=0;
        assert(!npe_render_world_load(&f,w,after));unsigned pd=0;for(unsigned i=0;i<sizeof(pixels);++i)pd+=pixels[i]!=after[i];
        printf("case%u closed-save grid=%u shape=%u reopened grid=%u metadata=%u pixels=%u\n",kind,shadow,shape,live,metadata,pd);
        failures+=shadow||shape||live||metadata||pd;
        if(kind==2){uint16_t tile;assert(npe_rpg_runtime_map_tile(&f,108,(uint16_t)(13*width+24),&tile)==1);
            unsigned td=npe_world_tile(w,&map,24,13)!=tile;printf("live width coordinate lookup differences=%u\n",td);failures+=td;
        }
        assert(!npe_world_restart_game(&f,w)&&w->map_number==1&&!w->terrain_snapshot_ready);
        npe_rpg_runtime_close(&f);assert(!npe_world_resources_open(&f,w)&&npe_rpg_runtime_map_current(&f,1));
        npe_rpg_runtime_close(&f);
    }
    printf("terrain close/save/reopen:4 cases,%u failures; explicit shutdown invalidates current map; pending snapshots cleared by restart\n",failures);
    npe_free(&f);free(w);free(cold);free(grid);return failures?1:0;
}
