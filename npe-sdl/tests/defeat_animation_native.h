#ifndef DEFEAT_ANIMATION_NATIVE_H
#define DEFEAT_ANIMATION_NATIVE_H
#include "negative_hp_native.h"
typedef struct { const char *name; unsigned party,character,scripted,frames,seed,party_actions,fast,enemy,final_actor,direct,icon; unsigned actions[3]; NegativeHpRecord before[6],after[6]; } DefeatAnimationCase;
static const DefeatAnimationCase defeat_animation_cases[]={
{"defeat-animation-ordinary",1,1,0,13,4,1,0,1,0,1,2,{5,0,0},{{1,30000,500,1,8,0,99,0,0,0,{0,0,0,0}},{0,30000,0,1,300,0,10,0,0,0,{0,0,0,0}},{30000,30000,0,1,300,3,10,45,0,0,{0,0,0,0}},{0,30000,0,1,300,90,10,0,0,0,{0,0,0,0}}},{{0,30000,500,1,8,100,99,0,0,2,{0,0,0,0}},{0,30000,0,1,300,0,10,0,0,0,{0,0,0,0}},{30000,30000,0,1,300,3,10,45,0,0,{0,0,0,0}},{0,30000,0,1,300,90,10,0,0,0,{0,0,0,0}}}},
{"defeat-animation-scripted",1,2,1,13,4,1,0,1,0,1,2,{5,0,0},{{1,30000,500,1,8,0,99,0,0,0,{0,0,0,0}},{0,30000,0,1,300,0,10,0,0,0,{0,0,0,0}},{30000,30000,0,1,300,3,10,45,0,0,{0,0,0,0}},{0,30000,0,1,300,90,10,0,0,0,{0,0,0,0}}},{{0,30000,500,1,8,100,99,0,0,2,{0,0,0,0}},{0,30000,0,1,300,0,10,0,0,0,{0,0,0,0}},{30000,30000,0,1,300,3,10,45,0,0,{0,0,0,0}},{0,30000,0,1,300,90,10,0,0,0,{0,0,0,0}}}},
};
#endif
