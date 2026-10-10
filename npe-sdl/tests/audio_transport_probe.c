#include "../audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    const char *name;unsigned operation,before89,before8a,flags88,flags8d,before8e,controls[2];
    int refill8e,resume8a;unsigned callback_or,entries[5][5],trace,route,after89,after8a,after8e;
} TransportNative;
#include "audio_transport_native.h"
typedef struct {
    const TransportNative *native;
    uint16_t f89,f8a,f8e,controls[2];
    unsigned entries[5][5],trace;
} Context;
static int action(Context *c,unsigned slot){
    unsigned *e=c->entries[slot];
    e[0]=c->f89;e[1]=c->f8a;e[2]=c->f8e;++e[4];
    c->trace=(c->trace<<3)|(slot+1);
    c->f89|=(uint16_t)c->native->callback_or;
    if(slot==2&&c->native->refill8e>=0)c->f8e=(uint16_t)c->native->refill8e;
    if(slot==3&&c->native->resume8a>=0)c->f8a=(uint16_t)c->native->resume8a;
    return 0;
}
static int adjust(void *c){return action(c,0);}
static int pause_output(void *c){return action(c,1);}
static int refill(void *c){return action(c,2);}
static int resume_output(void *c){return action(c,3);}
static const NpeAudioTransportBackend backend={adjust,pause_output,refill,resume_output};
int main(void){
    unsigned actions=0,polls=0;
    for(unsigned i=0;i<sizeof(transport_native)/sizeof(*transport_native);++i){
        const TransportNative *n=&transport_native[i];Context c={0};c.native=n;
        c.f89=(uint16_t)n->before89;c.f8a=(uint16_t)n->before8a;c.f8e=(uint16_t)n->before8e;
        for(unsigned j=0;j<2;++j)c.controls[j]=(uint16_t)n->controls[j];
        unsigned route=0;
        if(n->operation==0)assert(!npe_audio_transport_suspend(&c.f89,(uint16_t)n->flags88,&backend,&c));
        else if(n->operation==1){
            int result=npe_audio_transport_resume(&c.f89,&c.f8a,&c.f8e,c.controls,&backend,&c);
            assert(result>=0);route=result==NPE_AUDIO_TRANSPORT_BLOCKED?0:(unsigned)result;
            if(result==NPE_AUDIO_TRANSPORT_BLOCKED)assert(!(n->before89&0x800)&&n->before89&3);
        }else{
            int ready=npe_audio_producer_poll_begin(&c.f89,(uint16_t)n->flags8d);assert(ready>=0);
            if(ready){action(&c,4);npe_audio_service_end(&c.f89,NPE_AUDIO_SERVICE_PRODUCER);++polls;
                if(c.f89&0x400u)route=3; /* Captured original4070 return route. */}
        }
        if(memcmp(c.entries,n->entries,sizeof(c.entries)))fprintf(stderr,"%s action entry states differ\n",n->name);
        assert(!memcmp(c.entries,n->entries,sizeof(c.entries)));
        if(c.f89!=n->after89||c.trace!=n->trace)fprintf(stderr,"%s flags89=%04x/trace=%u differ\n",n->name,c.f89,c.trace);
        assert(c.f89==n->after89&&c.f8a==n->after8a&&c.f8e==n->after8e&&c.trace==n->trace&&route==n->route);
        for(unsigned s=0;s<5;++s)actions+=c.entries[s][4];
    }
    uint16_t flags=0x400,other=0,controls[2]={0x8000,0x8000};
    assert(npe_audio_transport_suspend(NULL,0,&backend,NULL)==-1);
    assert(npe_audio_transport_resume(NULL,&other,&other,controls,&backend,NULL)==-1);
    assert(npe_audio_transport_resume(&flags,NULL,&other,controls,&backend,NULL)==-1);
    assert(npe_audio_transport_resume(&flags,&other,&other,NULL,&backend,NULL)==-1);
    assert(npe_audio_producer_poll_begin(NULL,0)==-1);
    printf("Transport controller:66 original paths, %u backend actions and %u foreground producer callbacks; suspend/resume ordering, EOF bypass, live flags and32 poll guard combinations passed; transport calls are explicit boundaries\n",actions,polls);
    return 0;
}
