/* Original runtime traces: tmp/battle-native-flee.mjs. Independent
   formation/attribute fixtures; original NPE and PRG unchanged.
   Damage/RNG states are not compared across unsynchronized seeds. */
typedef struct {
    unsigned party, flee_actor, blocked, random, next_actor, quantity;
    unsigned attack_count, attack_order[3];
} NativeFleeCase;
static const NativeFleeCase native_flee_cases[] = {
    {1, 0, 0, 0, 0, 0, 1, {0, 0, 0}}, /* scripted */
    {2, 1, 0, 0, 0, 0, 2, {0, 1, 0}}, /* scripted */
    {3, 2, 0, 0, 0, 0, 3, {0, 1, 2}}, /* scripted */
    {3, 2, 1, 0, 1, 0, 2, {1, 2, 0}}, /* fallen */
    {2, 1, 2, 0, 1, 0, 1, {1, 0, 0}}, /* asleep */
    {2, 1, 0, 1, 0, 0, 2, {0, 1, 0}}, /* random-retry */
};
