/* Original PRG3F:8D52..9244. The formatter sees the guest buffer before
   each unlock/free; byte reads and lookahead have distinct cursor effects. */
static void text_guest_byte_write(const NpeResidentHeap *h,uint32_t a,uint8_t v){
    uint32_t aligned=a&~UINT32_C(1);
    uint16_t word=h->read_word(h->context,aligned);
    h->write_word(h->context,aligned,(uint16_t)((a&1u)?(word&0xff00u)|v:(word&255u)|((unsigned)v<<8)));
}
static void story_text_seek(const NpeTextPageService *s){
    s->seek(s->context,heap_long(&s->heap,UINT32_C(0x52797a))+
            s->heap.read_word(s->heap.context,UINT32_C(0x527a1a)));
}
static void story_text_read(const NpeTextPageService *s,uint8_t *ch){
    s->read_local(s->context,ch,1);
    const NpeResidentHeap *h=&s->heap;
    h->write_word(h->context,UINT32_C(0x527a1a),
        (uint16_t)(h->read_word(h->context,UINT32_C(0x527a1a))+1u));
}
static void story_text_draw(const NpeTextPageService *s,uint32_t buffer,int clear){
    uint16_t rect[4];
    for(unsigned i=0;i<4;++i)rect[i]=s->heap.read_word(s->heap.context,UINT32_C(0x52791e)+2u*i);
    s->draw(s->context,buffer,rect,clear);
}
int npe_story_text_page_service(const NpeTextPageService *s){
    if(!s||!s->heap.read_word||!s->heap.write_word||!s->seek||!s->read_local||!s->draw)return -1;
    const NpeResidentHeap *h=&s->heap;
    if(bitmap_guest_byte(h,UINT32_C(0x527ace))==1&&s->backdrop)s->backdrop(s->context);
    uint16_t handle=npe_resident_heap_allocate(h,64);
    uint32_t buffer=npe_resident_heap_lock(h,handle);
    uint16_t count=0;uint8_t ordinal=0,ch=s->read_seed;
    int first=1;
    story_text_seek(s);
    for(;;){
        story_text_read(s,&ch);
        text_guest_byte_write(h,buffer+(uint32_t)bitmap_signed_word(count),ch);
        if(ch=='@'){
            text_guest_byte_write(h,UINT32_C(0x527a1c),0);
            if(!first)ordinal=3;
            break;
        }
        if(ch=='{'){
            if(first&&bitmap_guest_byte(h,UINT32_C(0x527ace))==0){
                s->read_local(s->context,&ch,1);
                if(ch=='@')text_guest_byte_write(h,UINT32_C(0x527a1c),0);
                break;
            }
            if(first&&bitmap_signed_word(count)>=61)break;
            ++ordinal;
            if(!first){
                s->read_local(s->context,&ch,1);
                if(ch=='@')text_guest_byte_write(h,UINT32_C(0x527a1c),0);
            }
        }else if(!first&&bitmap_signed_word(count)>=61){
            /* The char at61 was consumed, then overwritten by the NUL. */
            ++ordinal;
        }else{
            ++count;continue;
        }
        if(ordinal>=3)break;
        text_guest_byte_write(h,buffer+(uint32_t)bitmap_signed_word(count),0);
        story_text_draw(s,buffer,0);
        npe_resident_heap_unlock(h,handle);npe_resident_heap_release(h,handle);
        h->write_word(h->context,UINT32_C(0x527920),
            (uint16_t)(h->read_word(h->context,UINT32_C(0x527920))+32u));
        handle=npe_resident_heap_allocate(h,64);buffer=npe_resident_heap_lock(h,handle);
        story_text_seek(s);count=0;first=0;
    }
    text_guest_byte_write(h,buffer+(uint32_t)bitmap_signed_word(count),0);
    story_text_draw(s,buffer,bitmap_guest_byte(h,UINT32_C(0x527ace))==0&&count!=0);
    if(bitmap_guest_byte(h,UINT32_C(0x527ace))==1)h->write_word(h->context,UINT32_C(0x527920),12);
    npe_resident_heap_unlock(h,handle);npe_resident_heap_release(h,handle);
    if(s->commit)s->commit(s->context);
    return 0;
}

/* Original NPC controller resets its window on every invocation, including
   ignored inputs. The page service implements its entry/continuation reader. */
int npe_npc_text_page_service(const NpeTextPageService *s,int entry,
                               NpeTextPageLoad load,void *load_context){
    if(!s||!s->heap.read_word||!s->heap.write_word||!s->seek||!s->read_local||!s->draw)return -1;
    const NpeResidentHeap *h=&s->heap;
    static const uint16_t rect[4]={0,84,240,118};
    for(unsigned i=0;i<4;++i)h->write_word(h->context,UINT32_C(0x52791e)+2u*i,rect[i]);
    uint16_t handle=npe_resident_heap_allocate(h,64);
    uint32_t buffer=npe_resident_heap_lock(h,handle);
    uint8_t ch=0;uint16_t count=0;
    /*3B:7166 initializes the local byte on every call, including continuation. */
    if(entry){
        if(load&&load(load_context)!=1){
            npe_resident_heap_unlock(h,handle);npe_resident_heap_release(h,handle);return -1;
        }
        s->seek(s->context,heap_long(h,UINT32_C(0x52797a)));
        h->write_word(h->context,UINT32_C(0x527a1a),0);
        text_guest_byte_write(h,UINT32_C(0x527a1c),1);
        story_text_read(s,&ch);
    }else story_text_seek(s);
    for(;;){
        story_text_read(s,&ch);
        text_guest_byte_write(h,buffer+(uint32_t)bitmap_signed_word(count),ch);
        if(ch=='@'){text_guest_byte_write(h,UINT32_C(0x527a1c),0);break;}
        if(ch=='{'){
            s->read_local(s->context,&ch,1);
            if(ch=='@')text_guest_byte_write(h,UINT32_C(0x527a1c),0);
            break;
        }
        if(bitmap_signed_word(count)>=61)break;
        ++count;
    }
    text_guest_byte_write(h,buffer+(uint32_t)bitmap_signed_word(count),0);
    /* Empty entry skips clear and draw; continuation still calls the formatter. */
    if(count||!entry)story_text_draw(s,buffer,count!=0);
    npe_resident_heap_unlock(h,handle);npe_resident_heap_release(h,handle);
    if(entry)h->write_word(h->context,UINT32_C(0x527ab4),0);
    if(s->commit)s->commit(s->context);
    return 0;
}
