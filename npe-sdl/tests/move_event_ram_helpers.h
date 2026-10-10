/* Test injection uses the attached runtime, leaving NPE file bytes intact. */
static uint64_t move_file_fingerprint;
static void inject_move_ram(const NpeFile *file,const uint8_t bytes[90]){
    assert(!npe_rpg_runtime_write(file,0x527652,bytes,50));
    assert(!npe_rpg_runtime_write(file,0x527728,bytes+50,40));
    uint8_t moving=0;
    for(unsigned i=0;i<20;++i)
        assert(!npe_rpg_runtime_write(file,0x526cad+i*14,&moving,1));
}
static void check_move_ram(const NpeFile *file,const uint8_t expected[90]){
    uint8_t actual[90];
    assert(!npe_rpg_runtime_read(file,0x527652,actual,50));
    assert(!npe_rpg_runtime_read(file,0x527728,actual+50,40));
    assert(!memcmp(actual,expected,90));
    assert(npe_game_fingerprint(file)==move_file_fingerprint);
    assert(!npe_rpg_runtime_failures(file));
}
