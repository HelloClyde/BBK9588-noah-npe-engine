#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_bitmap_service_native.h"

static uint8_t memory[0x20000],expected[0x20000],trace[0x54];
static uint8_t *file_bytes;
static size_t file_size;
static uint32_t cursor;
static unsigned reads,seeks;
static const BitmapServiceCase *current;
#ifdef NPE_BATTLE_BITMAP_WORLD
static NpeWorld world;
static void bind_records(uint32_t address,const uint8_t *bytes,unsigned count){
#ifdef NPE_BATTLE_BITMAP_MEMORY
    const uint32_t bases[]={0x526450,0x52730e,0x527a72,0x527a74,0x527aba};
    const unsigned sizes[]={0x6b82-0x6450,144+142,1,1,2};
#else
    const uint32_t bases[]={0x526450,0x5269c6};const unsigned sizes[]={6*148,3*148};
#endif
    for(unsigned i=0;i<sizeof(bases)/sizeof(*bases);++i){
        uint32_t first=address>bases[i]?address:bases[i];
        uint32_t last=address+count<bases[i]+sizes[i]?address+count:bases[i]+sizes[i];
        if(first<last){
#ifdef NPE_BATTLE_BITMAP_MEMORY
            assert(!npe_world_resident_write(&world,first,bytes+first-address,last-first));
#else
            assert(!npe_world_resident_records_write(&world,first,bytes+first-address,last-first));
#endif
        }
    }
}
static void verify_records(void){
    const uint32_t bases[]={0x526450,0x5269c6};const unsigned sizes[]={6*148,3*148};
    uint8_t records[6*148];
    for(unsigned i=0;i<2;++i){
        assert(!npe_world_resident_records_read(&world,bases[i],records,sizes[i]));
        assert(!memcmp(records,memory+bases[i]-0x510000,sizes[i]));
    }
    assert(world.battle_record_party_count==3);
}
#ifdef NPE_BATTLE_BITMAP_MEMORY
static void verify_memory(void){
    const uint32_t bases[]={0x526450,0x52730e,0x527a72,0x527a74,0x527aba};
    const unsigned sizes[]={0x6b82-0x6450,144+142,1,1,2};
    uint8_t bytes[0x6b82-0x6450];
    for(unsigned i=0;i<sizeof(bases)/sizeof(*bases);++i){
        assert(!npe_world_resident_read(&world,bases[i],bytes,sizes[i]));
        assert(!memcmp(bytes,memory+bases[i]-0x510000,sizes[i]));
    }
    assert(world.inventory_prefix_word==(uint16_t)((unsigned)memory[0x167c6]<<8|memory[0x167c7]));
    for(unsigned i=0;i<255;++i)
        assert(npe_world_inventory_word(&world,i)==(uint16_t)((unsigned)memory[0x167c8+2*i]<<8|memory[0x167c9+2*i]));
    assert(npe_world_inventory_word(&world,255)==(uint16_t)((unsigned)memory[0x169c6]<<8|memory[0x169c7]));
}
static void memory_alias_checks(void){
    uint8_t original[4],value[4],actual[4];
    /* Use bytes observed from the independent original warm-31 result;
       these are cross-record/cross-owner writes, not new native samples. */
    assert(!npe_world_resident_read(&world,0x5267c6,original,4));
    memcpy(value,memory+0x169c6,4);
    assert(!npe_world_resident_write(&world,0x5267c7,value,3));
    assert(!npe_world_resident_read(&world,0x5267c6,actual,4));
    assert(actual[0]==original[0] && !memcmp(actual+1,value,3));
    assert(world.inventory_prefix_word==(uint16_t)((unsigned)original[0]<<8|value[0]));
    assert(npe_world_inventory_word(&world,0)==(uint16_t)((unsigned)value[1]<<8|value[2]));
    uint8_t tail[148];assert(!npe_world_battle_record_image(&world,5,tail));
    assert(!memcmp(tail+146,actual,2));
    assert(!npe_world_resident_write(&world,0x5267c6,original,4));
    assert(!npe_world_resident_read(&world,0x5269c4,original,4));
    memcpy(value,memory+0x167c6,4);uint8_t battle_name[12];memcpy(battle_name,world.battle_names[0],12);
    assert(!npe_world_resident_write(&world,0x5269c5,value,3));
    assert(!npe_world_resident_read(&world,0x5269c4,actual,4));
    assert(actual[0]==original[0] && !memcmp(actual+1,value,3));
    assert(npe_world_inventory_word(&world,255)==(uint16_t)((unsigned)value[1]<<8|value[2]));
    assert(!memcmp(battle_name,world.battle_names[0],12));
    assert(!npe_world_resident_write(&world,0x5269c4,original,4));
    assert(!npe_world_resident_read(&world,0x52739d,original,4));
    assert(!npe_world_resident_write(&world,0x52739e,world.skill_record_bytes+140,3));
    assert(!npe_world_resident_read(&world,0x52739d,actual,4));
    assert(actual[0]==original[0] && !memcmp(actual+1,world.skill_record_bytes+140,3));
    assert(!npe_world_resident_write(&world,0x52739d,original,4));
    /* The later object owner makes cache/object flags contiguous. */
    assert(!npe_world_resident_read(&world,0x52742b,original,2));
    assert(!npe_world_resident_write(&world,0x52742b,value,2));
    assert(!npe_world_resident_read(&world,0x52742b,actual,2) && !memcmp(actual,value,2));
    assert(!npe_world_resident_write(&world,0x52742b,original,2));
    assert(!npe_world_resident_read(&world,0x5275dd,original,1));
    assert(npe_world_resident_write(&world,0x5275dd,value,2)==-1);
    assert(!npe_world_resident_read(&world,0x5275dd,actual,1) && actual[0]==original[0]);
    assert(!npe_world_resident_write(&world,0xffffffff,NULL,0));
    fprintf(stderr,"Resident aliases: record5/inventory-1, inventory255/map name, cache boundary, partial and zero writes verified\n");
}
#endif
#endif

