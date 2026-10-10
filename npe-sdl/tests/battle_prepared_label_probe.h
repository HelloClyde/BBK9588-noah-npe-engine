/* Twelve prepared bytes only; other named fields are checked by the caller. */
static void prepared_label_seed(NpeWorld *w,const uint8_t expected[6][12],unsigned count){
    for(unsigned slot=0;slot<count;++slot)
        assert(!npe_world_record_write(w,1,slot,102,expected[slot],12));
}
static void prepared_label_check(const NpeWorld *w,const uint8_t expected[6][12],
                                 unsigned count,const char *name,const char *phase){
    for(unsigned slot=0;slot<count;++slot){
        uint8_t r[148];assert(!npe_world_battle_record_image(w,slot,r));
        if(memcmp(r+102,expected[slot],12))
            fprintf(stderr,"%s %s prepared record%u differs\n",name,phase,slot);
        assert(!memcmp(r+102,expected[slot],12));
        if(strcmp(phase,"before")){
            char path[180];snprintf(path,sizeof(path),"tmp/native-%s-%s-record-%u-c.bin",name,phase,slot);
            FILE *out=fopen(path,"wb");assert(out);
            assert(fwrite(r+102,1,12,out)==12&&!fclose(out));
        }
    }
}
