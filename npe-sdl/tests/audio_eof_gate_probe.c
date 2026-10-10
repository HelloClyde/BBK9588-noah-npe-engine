#include "../audio.h"
#include <assert.h>
#include <stdio.h>
typedef struct {
    const char *name;int gate;
    unsigned before89,flags8d,stack,before8a,called,callback89,reopen,
        cleanup89,cleanup8a,after89,after8a,ticks;
} EofNative;
#include "audio_eof_gate_native.h"
int main(void){
    unsigned callbacks=0;
    for(unsigned i=0;i<sizeof(eof_native)/sizeof(*eof_native);++i){
        const EofNative *n=&eof_native[i];uint16_t flags89=(uint16_t)n->before89,flags8a=(uint16_t)n->before8a;
        int gate=npe_audio_eof_gate(&flags89,(uint16_t)n->flags8d,(uint16_t)n->stack);
        assert(gate==n->gate);int called=0;
        if(gate==NPE_AUDIO_EOF_READY){
            called=npe_audio_eof_callback_begin(&flags89);assert(called>=0);
            if(called){
                assert(flags89==n->callback89);
                if(n->reopen){npe_audio_cleanup_flags(&flags89,&flags8a);
                    assert(flags89==n->cleanup89&&flags8a==n->cleanup8a);}
                npe_audio_eof_callback_end(&flags89);++callbacks;
            }
        }
        if(flags89!=n->after89||flags8a!=n->after8a)fprintf(stderr,"%s:89=%04x/8a=%04x differs\n",n->name,flags89,flags8a);
        assert((unsigned)called==n->called&&flags89==n->after89&&flags8a==n->after8a);
        assert(n->ticks==(gate==NPE_AUDIO_EOF_READY?
            (n->reopen?((n->before8a&0x80u)?100u:300u):50u):0));
    }
    /* Registration is evaluated after stop, not captured by the IRQ gate;
       return clears the active bit in state modified by the callback. */
    uint16_t flags89=0xf800;
    assert(npe_audio_eof_gate(&flags89,0,0x6000)==NPE_AUDIO_EOF_READY);
    flags89&=(uint16_t)~0x8000u;assert(!npe_audio_eof_callback_begin(&flags89)&&!(flags89&8u));
    flags89|=0x8000u;assert(npe_audio_eof_callback_begin(&flags89)==1);
    flags89=0xffff;npe_audio_eof_callback_end(&flags89);assert(flags89==0xfff7);
    flags89=0x7000;assert(npe_audio_eof_gate(&flags89,0,0x6000)==NPE_AUDIO_EOF_IGNORED&&flags89==0x7000);
    assert(npe_audio_eof_gate(NULL,0,0)==-1&&npe_audio_eof_callback_begin(NULL)==-1);
    printf("EOF gate:56 original IRQ paths/%u callback entries; five busy flags, early stack/active guards, cleanup masks and current callback state passed\n",callbacks);
    return 0;
}
