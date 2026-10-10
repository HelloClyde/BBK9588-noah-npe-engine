static void battle_sound_file(const char *path,NpeFile *file){
    FILE *input=fopen(path,"rb");assert(input && !fseek(input,0,SEEK_END));
    long size=ftell(input);assert(size>0 && !fseek(input,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);assert(bytes && fread(bytes,1,(size_t)size,input)==(size_t)size);
    assert(!fclose(input));char error[256];assert(!npe_parse(file,bytes,(size_t)size,error,sizeof(error)));
}
