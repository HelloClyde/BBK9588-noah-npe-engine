/* NPC records share native RAM with world services. The file uses13 bytes,
   resident slots use14; the last byte is padding retained by the loader. */
static void npc_record_view(NpeWorld *w,unsigned slot){
    const uint8_t *r=w->npc_records[slot];uint8_t *v=w->npc_record_views[slot];
    memcpy(v,r,2);v[2]=r[9];v[3]=r[10];memcpy(v+4,r+2,2);
    v[6]=r[8];v[7]=r[11];memcpy(v+8,r+4,4);v[12]=r[12];
}
int npe_world_load_npc_records(NpeWorld *w,const NpeMap *map){
    if(!w||!map||map->tail_size<340)return -1;
    for(unsigned s=0;s<20;++s){
        const uint8_t *v; if(npe_map_npc(map,s,&v))return -1;
        uint8_t *r=w->npc_records[s];
        memcpy(r,v,2);memcpy(r+2,v+4,2);memcpy(r+4,v+8,4);
        r[8]=v[6];r[9]=v[2];r[10]=v[3];r[11]=v[7];r[12]=v[12];
        npc_record_view(w,s);
        w->npc_override_x[s]=w->npc_override_y[s]=NPE_NPC_POSITION_NONE;
        w->npc_override_direction[s]=255;
    }
    w->npc_records_ready=1;return 0;
}
int npe_world_npc(const NpeWorld *w,const NpeMap *map,unsigned s,const uint8_t **raw){
    if(!w||!raw||s>=20)return -1;
    if(!w->npc_records_ready)return npe_map_npc(map,s,raw);
    *raw=w->npc_record_views[s];return 0;
}
static uint8_t npc_record_byte(const NpeWorld *w,unsigned offset){
    unsigned s=offset/14,o=offset%14;
    if(o==9&&w->npc_override_x[s]!=NPE_NPC_POSITION_NONE)return (uint8_t)w->npc_override_x[s];
    if(o==10&&w->npc_override_y[s]!=NPE_NPC_POSITION_NONE)return (uint8_t)w->npc_override_y[s];
    if(o==8&&w->npc_override_direction[s]!=255)return w->npc_override_direction[s];
    return w->npc_records[s][o];
}
static void npc_record_byte_write(NpeWorld *w,unsigned offset,uint8_t value){
    unsigned s=offset/14,o=offset%14;w->npc_records[s][o]=value;
    if(o==9)w->npc_override_x[s]=value;
    if(o==10)w->npc_override_y[s]=value;
    if(o==8)w->npc_override_direction[s]=value;
    npc_record_view(w,s);w->npc_records_ready=1;
}
