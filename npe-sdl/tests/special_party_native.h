/* Original CPU outputs and the exact diagnostic inputs used to obtain them. */
static void special_party_records(const NpeWorld *w,uint8_t out[444]){
    for(unsigned i=0;i<3;++i)assert(!npe_world_map_record_image(w,i,out+148*i));
}
static void check_special_party_native(const char *dir,const char *name,const NpeWorld *w,int before){
    char path[1024];uint8_t expected[448],actual[444];size_t count=before?444:448;
    snprintf(path,sizeof(path),"%s/%s.%s",dir,name,before?"before":"after");FILE *f=fopen(path,"rb");
    assert(f&&fread(expected,1,count,f)==count&&fgetc(f)==EOF&&!fclose(f));
    special_party_records(w,actual);
    for(unsigned i=0;i<444;++i)if(actual[i]!=expected[i]){
        fprintf(stderr,"native %s record difference %s +%u: %u/%u\n",before?"input":"output",name,i,actual[i],expected[i]);assert(0);}
    if(!before){
        unsigned mode=(unsigned)expected[444]*256+expected[445],event=(unsigned)expected[446]*256+expected[447];
        assert(w->game_mode==mode&&w->story_event_id==event);
    }
}
