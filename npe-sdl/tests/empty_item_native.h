/* Original native empty-item fixtures: tmp/battle-native-empty-item.mjs.
   Independent formation/inventory/attribute samples, not story saves. */
typedef struct { const char *variant, *label;
    unsigned party, occurrence, actor, mode, name; } NativeEmptyPhase;
static const NativeEmptyPhase native_empty_phases[] = {
    {"cancel", "entry", 2, 0, 0, 3, 49853},
    {"cancel", "empty-confirm", 2, 0, 0, 3, 49852},
    {"cancel", "cancel", 2, 0, 0, 3, 49853},
    {"cancel", "empty-again", 2, 0, 0, 3, 49852},
    {"cancel", "cancel-again", 2, 0, 0, 3, 49853},
    {"back", "entry", 2, 0, 0, 3, 49853},
    {"back", "empty-confirm", 2, 0, 1, 3, 49852},
    {"back", "previous", 2, 0, 0, 3, 49853},
    {"use", "entry", 2, 0, 0, 3, 49853},
    {"use", "empty-confirm", 2, 0, 0, 3, 49853},
    {"victory", "entry", 2, 0, 0, 3, 49853},
    {"victory", "empty-confirm", 2, 0, 1, 3, 49852},
    {"victory", "settled", 2, 0, 0, 3, 49852},
    {"victory", "battle-exited", 2, 0, 0, 5, 49853},
    {"success-flee", "entry", 2, 0, 0, 3, 49853},
    {"success-flee", "empty-confirm", 2, 0, 1, 3, 49852},
    {"success-flee", "settled", 2, 0, 0, 3, 49852},
    {"success-flee", "flee-result", 2, 0, 0, 1, 49853},
    {"loss", "entry", 2, 0, 0, 3, 49853},
    {"loss", "empty-confirm", 2, 0, 1, 3, 49852},
    {"loss", "settled", 2, 0, 0, 3, 49852},
    {"loss", "loss-result", 2, 0, 0, 7, 49853},
    {"three", "entry", 3, 0, 0, 3, 49853},
    {"three", "empty-confirm", 3, 0, 1, 3, 49852},
    {"three", "empty-next", 3, 0, 2, 3, 49851},
    {"three", "empty-next", 3, 1, 0, 3, 49850},
    {"three", "settled", 3, 0, 0, 3, 49850},
    {"failed-flee", "entry", 2, 0, 0, 3, 49853},
    {"failed-flee", "empty-confirm", 2, 0, 1, 3, 49852},
    {"failed-flee", "settled", 2, 0, 0, 3, 49852},
    {"group", "entry", 2, 0, 0, 3, 49853},
    {"group", "empty-confirm", 2, 0, 1, 3, 49852},
    {"group", "settled", 2, 0, 0, 3, 49852},
    {"skip", "entry", 2, 0, 0, 3, 49853},
    {"skip", "empty-confirm", 2, 0, 1, 3, 49852},
    {"skip", "settled", 2, 0, 1, 3, 49852},
};
static const unsigned native_empty_item[] = {
    255, 0, 192, 0, 0, 3, 55808, 4
};
static const unsigned native_empty_target_qi[] = {60, 60};
