/* Original loader/heap snapshots; RAM IO and drawing are diagnostic boundaries. */
typedef struct {uint32_t address;uint16_t flags;uint32_t next;} BitmapHeapNode;
static const BitmapHeapNode bitmap_heap_nodes[]={
 {UINT32_C(0x514a40),0x8001,UINT32_C(0x514b46)},
 {UINT32_C(0x514b46),0x8002,UINT32_C(0x514c4c)},
 {UINT32_C(0x514c4c),0x8003,UINT32_C(0x514d52)},
 {UINT32_C(0x514d52),0x8004,UINT32_C(0x514e58)},
 {UINT32_C(0x514e58),0x8005,UINT32_C(0x5169b6)},
 {UINT32_C(0x5169b6),0x8006,UINT32_C(0x51e9bc)},
 {UINT32_C(0x51e9bc),0x8000,UINT32_C(0x0)},
};
typedef struct {const char *name;uint16_t number;int warm;uint8_t kind;
 uint16_t first_cap,second_cap,payload_cap;int draw_handle;
 const char *ram_file,*trace_file;} BitmapServiceCase;
static const BitmapServiceCase bitmap_service_cases[]={
 {"fresh-514",514,0,0,3,3,65535,-1,"tmp/battle-bitmap-service-native/fresh-514-ram.bin","tmp/battle-bitmap-service-native/fresh-514-trace.bin"},
 {"fresh-30",30,0,1,3,3,65535,-1,"tmp/battle-bitmap-service-native/fresh-30-ram.bin","tmp/battle-bitmap-service-native/fresh-30-trace.bin"},
 {"warm-34",34,1,2,3,3,65535,-1,"tmp/battle-bitmap-service-native/warm-34-ram.bin","tmp/battle-bitmap-service-native/warm-34-trace.bin"},
 {"warm-31",31,1,255,3,3,65535,-1,"tmp/battle-bitmap-service-native/warm-31-ram.bin","tmp/battle-bitmap-service-native/warm-31-trace.bin"},
 {"short-payload",514,0,0,3,3,5,-1,"tmp/battle-bitmap-service-native/short-payload-ram.bin","tmp/battle-bitmap-service-native/short-payload-trace.bin"},
 {"short-first-directory",514,0,1,1,3,65535,-1,"tmp/battle-bitmap-service-native/short-first-directory-ram.bin","tmp/battle-bitmap-service-native/short-first-directory-trace.bin"},
 {"zero-second-directory",514,0,2,3,0,65535,-1,"tmp/battle-bitmap-service-native/zero-second-directory-ram.bin","tmp/battle-bitmap-service-native/zero-second-directory-trace.bin"},
 {"draw-global-handle",514,0,0,3,3,65535,255,"tmp/battle-bitmap-service-native/draw-global-handle-ram.bin","tmp/battle-bitmap-service-native/draw-global-handle-trace.bin"},
 {"resident-29",29,1,0,3,3,65535,-1,"tmp/battle-bitmap-service-native/resident-29-ram.bin","tmp/battle-bitmap-service-native/resident-29-trace.bin"},
 {"resident-0",0,1,1,3,3,65535,-1,"tmp/battle-bitmap-service-native/resident-0-ram.bin","tmp/battle-bitmap-service-native/resident-0-trace.bin"},
 {"resident-1",1,1,2,3,3,65535,-1,"tmp/battle-bitmap-service-native/resident-1-ram.bin","tmp/battle-bitmap-service-native/resident-1-trace.bin"},
 {"file-displacement-wrap",10952,1,0,3,3,65535,-1,"tmp/battle-bitmap-service-native/file-displacement-wrap-ram.bin","tmp/battle-bitmap-service-native/file-displacement-wrap-trace.bin"},
 {"file-negative-offset",512,1,1,3,3,65535,-1,"tmp/battle-bitmap-service-native/file-negative-offset-ram.bin","tmp/battle-bitmap-service-native/file-negative-offset-trace.bin"},
};
