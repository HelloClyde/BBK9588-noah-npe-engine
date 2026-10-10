/* Independent original owner-helper and launcher-prefix RAM snapshots. */
typedef struct {const char *name;uint32_t request;int prefix,no_handles,no_space;
 const char *ram,*trace;} HeapOwnerCase;
static const HeapOwnerCase heap_owner_cases[]={
 {"launcher-four",UINT32_C(256),1,0,0,"tmp/resident-heap-owner-native/launcher-four-ram.bin","tmp/resident-heap-owner-native/launcher-four-trace.bin"},
 {"owned-high-size",UINT32_C(16777472),0,0,0,"tmp/resident-heap-owner-native/owned-high-size-ram.bin","tmp/resident-heap-owner-native/owned-high-size-trace.bin"},
 {"owned-zero",UINT32_C(0),0,0,0,"tmp/resident-heap-owner-native/owned-zero-ram.bin","tmp/resident-heap-owner-native/owned-zero-trace.bin"},
 {"owned-wrap",UINT32_C(65535),0,0,0,"tmp/resident-heap-owner-native/owned-wrap-ram.bin","tmp/resident-heap-owner-native/owned-wrap-trace.bin"},
 {"owned-no-handles",UINT32_C(256),0,1,0,"tmp/resident-heap-owner-native/owned-no-handles-ram.bin","tmp/resident-heap-owner-native/owned-no-handles-trace.bin"},
 {"owned-no-space",UINT32_C(256),0,0,1,"tmp/resident-heap-owner-native/owned-no-space-ram.bin","tmp/resident-heap-owner-native/owned-no-space-trace.bin"},
};
