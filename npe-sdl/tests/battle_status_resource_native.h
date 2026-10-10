/* Original heap services and packed loader; independent caller inputs. */
typedef struct {const char *name;uint16_t cap;int high_length,no_handles,cleanup;unsigned cycles;int released;const char *ram,*trace;} RpgResourceCase;
static const RpgResourceCase rpg_resource_cases[]={
{"cycle-full",65535,0,0,0,1,0,"tmp/battle-status-resource-native/cycle-full-ram.bin","tmp/battle-status-resource-native/cycle-full-trace.bin"},
{"cycle-short",5,0,0,0,2,0,"tmp/battle-status-resource-native/cycle-short-ram.bin","tmp/battle-status-resource-native/cycle-short-trace.bin"},
{"cycle-eof",0,0,0,0,2,0,"tmp/battle-status-resource-native/cycle-eof-ram.bin","tmp/battle-status-resource-native/cycle-eof-trace.bin"},
{"cycle-released",65535,0,0,0,1,1,"tmp/battle-status-resource-native/cycle-released-ram.bin","tmp/battle-status-resource-native/cycle-released-trace.bin"},
{"cycle-zero-release",0,0,1,0,0,1,"tmp/battle-status-resource-native/cycle-zero-release-ram.bin","tmp/battle-status-resource-native/cycle-zero-release-trace.bin"},
};