static uint16_t read_word(void *context,uint32_t address){
    (void)context;assert(address>=0x510000 && address+1<0x530000);
    unsigned p=address-0x510000;return (uint16_t)((unsigned)memory[p]<<8|memory[p+1]);
}
static void write_word(void *context,uint32_t address,uint16_t value){
    (void)context;assert(address>=0x510000 && address+1<0x530000);
    unsigned p=address-0x510000;memory[p]=(uint8_t)(value>>8);memory[p+1]=(uint8_t)value;
}
static void write_long(uint32_t address,uint32_t value){
    write_word(NULL,address,(uint16_t)(value>>16));write_word(NULL,address+2,(uint16_t)value);
}
static void record_word(unsigned address,uint16_t value){
    unsigned p=address-0x1400;assert(p+1<sizeof(trace));
    trace[p]=(uint8_t)(value>>8);trace[p+1]=(uint8_t)value;
}
static void record_long(unsigned address,uint32_t value){
    record_word(address,(uint16_t)(value>>16));record_word(address+2,(uint16_t)value);
}
static uint32_t file_long(unsigned address){
    assert(address+3<file_size);return (uint32_t)file_bytes[address]<<24 |
        (uint32_t)file_bytes[address+1]<<16 | (uint32_t)file_bytes[address+2]<<8 | file_bytes[address+3];
}
static void seek_file(void *context,uint32_t position){
    (void)context;assert(seeks<2);cursor=position;
    record_long(0x1400,cursor);record_long(0x1440+4*seeks,cursor);
    record_word(0x1406,(uint16_t)++seeks);
}
static unsigned read_file(uint8_t *destination,uint16_t requested){
    assert(reads<3);unsigned caps[]={current->first_cap,current->second_cap,current->payload_cap};
    unsigned count=requested<caps[reads]?requested:caps[reads];
    if(cursor>=file_size)count=0;
    else if(count>file_size-cursor)count=(unsigned)(file_size-cursor);
    if(count)memcpy(destination,file_bytes+cursor,count);
    record_word(0x1410+2*reads,requested);record_word(0x1416+2*reads,(uint16_t)count);
    record_word(0x1404,(uint16_t)++reads);cursor+=count;record_long(0x1400,cursor);
    return count;
}
static void read_local(void *context,uint8_t *destination,uint16_t requested){
    (void)context;assert(reads<2);(void)read_file(destination,requested);
}
static void read_guest(void *context,uint32_t address,uint16_t requested){
    (void)context;assert(reads==2 && address>=0x510000 && address+requested<=0x530000);
    record_long(0x1430,address);unsigned count=read_file(memory+address-0x510000,requested);
#ifdef NPE_BATTLE_BITMAP_WORLD
    bind_records(address,memory+address-0x510000,count);
#else
    (void)count;
#endif
}
static void draw(void *context,uint32_t address,int16_t x,int16_t y,uint8_t kind){
    (void)context;record_long(0x1420,address);record_word(0x1424,(uint16_t)x);
    record_word(0x1426,(uint16_t)y);record_word(0x1428,kind);record_word(0x142a,1);
    if(current->draw_handle>=0)write_word(NULL,0x527a88,(uint16_t)current->draw_handle);
}
static void allocation_failed(void *context,uint16_t request){
    (void)context;record_word(0x1450,1);record_word(0x1452,request);
}
static void compare(const char *path,const uint8_t *actual,unsigned size,uint32_t base){
    FILE *file=fopen(path,"rb");assert(file);
    assert(fread(expected,1,size,file)==size && fgetc(file)==EOF);fclose(file);
    for(unsigned i=0;i<size;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"%s at%06x: C%02x original%02x\n",current->name,(unsigned)(base+i),actual[i],expected[i]);
        assert(0);
    }
}
int main(int argc,char **argv){
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);
    assert(fseek(file,0,SEEK_END)==0);long length=ftell(file);assert(length>0);
    file_size=(size_t)length;file_bytes=malloc(file_size);assert(file_bytes);
    rewind(file);assert(fread(file_bytes,1,file_size,file)==file_size);fclose(file);
    const NpeBattleBitmapService service={{NULL,read_word,write_word,allocation_failed},NULL,seek_file,read_local,read_guest,draw};
    for(unsigned i=0;i<sizeof(bitmap_service_cases)/sizeof(*bitmap_service_cases);++i){
        current=&bitmap_service_cases[i];reads=seeks=0;cursor=0;
        memset(trace,0,sizeof(trace));memset(memory,0xa5,sizeof(memory));
        npe_resident_heap_initialize(&service.heap);
        if(current->warm){
            for(unsigned j=0;j<sizeof(bitmap_heap_nodes)/sizeof(*bitmap_heap_nodes);++j){
                const BitmapHeapNode *node=&bitmap_heap_nodes[j];
                write_word(NULL,node->address,node->flags);write_long(node->address+2,node->next);
            }
            write_word(NULL,NPE_RESIDENT_HEAP_BITMAP,0xfe00);
        }
        write_long(0x5279a6,0x5169bc);write_long(0x5279ca,file_long(0x3a));
        write_long(0x5279ce,file_long(0x42));write_word(NULL,0x527a88,0);
        if(current->number<=29){
            uint32_t position=file_long(0x3a + 9*8),count=file_long(0x3e + 9*8);
            assert(position+count<=file_size && count<=0x530000-0x5169bc);
            memcpy(memory+0x5169bc-0x510000,file_bytes+position,count);
        }
#ifdef NPE_BATTLE_BITMAP_WORLD
        memset(&world,0,sizeof(world));world.game_mode=3;
        world.battle_party_flags_saved=1;world.battle_record_party_count=3;
        memset(world.party_active,1,3);
#ifdef NPE_BATTLE_BITMAP_MEMORY
        bind_records(0x526450,memory+0x16450,0x6b82-0x6450);
        bind_records(0x52730e,memory+0x1730e,144+142);
        bind_records(0x527a72,memory+0x17a72,1);bind_records(0x527a74,memory+0x17a74,1);
        bind_records(0x527aba,memory+0x17aba,2);verify_memory();
#else
        bind_records(0x526450,memory+0x16450,6*148);
        bind_records(0x5269c6,memory+0x169c6,3*148);
#endif
        verify_records();
#endif
        assert(npe_battle_bitmap_service(&service,current->number,7,-3,current->kind)==0);
        compare(current->ram_file,memory,sizeof(memory),0x510000);
        compare(current->trace_file,trace,sizeof(trace),0x1400);
#ifdef NPE_BATTLE_BITMAP_WORLD
        verify_records();
        fprintf(stderr,"World records %s: six battle and three map records match original RAM\n",current->name);
#ifdef NPE_BATTLE_BITMAP_MEMORY
        verify_memory();
        fprintf(stderr,"Resident memory %s:2132 mapped bytes, inventory and both cache owners match original RAM\n",current->name);
        if(!strcmp(current->name,"warm-31"))memory_alias_checks();
#endif
        /* Native warm-31 contains different map and battle HP/ID/gear
           bytes. Map writes must not replace the active battle fields. */
        if(!strcmp(current->name,"warm-31")){
            for(unsigned m=0;m<3;++m){
                const uint8_t *b=memory+0x16450+148*m,*p=memory+0x169c6+148*m;
                assert(world.hp[m]==(uint16_t)((unsigned)b[80]<<8|b[81]));
                assert(world.battle_record_ids[m]==b[12] && world.party_ids[m]==p[12]);
                assert(!memcmp(world.equipped_items[m],b+18,6));
            }
            uint8_t before_map[148],before_battle[148],after[148];
            assert(!npe_world_map_record_image(&world,0,before_map));
            assert(!npe_world_battle_record_image(&world,0,before_battle));
            /* Split word writes and simultaneous physical owners use
               bytes observed from the original, including signed words. */
            for(unsigned j=0;j<2;++j)assert(!npe_world_resident_records_write(&world,0x5264a0+j,before_map+80+j,1));
            assert(!npe_world_map_record_image(&world,0,after));assert(!memcmp(after,before_map,148));
            for(unsigned j=0;j<2;++j)assert(!npe_world_resident_records_write(&world,0x526a16+j,before_battle+80+j,1));
            assert(world.hp[0]==(uint16_t)((unsigned)before_map[80]<<8|before_map[81]));
            assert(!npe_world_resident_records_read(&world,0x5264a0,after,2));assert(!memcmp(after,before_map+80,2));
            assert(!npe_world_resident_records_read(&world,0x526a16,after,2));assert(!memcmp(after,before_battle+80,2));
            assert(npe_world_resident_records_write(&world,0x5267c7,before_map,2)==-1);
            fprintf(stderr,"World warm-31: distinct physical HP/IDs/gear, split byte writes and record bounds verified\n");
        }
#endif
        fprintf(stderr,"Original bitmap loader %s: complete128KiB RAM and IO/draw records match\n",current->name);
    }
    free(file_bytes);fprintf(stderr,"Battle bitmap loader: %u original-service cases verified\n",
        (unsigned)(sizeof(bitmap_service_cases)/sizeof(*bitmap_service_cases)));return 0;
}
