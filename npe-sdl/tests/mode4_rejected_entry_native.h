/* Diagnostic inputs sampled from the unchanged original engine. */
typedef struct {const char *name;unsigned context,ui,kind;} Mode4RejectedCase;
static const Mode4RejectedCase mode4_rejected_cases[]={
    {"cold-zero",0,0,0},
    {"cold-nine",9,0,0},
    {"signed-negative",65535,0,2},
    {"retained-list",0,1,2},
    {"retained-quantity",0,2,1},
};
static const unsigned mode4_rejected_view[]={
    1,1,7,1,36,1,1,
};
static const unsigned mode4_rejected_npc[20][3]={
    {32,13,3},
    {5,8,0},
    {1,9,4},
    {50,3,4},
    {22,23,0},
    {17,23,1},
    {40,7,0},
    {31,26,3},
    {7,10,3},
    {4,17,2},
    {47,18,0},
    {24,10,0},
    {34,19,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
};
