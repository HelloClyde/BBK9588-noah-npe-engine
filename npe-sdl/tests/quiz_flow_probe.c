#include "../npe.h"
#include "../text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {unsigned key,commits,state[7];const char *input;unsigned stats[22];} QuizStage;
typedef struct {const char *name;unsigned event,count,fixed_seed,stages;QuizStage stage[48];} QuizCase;
#include "quiz_native.h"
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long size=ftell(f);assert(size>0&&!fseek(f,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,f)==(size_t)size&&!fclose(f));
    NpeFile file;char error[128];NpeFont font={0};
    assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    assert(!npe_font_load(&font,"npe-sdl/assets/gbk12-native.bin"));
    assert(!npe_font_load16(&font,"npe-sdl/assets/gbk16-native.bin"));
    unsigned frames=0,ignored=0;
    for(unsigned n=0;n<sizeof(quiz_cases)/sizeof(quiz_cases[0]);++n) {
        const QuizCase *c=&quiz_cases[n];NpeWorld w;NpeQuizQuestion q;
        assert(!npe_world_new_game(&file,&w));w.game_mode=8;w.story_event_id=(uint16_t)c->event;
        w.quiz_answer_count=(uint16_t)c->count;w.quiz_fixed_correct=(uint16_t)c->fixed_seed;
        for(unsigned i=0;i<10;++i){w.quiz_category_correct[i]=(uint16_t)c->stage[0].stats[2*i];w.quiz_category_wrong[i]=(uint16_t)c->stage[0].stats[2*i+1];}
        w.quiz_fixed_wrong=(uint16_t)c->stage[0].stats[21];
        assert(!npe_world_prepare_question(&file,&w,&q));
        for(unsigned s=0;s<c->stages;++s) {
            const QuizStage *st=&c->stage[s];
            if(s) {
                int action=npe_world_quiz_key(&file,&w,st->key);assert(action>=0);
                if(action==2)assert((q.input_type?npe_world_answer_text(&file,&w):
                    npe_world_answer_question(&file,&w,w.event_context_id))>=0);
                else assert((action==1)==(st->commits!=0));
            }
            const unsigned *v=st->state;
            assert(w.game_mode==v[0]&&w.story_event_id==v[1]&&w.event_context_id==v[2]&&
                w.quiz_answer_count==v[3]&&w.quiz_wrong_count==v[4]&&w.quiz_previous_index==v[5]);
            assert(q.input_type==v[6]&&!strcmp(w.quiz_input,st->input));
            for(unsigned i=0;i<10;++i)assert(w.quiz_category_correct[i]==st->stats[2*i]&&
                w.quiz_category_wrong[i]==st->stats[2*i+1]);
            assert(w.quiz_fixed_correct==st->stats[20]&&w.quiz_fixed_wrong==st->stats[21]);
            if(w.game_mode==8&&st->commits) {
                uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],native[sizeof(pixels)];char path[180];
                assert(!npe_render_quiz_page(&file,&font,&w,pixels));
                snprintf(path,sizeof(path),"tmp/native-quiz-%s-%u-0.idx",c->name,s);f=fopen(path,"rb");assert(f);
                assert(fread(native,1,sizeof(native),f)==sizeof(native)&&fgetc(f)==EOF&&!fclose(f));
                unsigned diff=0;for(unsigned i=0;i<sizeof(pixels);++i)diff+=pixels[i]!=native[i];
                fprintf(stderr,"%s stage%u: %u pixels\n",c->name,s,diff);
                if(diff) {f=fopen("tmp/c-quiz-current.idx","wb");assert(f);assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels)&&!fclose(f));}
                assert(!diff);++frames;
            }else if(!st->commits)++ignored;
        }
    }
    printf("Original fixed quiz: %u frames, %u ignored keys; state and text matched\n",frames,ignored);
    npe_font_free(&font);npe_free(&file);return 0;
}
