/* Ordinary inputs in the unmodified original title and slot chooser. */
typedef struct {unsigned mode;unsigned event;unsigned init;unsigned slot;unsigned selection;unsigned stage;unsigned map;unsigned progress;} TitleSlotState;
typedef struct {const char *label;unsigned mask,frames;TitleSlotState after;} TitleSlotPhase;
static const TitleSlotPhase title_slot_cancel[]={
{"down",2,1,{7,0,0,1,2,1,0,1}},
{"open",16,1,{7,0,0,1,2,2,0,1}},
{"slot2",2,1,{7,0,0,2,2,2,0,1}},
{"slot3",2,1,{7,0,0,3,2,2,0,1}},
{"wrap-down",2,1,{7,0,0,1,2,2,0,1}},
{"wrap-up",1,1,{7,0,0,3,2,2,0,1}},
{"up2",1,1,{7,0,0,2,2,2,0,1}},
{"up1",1,1,{7,0,0,1,2,2,0,1}},
{"cancel",64,2,{7,0,0,1,2,1,0,1}},
{"exit-row",2,1,{7,0,0,1,3,1,0,1}},
{"read-row",1,1,{7,0,0,1,2,1,0,1}},
{"reopen",16,1,{7,0,0,1,2,2,0,1}},
{"slot3-again",1,1,{7,0,0,3,2,2,0,1}},
{"cancel-again",64,2,{7,0,0,1,2,1,0,1}},
};
static const TitleSlotPhase title_slot_empty[]={
{"down",2,1,{7,0,0,1,2,1,0,1}},
{"open",16,1,{7,0,0,1,2,2,0,1}},
{"slot3",1,1,{7,0,0,3,2,2,0,1}},
{"new-from-empty",16,3,{1,0,0,3,2,2,1,1}},
};
typedef struct {const char *name;unsigned count;const TitleSlotPhase *phases;} TitleSlotCase;
static const TitleSlotCase title_slot_cases[]={
{"cancel",14,title_slot_cancel},
{"empty",4,title_slot_empty},
};
