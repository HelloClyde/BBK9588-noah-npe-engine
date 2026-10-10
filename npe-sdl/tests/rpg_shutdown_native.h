/* Original unmodified shutdown slice and heap services. */
typedef struct {const char *name;uint16_t cap;int high_length,no_handles,cleanup;unsigned flags;int zero_map,zero_battle;unsigned repeats;const char *ram,*trace;} RpgResourceCase;
static const RpgResourceCase rpg_resource_cases[]={
{"shutdown-active",65535,0,0,0,422,0,0,1,"tmp/rpg-shutdown-native/shutdown-active-ram.bin","tmp/rpg-shutdown-native/shutdown-active-trace.bin"},
{"shutdown-inactive",65535,0,0,0,166,0,0,1,"tmp/rpg-shutdown-native/shutdown-inactive-ram.bin","tmp/rpg-shutdown-native/shutdown-inactive-trace.bin"},
{"shutdown-two",65535,0,0,0,678,0,0,1,"tmp/rpg-shutdown-native/shutdown-two-ram.bin","tmp/rpg-shutdown-native/shutdown-two-trace.bin"},
{"shutdown-high129",65535,0,0,0,33190,0,0,1,"tmp/rpg-shutdown-native/shutdown-high129-ram.bin","tmp/rpg-shutdown-native/shutdown-high129-trace.bin"},
{"shutdown-high255",65535,0,0,0,65446,0,0,1,"tmp/rpg-shutdown-native/shutdown-high255-ram.bin","tmp/rpg-shutdown-native/shutdown-high255-trace.bin"},
{"shutdown-zero-map",65535,0,0,0,422,1,0,1,"tmp/rpg-shutdown-native/shutdown-zero-map-ram.bin","tmp/rpg-shutdown-native/shutdown-zero-map-trace.bin"},
{"shutdown-zero-battle",65535,0,0,0,422,0,1,1,"tmp/rpg-shutdown-native/shutdown-zero-battle-ram.bin","tmp/rpg-shutdown-native/shutdown-zero-battle-trace.bin"},
{"shutdown-zero-both",65535,0,0,0,422,1,1,1,"tmp/rpg-shutdown-native/shutdown-zero-both-ram.bin","tmp/rpg-shutdown-native/shutdown-zero-both-trace.bin"},
{"shutdown-repeat",65535,0,0,0,422,0,0,2,"tmp/rpg-shutdown-native/shutdown-repeat-ram.bin","tmp/rpg-shutdown-native/shutdown-repeat-trace.bin"},
};
