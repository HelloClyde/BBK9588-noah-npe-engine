/* Original UI audio requests; one retained-slot restart maps stop+play. */
static const QuizAudioCase quiz_audio_cases[]={
{"fixed-correct-on",13,1,0,2,0,{131,134},{0,0,1}},
{"fixed-wrong-on",13,1,2,1,0,{134},{0,1}},
{"fixed-correct-off",13,0,1,2,0,{131,134},{0,0,0}},
{"fixed-wrong-off",13,0,2,1,0,{134},{0,0}},
{"category-exit-on",0,1,1,1,0,{142},{0,1}},
{"category-third-wrong-on",0,1,2,1,2,{134},{0,1}},
};
