/* Shared original map formations at761A and moving chase slots at783A. */
static uint16_t encounter_word(const uint8_t *p){return (uint16_t)((unsigned)p[0]<<8|p[1]);}
static void monster_record_view(NpeWorld *w,unsigned s){
    const uint8_t *r=w->monster_records[s];uint8_t *v=w->monster_record_views[s];
    v[0]=r[11];v[1]=r[9];v[2]=r[10];memcpy(v+3,r+2,4);
    memcpy(v+7,r,2);memcpy(v+9,r+6,2);v[11]=r[12];v[12]=r[13];
}
static void chase_record_view(NpeWorld *w,unsigned s){
    memcpy(w->chase_record_views[s],w->chase_records[s],8);
    w->chase_record_views[s][8]=(uint8_t)w->chase_x[s];
    w->chase_record_views[s][9]=(uint8_t)w->chase_y[s];
}
int npe_world_load_map_encounter_records(NpeWorld *w,const NpeMap *map){
    if(!w||!map||map->tail_size<644)return -1;
    /* Validate the variable group before mutating any owned records. */
    NpeMapChase rows[2];int present[2];
    for(unsigned s=0;s<2;++s){present[s]=npe_map_chase(map,s,&rows[s]);if(present[s]<0)return -1;}
    for(unsigned s=0;s<4;++s){
        NpeMapMonster m;if(npe_map_monster(map,s,&m))return -1;
        uint8_t *r=w->monster_records[s];
        record_word(r,0,m.graphic);record_word(r,2,m.min_progress);
        record_word(r,4,m.max_progress);record_word(r,6,m.npc_id);
        r[9]=m.x;r[10]=m.y;r[11]=m.enemy_id;r[12]=m.left_enemy_id;r[13]=m.right_enemy_id;
        monster_record_view(w,s); /* byte8 remains untouched. */
    }
    for(unsigned s=0;s<2;++s){
        uint8_t *r=w->chase_records[s];record_word(r,6,0);
        if(present[s]){
            const NpeMapChase *c=&rows[s];record_word(r,0,c->graphic);
            record_word(r,2,c->min_progress);record_word(r,4,c->max_progress);
            record_word(r,6,c->enemy_id);record_word(r,8,c->x);record_word(r,10,c->y);
            w->chase_x[s]=c->x;w->chase_y[s]=c->y;r[12]=w->chase_cooldown[s]=0;
        }
        chase_record_view(w,s);
    }
    w->map_encounter_records_ready=1;return 0;
}
int npe_world_monster(const NpeWorld *w,const NpeMap *map,unsigned s,NpeMapMonster *m){
    if(!w||!m||s>=4)return -1;
    if(!w->map_encounter_records_ready)return npe_map_monster(map,s,m);
    const uint8_t *r=w->monster_records[s];
    m->graphic=encounter_word(r);m->min_progress=encounter_word(r+2);
    m->max_progress=encounter_word(r+4);m->npc_id=encounter_word(r+6);
    m->x=r[9];m->y=r[10];m->enemy_id=r[11];m->left_enemy_id=r[12];m->right_enemy_id=r[13];
    m->raw=w->monster_record_views[s];return 0;
}
int npe_world_chase(const NpeWorld *w,const NpeMap *map,unsigned s,NpeMapChase *c){
    if(!w||!c||s>=2)return -1;
    if(!w->map_encounter_records_ready)return npe_map_chase(map,s,c);
    const uint8_t *r=w->chase_records[s];
    c->graphic=encounter_word(r);c->min_progress=encounter_word(r+2);
    c->max_progress=encounter_word(r+4);c->enemy_id=encounter_word(r+6);
    c->x=(uint8_t)w->chase_x[s];c->y=(uint8_t)w->chase_y[s];c->raw=w->chase_record_views[s];return 1;
}
static uint8_t chase_record_byte(const NpeWorld *w,unsigned offset){
    unsigned s=offset/14,o=offset%14;
    if(o==8||o==9)return (uint8_t)(o==8?w->chase_x[s]>>8:w->chase_x[s]);
    if(o==10||o==11)return (uint8_t)(o==10?w->chase_y[s]>>8:w->chase_y[s]);
    if(o==12)return w->chase_cooldown[s];
    return w->chase_records[s][o];
}
static void chase_record_byte_write(NpeWorld *w,unsigned offset,uint8_t value){
    unsigned s=offset/14,o=offset%14;w->chase_records[s][o]=value;
    if(o==8)w->chase_x[s]=(uint16_t)((w->chase_x[s]&255u)|((unsigned)value<<8));
    if(o==9)w->chase_x[s]=(uint16_t)((w->chase_x[s]&0xff00u)|value);
    if(o==10)w->chase_y[s]=(uint16_t)((w->chase_y[s]&255u)|((unsigned)value<<8));
    if(o==11)w->chase_y[s]=(uint16_t)((w->chase_y[s]&0xff00u)|value);
    if(o==12)w->chase_cooldown[s]=value;
    chase_record_view(w,s);w->map_encounter_records_ready=1;
}
