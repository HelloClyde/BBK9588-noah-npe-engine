/* Unmodified original ending dispatcher branch and heap allocations. */
typedef struct {const char *name;uint16_t cap;int high_length,no_handles,cleanup;unsigned flags;int zero_map,zero_battle;unsigned repeats;int reallocate;const char *ram,*trace;} RpgResourceCase;
static const RpgResourceCase rpg_resource_cases[]={
{"ending-active",65535,0,0,0,422,0,0,1,0,"tmp/ending-return/native/ending-active-ram.bin","tmp/ending-return/native/ending-active-trace.bin"},
{"ending-inactive",65535,0,0,0,166,0,0,1,0,"tmp/ending-return/native/ending-inactive-ram.bin","tmp/ending-return/native/ending-inactive-trace.bin"},
{"ending-two",65535,0,0,0,678,0,0,1,0,"tmp/ending-return/native/ending-two-ram.bin","tmp/ending-return/native/ending-two-trace.bin"},
{"ending-zero-map",65535,0,0,0,422,1,0,1,0,"tmp/ending-return/native/ending-zero-map-ram.bin","tmp/ending-return/native/ending-zero-map-trace.bin"},
{"ending-repeat",65535,0,0,0,422,0,0,2,0,"tmp/ending-return/native/ending-repeat-ram.bin","tmp/ending-return/native/ending-repeat-trace.bin"},
{"ending-reallocate",65535,0,0,0,422,0,0,1,1,"tmp/ending-return/native/ending-reallocate-ram.bin","tmp/ending-return/native/ending-reallocate-trace.bin"},
};
