/* Original entry/flee/party-remove/reentry RAM batches. */
typedef struct {unsigned count;uint8_t ids[3],flags[3],map[3],battle[3];} GroupState;
typedef struct {const char *name;unsigned remove;GroupState entry,exit,removed,reentry;} GroupLifecycleCase;
static const GroupLifecycleCase group_lifecycle_cases[]={
{"one",0,{1,{1,2,3},{1,0,0},{0,2,255},{0,0,0}},{1,{1,2,3},{1,0,0},{1,2,255},{1,0,0}},{1,{1,2,3},{1,0,0},{1,2,255},{1,0,0}},{1,{1,2,3},{1,0,0},{1,2,255},{1,0,0}}},
{"all",1,{3,{1,2,3},{1,1,1},{0,2,255},{0,2,255}},{3,{1,2,3},{1,1,1},{1,255,2},{1,255,2}},{3,{1,3,2},{1,1,0},{1,2,255},{1,255,2}},{2,{1,3,2},{1,1,0},{1,2,255},{1,2,0}}},
{"sparse",0,{3,{3,2,1},{1,0,1},{0,2,255},{0,2,255}},{3,{3,2,1},{1,0,1},{1,255,2},{1,255,2}},{3,{3,2,1},{1,0,1},{1,255,2},{1,255,2}},{3,{3,2,1},{1,0,1},{1,255,2},{1,255,2}}},
};
