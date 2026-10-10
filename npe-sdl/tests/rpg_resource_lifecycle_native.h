/* Unmodified original allocation/IO/cleanup results, no header seeds. */
typedef struct {const char *name;uint16_t cap;int high_length,no_handles,cleanup;
 const char *ram,*trace;} RpgResourceCase;
static const RpgResourceCase rpg_resource_cases[]={
 {"resources-full",65535,0,0,0,"tmp/rpg-resource-lifecycle-native/resources-full-ram.bin","tmp/rpg-resource-lifecycle-native/resources-full-trace.bin"},
 {"resources-short",5,0,0,0,"tmp/rpg-resource-lifecycle-native/resources-short-ram.bin","tmp/rpg-resource-lifecycle-native/resources-short-trace.bin"},
 {"resources-eof",0,0,0,0,"tmp/rpg-resource-lifecycle-native/resources-eof-ram.bin","tmp/rpg-resource-lifecycle-native/resources-eof-trace.bin"},
 {"resources-high-length",65535,1,0,0,"tmp/rpg-resource-lifecycle-native/resources-high-length-ram.bin","tmp/rpg-resource-lifecycle-native/resources-high-length-trace.bin"},
 {"resources-allocation-failed",65535,0,1,0,"tmp/rpg-resource-lifecycle-native/resources-allocation-failed-ram.bin","tmp/rpg-resource-lifecycle-native/resources-allocation-failed-trace.bin"},
 {"resources-launcher-cleanup",65535,0,0,1,"tmp/rpg-resource-lifecycle-native/resources-launcher-cleanup-ram.bin","tmp/rpg-resource-lifecycle-native/resources-launcher-cleanup-trace.bin"},
 {"resources-zero-cleanup",65535,0,0,2,"tmp/rpg-resource-lifecycle-native/resources-zero-cleanup-ram.bin","tmp/rpg-resource-lifecycle-native/resources-zero-cleanup-trace.bin"},
};
