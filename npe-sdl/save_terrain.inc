/* Own V23 logical-grid and V24 retained map-buffer extensions. Guest map addresses and host pointers are not serialized.
   The legacy prefix is retained; logical map cells and all192 record bytes
   cover terrain state that the old53-pair projection cannot represent. */
static int save_terrain_shape(const NpeFile *file,const NpeWorld *w,const NpeMap *map,
                              uint8_t *width,uint8_t *height,uint16_t *count,int *active){
    *active=npe_rpg_runtime_owner(file)==w && npe_rpg_runtime_map_current(file,w->map_number)==1;
    if(*active){
        if(npe_rpg_runtime_read(file,0x527a56,width,1)||npe_rpg_runtime_read(file,0x527a5a,height,1))return -1;
    }else if(w->terrain_snapshot_ready){*width=w->terrain_snapshot_width;*height=w->terrain_snapshot_height;}
    else{*width=(uint8_t)map->width;*height=(uint8_t)map->height;}
    uint16_t product=(uint16_t)((unsigned)(*width)*(*height));
    *count=product<0x8000u?product:0;
    return 0;
}
static int save_terrain_cell(const NpeFile *file,const NpeWorld *w,const NpeMap *map,
                             int active,unsigned index,uint16_t *tile){
    if(active)return npe_rpg_runtime_map_tile(file,w->map_number,(uint16_t)index,tile)==1?0:-1;
    if(w->terrain_snapshot_ready){
        unsigned stored=w->terrain_snapshot_count;
        if(w->terrain_buffer_snapshot_ready && stored<NPE_MAP_BUFFER_WORDS)stored=NPE_MAP_BUFFER_WORDS;
        if(index>=stored)return -1;
        *tile=w->terrain_snapshot_cells[index];return 0;
    }
    *tile=npe_world_tile(w,map,index%map->width,index/map->width);return 0;
}
static void save_terrain_word(uint8_t *bytes,unsigned value){bytes[0]=(uint8_t)(value>>8);bytes[1]=(uint8_t)value;}
static int save_map_buffer_needed(const NpeWorld *w,uint8_t width,uint16_t count){
    /*4C:E15A..E27E refreshes105 cells without logical-grid bounds. Keep
      the fixed map-buffer tail when the current viewport reads it. */
    for(unsigned row=0;row<7;++row)for(unsigned column=0;column<15;++column){
        uint16_t index=(uint16_t)((uint8_t)w->view.offset_x+
            ((uint8_t)w->view.offset_y+row)*width+column);
        if(index>=count && index<NPE_MAP_BUFFER_WORDS)return 1;
    }
    return 0;
}
static int save_terrain_extended(const NpeFile *file,const NpeWorld *w){
    NpeMap map;NpeMapTileChange changes[3];unsigned slots;
    if(npe_map(file,w->map_number-1,&map)||npe_map_tile_changes(&map,changes,&slots))return -1;
    uint8_t width,height;uint16_t count;int active;
    if(save_terrain_shape(file,w,&map,&width,&height,&count,&active))return -1;
    if(save_map_buffer_needed(w,width,count)){
        if(!active && !w->terrain_buffer_snapshot_ready)return -1;
        return 2;
    }
    uint8_t expected[192]={0};
    for(unsigned i=0;i<slots;++i){
        uint8_t *r=expected+64u*i;save_terrain_word(r,changes[i].progress);
        for(unsigned j=0;j<changes[i].count;++j){
            save_terrain_word(r+2+2u*j,changes[i].indices[j]);save_terrain_word(r+32+2u*j,changes[i].tiles[j]);
        }
        r[62]=(uint8_t)((w->tile_change_applied>>i)&1u);
    }
    if(memcmp(expected,w->tile_record_bytes,192))return 1;
    if(width!=map.width||height!=map.height)return 1;
    NpeMap projected=map;projected.override_indices=w->tile_override_indices;
    projected.override_tiles=w->tile_override_tiles;projected.override_count=w->tile_override_count;
    for(unsigned i=0;i<count;++i){
        uint16_t tile;if(save_terrain_cell(file,w,&map,active,i,&tile))return -1;
        if(tile!=npe_map_tile(&projected,i%map.width,i/map.width))return 1;
    }
    return 0;
}
static int write_terrain_snapshot(Codec *c,const NpeFile *file,const NpeWorld *w,int buffer){
    NpeMap map;uint8_t width,height;uint16_t count;int active;
    if(npe_map(file,w->map_number-1,&map)||save_terrain_shape(file,w,&map,&width,&height,&count,&active))return -1;
    put(c,width,1);put(c,height,1);put(c,count,2);
    for(unsigned i=0;i<192;++i)put(c,w->tile_record_bytes[i],1);
    unsigned stored=buffer && count<NPE_MAP_BUFFER_WORDS?NPE_MAP_BUFFER_WORDS:count;
    for(unsigned i=0;i<stored;++i){uint16_t tile;if(save_terrain_cell(file,w,&map,active,i,&tile))return -1;put(c,tile,2);}
    return c->bad?-1:0;
}
static void read_terrain_snapshot(Codec *c,NpeWorld *w,int buffer){
    w->terrain_snapshot_width=(uint8_t)get(c,1);w->terrain_snapshot_height=(uint8_t)get(c,1);
    w->terrain_snapshot_count=(uint16_t)get(c,2);
    uint16_t product=(uint16_t)((unsigned)w->terrain_snapshot_width*w->terrain_snapshot_height);
    if(w->terrain_snapshot_count!=(product<0x8000u?product:0)){c->bad=1;return;}
    for(unsigned i=0;i<192;++i)w->tile_record_bytes[i]=(uint8_t)get(c,1);
    unsigned stored=buffer && w->terrain_snapshot_count<NPE_MAP_BUFFER_WORDS?
        NPE_MAP_BUFFER_WORDS:w->terrain_snapshot_count;
    for(unsigned i=0;i<stored;++i)w->terrain_snapshot_cells[i]=(uint16_t)get(c,2);
    w->terrain_buffer_snapshot_ready=(uint8_t)buffer;
    w->terrain_snapshot_ready=1;
}
