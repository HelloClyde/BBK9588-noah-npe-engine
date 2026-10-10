/* RAM-backed production adapters for the original resource and draw services.
   File IO reads the current NPE, with partial/EOF writes retaining RAM tails. */
struct NpeRpgRuntime {
    const NpeFile *file;
    NpeResidentBindings bindings;
    NpeResidentHeap heap;
    uint8_t ram[262144];
    uint8_t *pixels;
    uint32_t cursor;
    unsigned failures,reads;
    unsigned map_number;
    int map_buffer_released;
    NpeWorld *terrain_world; /* Set only by the typed World map-load adapter. */
    int error;
    NpeAnimationStream scene;
    NpeAnimationStream role;
    int16_t role_x,role_y;
    float role_dx,role_dy;
    unsigned role_steps,role_position;
};
static const uint32_t rpg_image_group_addresses[9]={
    0x527afc,0x527ae6,0x527aee,0x527afe,0x527af2,
    0x527ae4,0x527af8,0x527aec,0x527aea
};
static int rpg_runtime_range(uint32_t address,unsigned count){
    return address>=UINT32_C(0x510000)&&address<=UINT32_C(0x550000)&&
           count<=UINT32_C(0x550000)-address;
}
static uint8_t rpg_runtime_byte(NpeRpgRuntime *r,uint32_t address){
    uint8_t value;
    address&=UINT32_C(0xffffff);
    if(!rpg_runtime_range(address,1)){r->error=1;return 0;}
    if(r->bindings.read&&!r->bindings.read(r->bindings.context,address,&value,1))return value;
    return r->ram[address-UINT32_C(0x510000)];
}
static void rpg_runtime_byte_write(NpeRpgRuntime *r,uint32_t address,uint8_t value){
    address&=UINT32_C(0xffffff);
    if(!rpg_runtime_range(address,1)){r->error=1;return;}
    r->ram[address-UINT32_C(0x510000)]=value;
    if(r->bindings.write)r->bindings.write(r->bindings.context,address,&value,1);
}
static uint16_t rpg_runtime_word(void *context,uint32_t address){
    NpeRpgRuntime *r=context;
    return (uint16_t)((unsigned)rpg_runtime_byte(r,address)<<8|rpg_runtime_byte(r,address+1));
}
static void rpg_runtime_word_write(void *context,uint32_t address,uint16_t value){
    rpg_runtime_byte_write(context,address,(uint8_t)(value>>8));
    rpg_runtime_byte_write(context,address+1,(uint8_t)value);
}
static void rpg_runtime_failed(void *context,uint16_t request){
    (void)request;NpeRpgRuntime *r=context;++r->failures;
}
static void rpg_runtime_seek(void *context,uint32_t position){
    NpeRpgRuntime *r=context;r->cursor=position;
}
static unsigned rpg_runtime_available(NpeRpgRuntime *r,unsigned count){
    if(r->cursor>=r->file->size)return 0;
    size_t available=r->file->size-r->cursor;
    return count<available?count:(unsigned)available;
}
static void rpg_runtime_read_local(void *context,uint8_t *destination,uint16_t count){
    NpeRpgRuntime *r=context;unsigned supplied=rpg_runtime_available(r,count);
    ++r->reads;
    if(supplied)memcpy(destination,r->file->bytes+r->cursor,supplied);
    r->cursor+=supplied;
}
static void rpg_runtime_read_guest(void *context,uint32_t address,uint16_t count){
    NpeRpgRuntime *r=context;unsigned supplied=rpg_runtime_available(r,count);
    ++r->reads;
    for(unsigned i=0;i<supplied;++i)rpg_runtime_byte_write(r,address+i,r->file->bytes[r->cursor+i]);
    r->cursor+=supplied;
}
static void rpg_runtime_draw_guest(void *context,uint32_t address,int16_t x,int16_t y,uint8_t kind){
    NpeRpgRuntime *r=context;
    if(npe_draw_guest_bitmap(&r->heap,address,x,y,kind,r->pixels))r->error=1;
}
static int rpg_runtime_open(NpeFile *file,const NpeResidentBindings *bindings,int packed){
    if(!file||!file->bytes||file->section_count<=9||file->rpg_runtime)return -1;
    NpeRpgRuntime *r=calloc(1,sizeof(*r));if(!r)return -1;
    r->file=file;if(bindings)r->bindings=*bindings;
    r->heap=(NpeResidentHeap){r,rpg_runtime_word,rpg_runtime_word_write,rpg_runtime_failed};
    npe_resident_heap_initialize(&r->heap);
    const NpeResidentHeapOwnerService owners={r->heap,r,NULL};
    if(npe_resident_heap_launcher_buffers(&owners)!=1){free(r);return -1;}
    npe_rpg_map_buffer_allocate(&r->heap);
    heap_set_long(&r->heap,UINT32_C(0x5279be),file->sections[9].size);
    heap_set_long(&r->heap,UINT32_C(0x5279c2),file->sections[9].offset);
    heap_set_long(&r->heap,UINT32_C(0x5279c6),file->sections[0].size);
    heap_set_long(&r->heap,UINT32_C(0x5279ca),file->sections[0].offset);
    heap_set_long(&r->heap,UINT32_C(0x5279ce),file->sections[1].offset);
    /* Source31:E956..EAAC reads header vectors0..8 in order. */
    for(unsigned group=0;group<9;++group)
        rpg_runtime_word_write(r,rpg_image_group_addresses[group],file->vectors[group]);
    if(file->section_count>14){
        heap_set_long(&r->heap,UINT32_C(0x5279f6),file->sections[13].offset);
        heap_set_long(&r->heap,UINT32_C(0x5279fa),file->sections[14].offset);
    }
    if(file->section_count>5)heap_set_long(&r->heap,UINT32_C(0x5279f2),file->sections[5].offset);
    if(file->section_count>7){
        heap_set_long(&r->heap,UINT32_C(0x5279de),file->sections[6].offset);
        heap_set_long(&r->heap,UINT32_C(0x5279e2),file->sections[7].offset);
    }
    if(file->section_count>4)heap_set_long(&r->heap,UINT32_C(0x5279b6),file->sections[4].offset);
    /* Source31:F076..F1A4 caches directory16..19 finite scene sounds. */
    for(unsigned i=0;i<4;++i)if(file->section_count>16+i){
        heap_set_long(&r->heap,UINT32_C(0x5278c4)+4u*i,file->sections[16+i].size);
        heap_set_long(&r->heap,UINT32_C(0x5278d4)+4u*i,file->sections[16+i].offset);
    }
    /* Source31:F28A..F4A0 caches directory23..29 battle backgrounds. */
    for(unsigned i=0;i<7;++i)if(file->section_count>23+i){
        heap_set_long(&r->heap,UINT32_C(0x527856)+4u*i,file->sections[23+i].size);
        heap_set_long(&r->heap,UINT32_C(0x527872)+4u*i,file->sections[23+i].offset);
    }
    /* Source31:F1A6..F288 caches directory20..22 map offset/length pairs. */
    for(unsigned i=0;i<3;++i)if(file->section_count>20+i){
        heap_set_long(&r->heap,UINT32_C(0x5278f2)+4u*i,file->sections[20+i].size);
        heap_set_long(&r->heap,UINT32_C(0x5278fe)+4u*i,file->sections[20+i].offset);
    }
    /*31:F4A2..F6FC reads directory30..37 in this address order.
      Battle38:E548 copies33/36 by sex;3E:F85E uses the remaining roles. */
    const uint32_t sound_lengths[]={0x52794a,0x527972,0x52796a,0x527962,
                                   0x52795a,0x527952,0x527942,0x52793a};
    for(unsigned i=0;i<8;++i)if(file->section_count>30+i){
        heap_set_long(&r->heap,sound_lengths[i],file->sections[30+i].size);
        heap_set_long(&r->heap,sound_lengths[i]+4,file->sections[30+i].offset);
    }
    const NpeRpgResourceService resources={r->heap,r,rpg_runtime_seek,rpg_runtime_read_guest};
    if((packed && npe_rpg_battle_resources_allocate(&resources))||r->error){free(r);return -1;}
    file->rpg_runtime=r;return 0;
}
int npe_rpg_runtime_open(NpeFile *file,const NpeResidentBindings *bindings){
    return rpg_runtime_open(file,bindings,1);
}
int npe_rpg_runtime_open_idle(NpeFile *file,const NpeResidentBindings *bindings){
    return rpg_runtime_open(file,bindings,0);
}
static void rpg_runtime_preserve_terrain(NpeRpgRuntime *r){
    NpeWorld *w=r->terrain_world;
    if(!w || r->bindings.context!=w || !r->map_number || r->map_number!=w->map_number)return;
    uint8_t width=rpg_runtime_byte(r,UINT32_C(0x527a56));
    uint8_t height=rpg_runtime_byte(r,UINT32_C(0x527a5a));
    uint16_t product=(uint16_t)((unsigned)width*height);
    unsigned count=product<0x8000u?product:0;
    uint32_t address=heap_long(&r->heap,UINT32_C(0x52797e));
    /* The original map allocation is always7000 bytes. Zero/small or
       signed-negative dimensions leave its unused cells live across maps. */
    unsigned stored=count<NPE_MAP_BUFFER_WORDS?NPE_MAP_BUFFER_WORDS:count;
    if(!rpg_runtime_range(address,2u*stored)){
        w->terrain_snapshot_ready=w->terrain_buffer_snapshot_ready=0;return;
    }
    for(unsigned i=0;i<stored;++i)w->terrain_snapshot_cells[i]=rpg_runtime_word(r,address+2u*i);
    w->terrain_buffer_snapshot_ready=1;
    w->terrain_snapshot_count=(uint16_t)count;
    w->terrain_snapshot_width=width;w->terrain_snapshot_height=height;
    w->terrain_snapshot_ready=1;
}
int npe_rpg_runtime_finish_game(const NpeFile *file){
    if(!file || !file->rpg_runtime)return -1;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    if(npe_rpg_finish_game_service(&r->heap)||r->error)return -1;
    r->map_number=0;r->terrain_world=NULL;r->map_buffer_released=1;
    return 0;
}
int npe_rpg_runtime_shutdown(const NpeFile *file){
    if(!file || !file->rpg_runtime)return -1;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    rpg_runtime_preserve_terrain(r);
    int result=npe_rpg_resources_release(&r->heap)||r->error?-1:0;
    r->map_number=0;r->terrain_world=NULL;return result;
}
void npe_rpg_runtime_close(NpeFile *file){
    if(!file||!file->rpg_runtime)return;
    NpeRpgRuntime *r=file->rpg_runtime;
    rpg_runtime_preserve_terrain(r);r->terrain_world=NULL;
    npe_animation_stream_close(&r->heap,&r->scene);
    npe_animation_stream_close(&r->heap,&r->role);
    (void)npe_rpg_runtime_shutdown(file);
    npe_resident_heap_launcher_cleanup(&r->heap);
    free(r);file->rpg_runtime=NULL;
}
int npe_rpg_runtime_read(const NpeFile *file,uint32_t address,uint8_t *bytes,unsigned count){
    if(!file||!file->rpg_runtime||(!bytes&&count)||!rpg_runtime_range(address,count))return -1;
    for(unsigned i=0;i<count;++i)bytes[i]=rpg_runtime_byte(file->rpg_runtime,address+i);
    return 0;
}
int npe_rpg_runtime_write(const NpeFile *file,uint32_t address,const uint8_t *bytes,unsigned count){
    if(!file||!file->rpg_runtime||(!bytes&&count)||!rpg_runtime_range(address,count))return -1;
    for(unsigned i=0;i<count;++i)rpg_runtime_byte_write(file->rpg_runtime,address+i,bytes[i]);
    return 0;
}
uint32_t npe_rpg_runtime_cursor(const NpeFile *file){return file&&file->rpg_runtime?file->rpg_runtime->cursor:0;}
int npe_rpg_runtime_map_tile_records(const NpeFile *file,const NpeMap *map,uint16_t progress){
    if(!file || !file->rpg_runtime || !map ||
       !map->tail || map->tail_size<703)return -1;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    rpg_runtime_word_write(r,UINT32_C(0x527a66),progress);
    rpg_runtime_seek(r,(uint32_t)(map->tail-file->bytes)+703u);
    const NpeBattleBitmapService service={r->heap,r,rpg_runtime_seek,
        rpg_runtime_read_local,rpg_runtime_read_guest,NULL};
    int result=npe_map_tile_record_service(&service,map->tail[654]);
    return r->error?-2:result;
}
unsigned npe_rpg_runtime_reads(const NpeFile *file){return file&&file->rpg_runtime?file->rpg_runtime->reads:0;}
int npe_rpg_runtime_map_grid(const NpeFile *file,const NpeMap *map){
    if(!file || !file->rpg_runtime || !map || !map->tile_words)return -1;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;r->map_number=0;
    /* After mode21 the next title constructor/occupied-slot loader owns
      a fresh7000-byte map buffer (46:D1C6 /3A:9024). Never reuse the
      released published pointer, which may now refer to another chunk. */
    if(r->map_buffer_released){
        if(npe_rpg_map_buffer_allocate(&r->heap)||r->error)return -2;
        r->map_buffer_released=0;
    }
    enemy_guest_byte_write(&r->heap,UINT32_C(0x527a56),map->width);
    enemy_guest_byte_write(&r->heap,UINT32_C(0x527a5a),map->height);
    rpg_runtime_seek(r,(uint32_t)(map->tile_words-file->bytes));
    const NpeBattleBitmapService service={r->heap,r,rpg_runtime_seek,
        rpg_runtime_read_local,rpg_runtime_read_guest,NULL};
    int result=npe_map_grid_stream_service(&service);
    return r->error?-2:result;
}
int npe_rpg_runtime_map_current(const NpeFile *file,unsigned number){
    return file && file->rpg_runtime && number && file->rpg_runtime->map_number==number;
}
int npe_rpg_runtime_map_update(const NpeFile *file,NpeMapTileObserver observe,void *context){
    if(!file || !file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    int result=npe_map_tile_update_service(&r->heap,observe,context);
    return r->error?-2:result;
}
int npe_rpg_runtime_map_tile_write(const NpeFile *file,unsigned number,uint16_t index,uint16_t tile){
    if(!file || !file->rpg_runtime)return 0;
    NpeRpgRuntime *r=file->rpg_runtime;if(!number || r->map_number!=number)return 0;
    r->error=0;
    uint32_t address=heap_long(&r->heap,UINT32_C(0x52797e))+
        (uint32_t)(2*bitmap_signed_word(index));
    rpg_runtime_word_write(r,address,tile);return r->error?-2:1;
}
int npe_rpg_runtime_map_tile(const NpeFile *file,unsigned number,uint16_t index,uint16_t *tile){
    if(!file || !file->rpg_runtime)return 0;
    NpeRpgRuntime *r=file->rpg_runtime;if(!number || r->map_number!=number)return 0;
    if(!tile)return -2;
    r->error=0;
    uint32_t address=heap_long(&r->heap,UINT32_C(0x52797e))+
        (uint32_t)(2*bitmap_signed_word(index));
    *tile=rpg_runtime_word(r,address);return r->error?-2:1;
}
int npe_rpg_runtime_map_cache(const NpeFile *file,unsigned number,unsigned x,unsigned y,uint16_t tiles[105]){
    if(!file || !file->rpg_runtime)return 0;
    NpeRpgRuntime *r=file->rpg_runtime;if(!number || r->map_number!=number)return 0;
    if(!tiles)return -2;
    r->error=0;
    enemy_guest_byte_write(&r->heap,UINT32_C(0x527a2e),(uint8_t)x);
    enemy_guest_byte_write(&r->heap,UINT32_C(0x527a2c),(uint8_t)y);
    if(npe_map_viewport_cache_refresh(&r->heap))return -2;
    for(unsigned i=0;i<105;++i)tiles[i]=rpg_runtime_word(r,UINT32_C(0x5270b4)+2u*i);
    return r->error?-2:1;
}
int npe_rpg_runtime_map_objects_clear(const NpeFile *file,uint16_t slot){
    if(!file || !file->rpg_runtime)return -1;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    int cleared=npe_map_object_clear_service(&r->heap,slot);
    return r->error?-1:cleared;
}
int npe_rpg_runtime_map_load(const NpeFile *file,const NpeWorld *world,int restore){
    if(!file || !file->rpg_runtime || !world || npe_rpg_runtime_owner(file)!=world)return -1;
    NpeMap map;if(!world->map_number || npe_map(file,world->map_number-1,&map))return -1;
    NpeRpgRuntime *r=file->rpg_runtime;
    /* Original0C:BE84..BEE2 writes exactly12 name bytes and a NUL at78F0.
       The following78F1 byte remains resident across map loads. A trailing
       high byte can consume that NUL as its glyph trail, then read78F1. */
    for(unsigned i=0;i<sizeof(map.name);++i)
        rpg_runtime_byte_write(r,UINT32_C(0x5278e4)+i,map.name[i]);
    rpg_runtime_byte_write(r,UINT32_C(0x5278f0),0);
    int snapshot=restore && world->terrain_snapshot_ready;
    uint8_t records[192];if(snapshot)memcpy(records,world->tile_record_bytes,192);
    if(npe_rpg_runtime_map_grid(file,&map))return -1;
    if(npe_rpg_runtime_map_objects_clear(file,UINT16_MAX)<0 ||
       npe_rpg_runtime_map_tile_records(file,&map,world->story_progress))return -1;
    r->map_number=world->map_number;
    if(restore){
        if(snapshot){
            /* Own terrain snapshots restore logical cells and metadata;
               V24 also keeps the unused fixed map-buffer cells. No heap
               address is serialized; writes follow the newly allocated buffer. */
            if(npe_rpg_runtime_write(file,UINT32_C(0x52724e),records,192))return -1;
            enemy_guest_byte_write(&r->heap,UINT32_C(0x527a56),world->terrain_snapshot_width);
            enemy_guest_byte_write(&r->heap,UINT32_C(0x527a5a),world->terrain_snapshot_height);
            unsigned restored_count=world->terrain_snapshot_count;
            if(world->terrain_buffer_snapshot_ready && restored_count<NPE_MAP_BUFFER_WORDS)
                restored_count=NPE_MAP_BUFFER_WORDS;
            for(unsigned i=0;i<restored_count;++i)
                if(npe_rpg_runtime_map_tile_write(file,world->map_number,(uint16_t)i,
                    world->terrain_snapshot_cells[i])<0)return -1;
        }else{
        /* Own-save restoration replays its validated live cell order;
           this is distinct from the original fresh map load above. */
        for(unsigned i=0;i<world->tile_override_count;++i)
            if(npe_rpg_runtime_map_tile_write(file,world->map_number,
                world->tile_override_indices[i],world->tile_override_tiles[i])<0)return -1;
        /* Missing file slots retain their high flag byte in the original
           loader; own restoration overlays only declared slot flags. */
        for(unsigned i=0;i<3 && i<map.tail[654];++i)enemy_guest_byte_write(&r->heap,
            UINT32_C(0x52728c)+64u*i,(uint8_t)((world->tile_change_applied>>i)&1u));
        }
    }
    uint16_t tiles[105];int result=npe_rpg_runtime_map_cache(file,world->map_number,
        world->view.offset_x,world->view.offset_y,tiles)<0?-1:0;
    if(!result)r->terrain_world=r->bindings.context;
    return result;
}
int npe_rpg_runtime_viewport_refresh(const NpeFile *file){
    if(!file || !file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    int result=npe_map_viewport_cache_refresh(&r->heap);
    return r->error?-2:result;
}
uint16_t npe_image_group_index(const NpeFile *file,unsigned group,uint16_t offset){
    if(!file||group>=17)return UINT16_MAX;
    uint16_t base=file->vectors[group];
    if(file->rpg_runtime && group<9)
        base=rpg_runtime_word(file->rpg_runtime,rpg_image_group_addresses[group]);
    return (uint16_t)(base+offset);
}
unsigned npe_rpg_runtime_failures(const NpeFile *file){return file&&file->rpg_runtime?file->rpg_runtime->failures:0;}
typedef struct {NpeRpgRuntime *runtime;NpeTextBufferDraw draw;void *context;} RpgTextDraw;
static void rpg_text_seek(void *context,uint32_t position){
    rpg_runtime_seek(((RpgTextDraw *)context)->runtime,position);
}
static void rpg_text_read(void *context,uint8_t *bytes,uint16_t count){
    rpg_runtime_read_local(((RpgTextDraw *)context)->runtime,bytes,count);
}
static void rpg_text_draw(void *context,uint32_t address,const uint16_t rect[4],int clear){
    RpgTextDraw *text=context;text->draw(text->context,address,rect,clear);
}
static void rpg_text_backdrop(void *context){
    NpeRpgRuntime *r=((RpgTextDraw *)context)->runtime;
    memset(r->pixels,0,NPE_WIDTH*NPE_HEIGHT);
    const NpeBattleBitmapService service={r->heap,r,rpg_runtime_seek,rpg_runtime_read_local,
        rpg_runtime_read_guest,rpg_runtime_draw_guest};
    uint16_t index=(uint16_t)(rpg_runtime_word(r,UINT32_C(0x527aec))+19u);
    if(npe_indexed_bitmap_service(&service,(int16_t)index,0,0,0))r->error=1;
}
int npe_rpg_runtime_text_substring(const NpeFile *file,uint32_t destination,uint32_t source,
                                    uint16_t start,uint16_t count,uint16_t *result){
    if(!file||!file->rpg_runtime||!result)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    *result=npe_text_substring_service(&r->heap,destination,source,start,count);
    return r->error?-1:0;
}
int npe_rpg_runtime_text_length(const NpeFile *file,uint32_t source,int cursor_kind,uint16_t *result){
    if(!file||!file->rpg_runtime||!result)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    *result=npe_text_length_service(&r->heap,source,cursor_kind);
    return r->error?-1:0;
}
/* The formatter consumes an original caller-local string. Keep that local
   separate from shared RAM until the original stack address is modelled. */
typedef struct {NpeRpgRuntime *r;uint8_t *bytes;unsigned capacity;int error;} RpgTextSlice;
#define RPG_TEXT_LOCAL_ADDRESS UINT32_C(0x010000)
static uint16_t rpg_slice_word(void *context,uint32_t address){
    RpgTextSlice *s=context;
    if(address>=RPG_TEXT_LOCAL_ADDRESS&&address<RPG_TEXT_LOCAL_ADDRESS+65536u){
        unsigned i=address-RPG_TEXT_LOCAL_ADDRESS;
        uint16_t value=i<s->capacity?(uint16_t)((unsigned)s->bytes[i]<<8):0;
        return (uint16_t)(value|(i+1u<s->capacity?s->bytes[i+1u]:0));
    }
    return rpg_runtime_word(s->r,address);
}
static void rpg_slice_word_write(void *context,uint32_t address,uint16_t value){
    RpgTextSlice *s=context;
    if(address>=RPG_TEXT_LOCAL_ADDRESS&&address<RPG_TEXT_LOCAL_ADDRESS+65536u){
        unsigned i=address-RPG_TEXT_LOCAL_ADDRESS;
        if(i>=s->capacity){s->error=1;return;}
        s->bytes[i]=(uint8_t)(value>>8);
        if(i+1u<s->capacity)s->bytes[i+1u]=(uint8_t)value;
        return;
    }
    /* This adapter never permits caller-local writes to shared RAM. */
    s->error=1;
}
int npe_rpg_runtime_text_slice(const NpeFile *file,uint32_t source,uint16_t start,uint16_t count,
                                uint8_t *output,unsigned capacity,uint16_t *result){
    if(!file||!file->rpg_runtime||!output||!capacity||!result)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    RpgTextSlice slice={r,output,capacity,0};
    NpeResidentHeap heap={&slice,rpg_slice_word,rpg_slice_word_write,NULL};
    *result=npe_text_substring_service(&heap,RPG_TEXT_LOCAL_ADDRESS,source,start,count);
    return r->error||slice.error?-1:0;
}
#undef RPG_TEXT_LOCAL_ADDRESS
int npe_rpg_runtime_text_prefix(const NpeFile *file){
    if(!file||!file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;uint8_t ignored=0;
    rpg_runtime_seek(r,heap_long(&r->heap,UINT32_C(0x52797a)));
    rpg_runtime_read_local(r,&ignored,1);
    rpg_runtime_word_write(r,UINT32_C(0x527a1a),
        (uint16_t)(rpg_runtime_word(r,UINT32_C(0x527a1a))+1u));
    return r->error?-1:0;
}
int npe_rpg_runtime_story_page(const NpeFile *file,NpeTextBufferDraw draw,void *context,uint8_t *pixels){
    if(!file||!file->rpg_runtime||!draw||!pixels)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;r->pixels=pixels;
    RpgTextDraw text={r,draw,context};
    const NpeTextPageService service={r->heap,&text,rpg_text_seek,rpg_text_read,
        rpg_text_draw,rpg_text_backdrop,NULL,0};
    int result=npe_story_text_page_service(&service);
    r->pixels=NULL;return result||r->error?-1:0;
}
int npe_rpg_runtime_npc_page(const NpeFile *file,NpeTextBufferDraw draw,void *context,uint8_t *pixels,
                             int entry,NpeTextPageLoad load,void *load_context){
    if(!file||!file->rpg_runtime||!draw||!pixels)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;r->pixels=pixels;
    RpgTextDraw text={r,draw,context};
    const NpeTextPageService service={r->heap,&text,rpg_text_seek,rpg_text_read,
        rpg_text_draw,NULL,NULL,0};
    int result=npe_npc_text_page_service(&service,entry,load,load_context);
    r->pixels=NULL;return result||r->error?-1:0;
}
void *npe_rpg_runtime_owner(const NpeFile *file){return file&&file->rpg_runtime?file->rpg_runtime->bindings.context:NULL;}
static void rpg_enemy_seek(void *context,uint32_t position){
    NpeRpgRuntime *r=context;
    if(position<=r->file->size)r->cursor=position;
}
int npe_rpg_runtime_map_sound(const NpeFile *file,NpeMapSoundRequest *request){
    if(!file || !file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    int result=npe_map_sound_request(&r->heap,request);
    return r->error?-2:result;
}
int npe_rpg_runtime_battle_sound(const NpeFile *file,NpeBattleSoundRequest *request){
    if(!file || !file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    int result=npe_battle_sound_request(&r->heap,request);
    return r->error?-2:result;
}
int npe_rpg_runtime_music_controller(const NpeFile *file,unsigned operation,NpeMusicControllerObserver observer,void *context){
    if(!file || !file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    int result=npe_music_controller_apply(&r->heap,operation,observer,context);
    return r->error?-2:result;
}
int npe_rpg_runtime_background_sound(const NpeFile *file,unsigned phase,unsigned value,NpeRpgSoundRequest *request){
    if(!file || !file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;int result;
    if(!phase)result=npe_scene_sound_request(&r->heap,value,request);
    else if(phase==1)result=npe_battle_background_start(&r->heap,value,request);
    else if(phase==2)result=npe_battle_background_replay(&r->heap,request);
    else return -1;
    return r->error?-2:result;
}
int npe_rpg_runtime_party_descriptors(const NpeFile *file,unsigned party_count,int phase){
    if(!file||!file->rpg_runtime||party_count>255||(phase!=0&&phase!=1))return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    enemy_guest_byte_write(&r->heap,UINT32_C(0x527afa),(uint8_t)party_count);
    int status=0;
    if(!phase){
        status=npe_party_battle_positions_initialize(&r->heap);
        if(!status)status=npe_party_battle_place(&r->heap);
    }else status=npe_party_battle_audio(&r->heap);
    return r->error?-2:status;
}
int npe_rpg_runtime_load_enemies(const NpeFile *file,unsigned party_count){
    if(!file||!file->rpg_runtime||party_count>255)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    enemy_guest_byte_write(&r->heap,UINT32_C(0x527afa),(uint8_t)party_count);
    const NpeBattleBitmapService s={r->heap,r,rpg_enemy_seek,rpg_runtime_read_local,rpg_runtime_read_guest,NULL};
    int status=npe_enemy_record_service(&s);return r->error?-2:status;
}
int npe_rpg_runtime_battle_resources(const NpeFile *file,int active){
    if(!file||!file->rpg_runtime)return -1;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    const NpeRpgResourceService service={r->heap,r,rpg_runtime_seek,rpg_runtime_read_guest};
    int result=active?npe_rpg_battle_resources_allocate(&service):npe_rpg_battle_resources_release(&r->heap);
    return result||r->error?-1:0;
}
int npe_rpg_runtime_draw(const NpeFile *file,uint16_t number,int16_t x,int16_t y,uint8_t kind,
                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!file||!file->rpg_runtime||!pixels)return -1;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;r->pixels=pixels;
    const NpeBattleBitmapService service={r->heap,r,rpg_runtime_seek,rpg_runtime_read_local,rpg_runtime_read_guest,rpg_runtime_draw_guest};
    int result=npe_battle_bitmap_service(&service,number,x,y,kind);
    r->pixels=NULL;return result||r->error?-1:0;
}
int npe_rpg_runtime_draw_indexed(const NpeFile *file,int16_t index,int16_t x,int16_t y,int16_t kind,
                                 uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!file||!file->rpg_runtime||!pixels)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;r->pixels=pixels;
    const NpeBattleBitmapService service={r->heap,r,rpg_runtime_seek,rpg_runtime_read_local,rpg_runtime_read_guest,rpg_runtime_draw_guest};
    int result=npe_indexed_bitmap_service(&service,index,x,y,kind);
    r->pixels=NULL;return r->error?-2:result;
}
int npe_rpg_runtime_scene_begin(const NpeFile *file,int16_t group){
    if(!file||!file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    const NpeBattleBitmapService service={r->heap,r,rpg_runtime_seek,rpg_runtime_read_local,rpg_runtime_read_guest,rpg_runtime_draw_guest};
    int result=npe_scene_animation_stream_open(&service,group,&r->scene);
    return r->error?-2:result;
}
unsigned npe_rpg_runtime_scene_frames(const NpeFile *file){
    return file&&file->rpg_runtime&&file->rpg_runtime->scene.active?file->rpg_runtime->scene.frames:0;
}
int npe_rpg_runtime_scene_frame(const NpeFile *file,int16_t x,int16_t y,uint8_t kind,
                                uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!file||!file->rpg_runtime||!pixels)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;r->pixels=pixels;
    const NpeBattleBitmapService service={r->heap,r,rpg_runtime_seek,rpg_runtime_read_local,rpg_runtime_read_guest,rpg_runtime_draw_guest};
    int result=npe_animation_stream_draw(&service,&r->scene,x,y,kind);
    r->pixels=NULL;return r->error?-2:result;
}
void npe_rpg_runtime_scene_end(const NpeFile *file){
    if(file&&file->rpg_runtime)npe_animation_stream_close(&file->rpg_runtime->heap,&file->rpg_runtime->scene);
}
int npe_rpg_runtime_role_begin(const NpeFile *file,int16_t group,int16_t x,int16_t y,int16_t end_x,int16_t end_y){
    if(!file||!file->rpg_runtime)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    const NpeBattleBitmapService service={r->heap,r,rpg_runtime_seek,rpg_runtime_read_local,rpg_runtime_read_guest,rpg_runtime_draw_guest};
    int result=npe_role_animation_stream_open(&service,group,&r->role);
    if(result||r->error)return r->error?-2:result;
    int16_t dx=(int16_t)(uint16_t)((uint16_t)x-(uint16_t)end_x);
    int16_t dy=(int16_t)(uint16_t)((uint16_t)y-(uint16_t)end_y);
    r->role_x=x;r->role_y=y;r->role_position=0;
    if(r->role.frames==1){
        int16_t distance=dx>dy?dx:dy;
        int16_t steps=(int16_t)(distance/16);
        r->role_steps=steps>0?(unsigned)steps:0;
        /* Original4712 reads once, including when the loop has no steps. */
        rpg_runtime_read_guest(r,r->role.address,(uint16_t)r->role.length);
    }else r->role_steps=r->role.frames;
    unsigned denominator=r->role_steps;
    /* Original2F:52FE..530E does not mask the numerator sign out of
       its exponent. A negative numerator therefore takes53A6 and
       returns negative infinity, even with a positive denominator. */
    r->role_dx=denominator?(dx<0?-INFINITY:(float)dx/(float)denominator):0;
    r->role_dy=denominator?(dy<0?-INFINITY:(float)dy/(float)denominator):0;
    return r->error?-2:0;
}
unsigned npe_rpg_runtime_role_steps(const NpeFile *file){
    return file&&file->rpg_runtime&&file->rpg_runtime->role.active?file->rpg_runtime->role_steps:0;
}
/* Source49:75D6 rounds each interpolated float before subtracting it from
   the original16-bit coordinate. Volatile intermediates retain binary32. */
static int16_t role_animation_offset(float slope,unsigned step){
    volatile float product=slope*(float)step;
    /*32:BD0C/BD10 returns0 for an exponent above168, including Inf/NaN. */
    if(!isfinite(product))return 0;
    int16_t truncated=(int16_t)(uint16_t)(int32_t)product;
    int16_t shifted=(int16_t)(uint16_t)(int32_t)((double)product-0.5);
    /* Original32:D218 subtracts the binary64 constant0.5. 49:7650
       compares the low-word conversions and only increments for >0.
       This deliberately retains the original asymmetric rounding. */
    return shifted==truncated && product>0?
        (int16_t)(uint16_t)((uint16_t)truncated+1u):truncated;
}
int npe_rpg_runtime_role_frame(const NpeFile *file,uint8_t kind,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],int16_t position[2]){
    if(!file||!file->rpg_runtime||!pixels)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;
    if(!r->role.active||kind>2||(r->role_steps<255 && r->role_position>=r->role_steps))return -1;
    r->error=0;r->pixels=pixels;
    unsigned step=(uint8_t)(r->role_position+1);
    int16_t x=(int16_t)(uint16_t)((uint16_t)r->role_x-(uint16_t)role_animation_offset(r->role_dx,step));
    int16_t y=(int16_t)(uint16_t)((uint16_t)r->role_y-(uint16_t)role_animation_offset(r->role_dy,step));
    if(position){position[0]=x;position[1]=y;}
    if(r->role.frames!=1)rpg_runtime_read_guest(r,r->role.address,(uint16_t)r->role.length);
    rpg_runtime_draw_guest(r,r->role.address,x,y,kind);
    ++r->role_position;r->pixels=NULL;return r->error?-2:0;
}
void npe_rpg_runtime_role_end(const NpeFile *file){
    if(file&&file->rpg_runtime)npe_animation_stream_close(&file->rpg_runtime->heap,&file->rpg_runtime->role);
}

int npe_rpg_runtime_npc_event(const NpeFile *file,uint16_t id,NpeNpcEvent *event){
    if(!file||!file->rpg_runtime||!event)return -2;
    NpeRpgRuntime *r=file->rpg_runtime;r->error=0;
    uint8_t local[44]={0};
    const NpeNpcEventService service={r->heap,r,rpg_runtime_seek,rpg_runtime_read_local,rpg_runtime_read_guest};
    int result=npe_npc_event_service(&service,id,local,event);
    return r->error?-1:result;
}
