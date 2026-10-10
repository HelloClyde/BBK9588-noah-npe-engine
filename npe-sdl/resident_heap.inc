/* Pure C ports of the original resident heap services reached through
   the20:431C..4328 jump vectors. No host allocation or CPU core. */
static uint32_t heap_long(const NpeResidentHeap *heap,uint32_t address){
    return (uint32_t)heap->read_word(heap->context,address)<<16 |
           heap->read_word(heap->context,address+2);
}
static void heap_set_long(const NpeResidentHeap *heap,uint32_t address,uint32_t value){
    heap->write_word(heap->context,address,(uint16_t)(value>>16));
    heap->write_word(heap->context,address+2,(uint16_t)value);
}
void npe_resident_heap_initialize(const NpeResidentHeap *heap){
    if(!heap || !heap->read_word || !heap->write_word)return;
    /*4D:8C1C..8D10: reserve ID0, zero the arena and publish its end. */
    heap->write_word(heap->context,NPE_RESIDENT_HEAP_HEAD,0);
    heap_set_long(heap,NPE_RESIDENT_HEAP_HEAD+2,0);
    for(unsigned i=0;i<16;++i)
        heap->write_word(heap->context,NPE_RESIDENT_HEAP_BITMAP+2*i,i?0:0x8000);
    for(unsigned i=0;i<NPE_RESIDENT_HEAP_BYTES;i+=2)
        heap->write_word(heap->context,NPE_RESIDENT_HEAP_HEAD+i,0);
    heap_set_long(heap,NPE_RESIDENT_HEAP_LIMIT,NPE_RESIDENT_HEAP_HEAD);
    uint32_t limit=heap_long(heap,NPE_RESIDENT_HEAP_LIMIT)+NPE_RESIDENT_HEAP_BYTES;
    /* Original ADD low word followed by ADDC high word. */
    heap->write_word(heap->context,NPE_RESIDENT_HEAP_LIMIT+2,(uint16_t)limit);
    heap->write_word(heap->context,NPE_RESIDENT_HEAP_LIMIT,(uint16_t)(limit>>16));
}
static uint16_t heap_handle(const NpeResidentHeap *heap){
    /*4D:251E..262E scans16 bitmap words from MSB to LSB. */
    for(unsigned word=0;word<16;++word){
        uint32_t address=NPE_RESIDENT_HEAP_BITMAP+2*word;
        uint16_t value=heap->read_word(heap->context,address);
        for(unsigned bit=0;bit<16;++bit){
            uint16_t mask=(uint16_t)(0x8000u>>bit);
            if(!(value&mask)){
                heap->write_word(heap->context,address,(uint16_t)(value|mask));
                return (uint16_t)(16*word+bit);
            }
        }
    }
    return 0;
}
uint16_t npe_resident_heap_allocate(const NpeResidentHeap *heap,uint16_t request){
    if(!heap || !heap->read_word || !heap->write_word)return 0;
    /*42:77F6..781A rounds in a16-bit word, including FFFF->0000. */
    request=(uint16_t)(request+(request&1u));
    uint32_t cursor=NPE_RESIDENT_HEAP_HEAD;
    uint32_t limit=heap_long(heap,NPE_RESIDENT_HEAP_LIMIT);
    while(cursor){
        uint16_t flags=heap->read_word(heap->context,cursor);
        uint32_t next=heap_long(heap,cursor+2);
        if(!flags){
            if(next>limit)break;
            uint16_t available;
            if(!next){
                uint32_t span=limit-cursor-6;
                if(span>NPE_RESIDENT_HEAP_BYTES)break;
                available=(uint16_t)span;
            }else{
                /*42:79F4..79FE discards the high word and reserves
                  the six bytes of both current and split headers. */
                available=(uint16_t)(next-cursor-12);
            }
            if(available<request){
                if(next){cursor=next;continue;}
                break;
            }
            uint16_t handle=heap_handle(heap);
            if(!handle)break;
            heap->write_word(heap->context,cursor,handle);
            if(available>request){
                uint32_t split=cursor+6+request;
                heap_set_long(heap,cursor+2,split);
                heap->write_word(heap->context,split,0);
                heap_set_long(heap,split+2,next);
            }
            return handle;
        }
        if(!next || next>limit)break;
        cursor=next;
    }
    if(heap->allocation_failed)heap->allocation_failed(heap->context,request);
    return 0;
}
uint32_t npe_resident_heap_lock(const NpeResidentHeap *heap,uint16_t handle){
    if(!heap || !heap->read_word || !heap->write_word)return 0;
    uint32_t cursor=NPE_RESIDENT_HEAP_HEAD;
    while(cursor){
        uint16_t flags=heap->read_word(heap->context,cursor);
        if((flags&0x3fffu)==handle){
            /*4E:5364..53B2 also matches handle0 to a free header. */
            heap->write_word(heap->context,cursor,(uint16_t)(flags|0x8000u));
            return cursor+6;
        }
        cursor=heap_long(heap,cursor+2);
    }
    return 0;
}
static void heap_release_handle(const NpeResidentHeap *heap,uint16_t handle){
    /*4E:8ABC..8B68 accepts0..255, including the reserved zero bit. */
    if(handle>255)return;
    uint32_t address=NPE_RESIDENT_HEAP_BITMAP+2*(handle>>4);
    uint16_t mask=(uint16_t)(0x8000u>>(handle&15u));
    heap->write_word(heap->context,address,
        (uint16_t)(heap->read_word(heap->context,address)&(uint16_t)~mask));
}
void npe_resident_heap_compact(const NpeResidentHeap *heap){
    if(!heap || !heap->read_word || !heap->write_word)return;
    /*44:6100..6448: find two exactly-zero headers, then merge them
      or move the intervening unlocked nodes into the first hole. */
    uint32_t cursor=NPE_RESIDENT_HEAP_HEAD;
    for(;;){
        uint32_t first=0,second=0;
        for(uint32_t node=cursor;node;node=heap_long(heap,node+2)){
            if(!heap->read_word(heap->context,node)){
                if(!first)first=node;
                else{second=node;break;}
            }
        }
        if(!first || !second)return;
        int locked=0;
        for(uint32_t node=first;node!=second;node=heap_long(heap,node+2)){
            if(heap->read_word(heap->context,node)&0x8000u){locked=1;break;}
        }
        if(locked){cursor=second;continue;}
        uint32_t source=heap_long(heap,first+2);
        if(source==second){heap_set_long(heap,first+2,heap_long(heap,second+2));continue;}
        /* Original signed32-bit divide by2 keeps the quotient's low word,
           then doubles in a16-bit word before subtracting pointers. */
        uint32_t span=source-first;
        uint32_t half=(span&UINT32_C(0x80000000))?0u-((0u-span)/2u):span/2u;
        uint16_t distance=(uint16_t)((uint16_t)half*2u);
        for(uint32_t node=source;node!=second;){
            uint32_t next=heap_long(heap,node+2);
            heap_set_long(heap,node+2,next-distance);
            node=next;
        }
        /*44:639A tests the current word index, incremented after each
           write, and copies forward up to the second free header. */
        uint16_t index=0;
        while(source+(uint32_t)index*2u!=second){
            uint16_t word=heap->read_word(heap->context,source+(uint32_t)index*2u);
            heap->write_word(heap->context,first+(uint32_t)index*2u,word);
            index=(uint16_t)(index+1u);
        }
        uint32_t merged=second-distance;
        heap->write_word(heap->context,merged,0);
        heap_set_long(heap,merged+2,heap_long(heap,second+2));
        cursor=merged;
    }
}
void npe_resident_heap_unlock(const NpeResidentHeap *heap,uint16_t handle){
    if(!heap || !heap->read_word || !heap->write_word)return;
    /*4D:8B26..8C1A: zero is a valid lookup; absent IDs do nothing. */
    for(uint32_t node=NPE_RESIDENT_HEAP_HEAD;node;node=heap_long(heap,node+2)){
        uint16_t flags=heap->read_word(heap->context,node);
        if((flags&0x3fffu)!=handle)continue;
        heap->write_word(heap->context,node,(uint16_t)(flags&0x7fffu));
        if(heap->read_word(heap->context,node)&0x4000u){
            heap->write_word(heap->context,node,0);
            heap_release_handle(heap,handle);
        }
        npe_resident_heap_compact(heap);
        return;
    }
}
void npe_resident_heap_release(const NpeResidentHeap *heap,uint16_t handle){
    if(!heap || !heap->read_word || !heap->write_word || !handle)return;
    /*4D:3A56..3B60: locked nodes become deferred; free0 is a no-op. */
    for(uint32_t node=NPE_RESIDENT_HEAP_HEAD;node;node=heap_long(heap,node+2)){
        uint16_t flags=heap->read_word(heap->context,node);
        if((flags&0x3fffu)!=handle)continue;
        if(flags&0x8000u)heap->write_word(heap->context,node,(uint16_t)(flags|0x4000u));
        else{heap->write_word(heap->context,node,0);heap_release_handle(heap,handle);}
        npe_resident_heap_compact(heap);
        return;
    }
}
