/* Independent original guest service samples after explicit heap reset.
   Instrumented diagnostics, not unmodified game or full SDL traces. */
typedef struct {uint16_t request,handle;uint32_t pointer;uint16_t flags;
 uint32_t next;uint16_t bitmap;} ResidentHeapNativeCase;
static const ResidentHeapNativeCase resident_heap_native_cases[]={
 {3,1,UINT32_C(0x514a46),0x8001,UINT32_C(0x514a4a),0xc000},
 {35329,1,UINT32_C(0x514a46),0x8001,UINT32_C(0x51d448),0xc000},
 {65535,1,UINT32_C(0x514a46),0x8001,UINT32_C(0x514a46),0xc000},
};
typedef struct {uint16_t request,flags,bitmap;uint32_t limit,pointer;} ResidentHeapTailCase;
static const ResidentHeapTailCase resident_heap_tail_cases[]={
 {3,0x8000,0xfe00,UINT32_C(0x523a40),UINT32_C(0x51e9c2)},
 {35329,0x8000,0xfe00,UINT32_C(0x523a40),UINT32_C(0x51e9c2)},
};
