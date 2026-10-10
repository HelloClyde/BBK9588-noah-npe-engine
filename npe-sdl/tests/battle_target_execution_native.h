/* Original firmware public-input samples, six isolated target cases. */
#ifndef BATTLE_TARGET_EXECUTION_NATIVE_H
#define BATTLE_TARGET_EXECUTION_NATIVE_H
typedef struct { const char *name; unsigned side,hp,phase_count;
unsigned menus[5]; uint8_t targets[5][3],execution[3],post_execution[3]; } NativeTargetCase;
static const NativeTargetCase native_target_cases[] = {
{"enemy-0",0,0,4,{10,10,10,30},{{4,4,4},{5,5,4},{5,5,4},{4,4,4}},{4,0,4},{4,0,4}},
{"friend-0",1,0,5,{10,20,30,30,30},{{4,4,4},{2,4,4},{2,0,4},{2,0,4},{4,4,4}},{3,0,0},{3,0,0}},
{"enemy-65535",0,65535,4,{10,10,10,30},{{4,4,4},{5,5,4},{5,5,4},{4,4,4}},{5,0,4},{4,0,4}},
{"friend-65535",1,65535,5,{10,20,30,30,30},{{4,4,4},{2,4,4},{2,0,4},{2,0,4},{4,4,4}},{2,0,0},{2,0,0}},
{"enemy-32768",0,32768,4,{10,10,10,30},{{4,4,4},{5,5,4},{5,5,4},{4,4,4}},{5,0,4},{5,0,4}},
{"friend-32768",1,32768,5,{10,20,30,30,30},{{4,4,4},{2,4,4},{2,0,4},{2,0,4},{4,4,4}},{2,0,0},{2,0,0}},
};
#endif
