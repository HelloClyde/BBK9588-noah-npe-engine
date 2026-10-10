/* The numeric public World API also uses the same C loader. Without the
   persistent resource runtime, unbound resident bytes are a temporary RAM
   backing; production SDL uses its persistent full RAM and file adapters. */
typedef struct{
    const NpeFile *file;NpeWorld *world;uint32_t cursor;uint8_t ram[131072];int error;
}WorldEnemyIo;
static uint8_t world_enemy_byte(WorldEnemyIo *io,uint32_t a){
    uint8_t b=0;
    if(a<0x510000||a>=0x530000){io->error=1;return 0;}
    if(npe_world_resident_read(io->world,a,&b,1))b=io->ram[a-0x510000];
    return b;
}
static void world_enemy_byte_write(WorldEnemyIo *io,uint32_t a,uint8_t b){
    if(a<0x510000||a>=0x530000){io->error=1;return;}
    io->ram[a-0x510000]=b;npe_world_resident_write(io->world,a,&b,1);
}
static uint16_t world_enemy_word(void *context,uint32_t a){
    WorldEnemyIo *io=context;return (uint16_t)((unsigned)world_enemy_byte(io,a)*256+world_enemy_byte(io,a+1));
}
static void world_enemy_word_write(void *context,uint32_t a,uint16_t v){
    world_enemy_byte_write(context,a,(uint8_t)(v>>8));world_enemy_byte_write(context,a+1,(uint8_t)v);
}
static void world_enemy_seek(void *context,uint32_t p){
    WorldEnemyIo *io=context;if(p<=io->file->size)io->cursor=p;
}
static unsigned world_enemy_available(WorldEnemyIo *io,unsigned n){
    if(io->cursor>=io->file->size)return 0;
    size_t available=io->file->size-io->cursor;return n<available?n:(unsigned)available;
}
static void world_enemy_read_local(void *context,uint8_t *dst,uint16_t n){
    WorldEnemyIo *io=context;unsigned supplied=world_enemy_available(io,n);
    if(supplied)memcpy(dst,io->file->bytes+io->cursor,supplied);
    io->cursor+=supplied;
}
static void world_enemy_read_guest(void *context,uint32_t dst,uint16_t n){
    WorldEnemyIo *io=context;unsigned supplied=world_enemy_available(io,n);
    for(unsigned i=0;i<supplied;++i)world_enemy_byte_write(io,dst+i,io->file->bytes[io->cursor+i]);
    io->cursor+=supplied;
}
static int world_load_party_battle_descriptors(const NpeFile *file,NpeWorld *w,int phase){
    unsigned party=world_record_party_count(w);
    if(file->rpg_runtime){
        if(npe_rpg_runtime_owner(file)!=w)return -1;
        return npe_rpg_runtime_party_descriptors(file,party,phase);
    }
    WorldEnemyIo *io=calloc(1,sizeof(*io));if(!io)return -1;
    io->file=file;io->world=w;
    NpeResidentHeap h={io,world_enemy_word,world_enemy_word_write,NULL};
    world_enemy_byte_write(io,UINT32_C(0x527afa),(uint8_t)party);
    const unsigned sections[]={36,33};
    const uint32_t lengths[]={UINT32_C(0x527942),UINT32_C(0x527962)};
    for(unsigned i=0;i<2;++i)if(file->section_count>sections[i]){
        uint32_t size=file->sections[sections[i]].size,offset=file->sections[sections[i]].offset;
        world_enemy_word_write(io,lengths[i],(uint16_t)(size>>16));
        world_enemy_word_write(io,lengths[i]+2,(uint16_t)size);
        world_enemy_word_write(io,lengths[i]+4,(uint16_t)(offset>>16));
        world_enemy_word_write(io,lengths[i]+6,(uint16_t)offset);
    }
    int result=0;
    if(!phase){
        result=npe_party_battle_positions_initialize(&h);
        if(!result)result=npe_party_battle_place(&h);
    }else result=npe_party_battle_audio(&h);
    if(io->error)result=-2;
    free(io);return result;
}
int npe_world_load_battle_enemies(const NpeFile *file,NpeWorld *w){
    if(!file||!w||file->section_count<=4)return -1;
    unsigned party=world_record_party_count(w);int result;
    if(file->rpg_runtime){
        if(npe_rpg_runtime_owner(file)!=w)return -1;
        result=npe_rpg_runtime_load_enemies(file,party);
    }else{
        WorldEnemyIo *io=calloc(1,sizeof(*io));if(!io)return -1;io->file=file;io->world=w;
        NpeBattleBitmapService s={{io,world_enemy_word,world_enemy_word_write,NULL},io,
                                 world_enemy_seek,world_enemy_read_local,world_enemy_read_guest,NULL};
        uint32_t base=file->sections[4].offset;
        world_enemy_word_write(io,0x5279b6,(uint16_t)(base>>16));world_enemy_word_write(io,0x5279b8,(uint16_t)base);
        world_enemy_byte_write(io,0x527afa,(uint8_t)party);
        result=npe_enemy_record_service(&s);if(io->error)result=-2;free(io);
    }
    if(result)return result;
    /* Preserve these diagnostic lookup keys for old callers. Neither the
       native byte12 nor drawing is derived from this truncated host key. */
    for(unsigned e=0;e<w->battle_formation_count&&e<3;++e){
        if(w->battle_scripted_entry==1?(int16_t)w->battle_selected_ids[e]>0:w->battle_selected_ids[e]!=0)
            w->battle_enemy_ids[e]=(uint8_t)w->battle_selected_ids[e];
        w->battle_enemy_last_skill[e]=w->battle_enemy_last_target[e]=0;
    }
    w->battle_enemy_records_loaded=1;
    return 0;
}
