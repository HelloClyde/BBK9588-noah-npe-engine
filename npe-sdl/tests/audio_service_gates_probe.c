#include "../audio.h"
#include <assert.h>
#include <stdio.h>
typedef struct {
    const char *name;
    unsigned service,before89,before8a,flags8d,before8e,callback_or,callback_clear;
    unsigned entries[3][5],after89,after8a,after8d,after8e,route;
} ServiceNative;
#include "audio_service_gates_native.h"
int main(void){
    unsigned calls=0;
    for(unsigned i=0;i<sizeof(service_native)/sizeof(*service_native);++i){
        const ServiceNative *n=&service_native[i];
        uint16_t f89=(uint16_t)n->before89,f8e=(uint16_t)n->before8e;
        unsigned route=0,entries[3][5]={{0}};
        for(unsigned service=n->service;service<3;++service){
            int gate=npe_audio_service_begin(&f89,(uint16_t)n->before8a,
                (uint16_t)n->flags8d,&f8e,service);
            assert(gate>=0);
            if(gate==NPE_AUDIO_SERVICE_YIELD||gate==NPE_AUDIO_SERVICE_RESUME){
                route=(unsigned)gate;break;
            }
            if(gate!=NPE_AUDIO_SERVICE_READY)continue;
            entries[service][0]=f89;entries[service][1]=n->before8a;
            entries[service][2]=n->flags8d;entries[service][3]=f8e;
            entries[service][4]=1;++calls;
            f89|=(uint16_t)n->callback_or;f89&=(uint16_t)~n->callback_clear;
            npe_audio_service_end(&f89,service);
        }
        for(unsigned s=0;s<3;++s)for(unsigned v=0;v<5;++v){
            if(entries[s][v]!=n->entries[s][v])fprintf(stderr,"%s entry%u field%u %04x != %04x\n",n->name,s,v,entries[s][v],n->entries[s][v]);
            assert(entries[s][v]==n->entries[s][v]);
        }
        if(f89!=n->after89||f8e!=n->after8e||route!=n->route)
            fprintf(stderr,"%s final89=%04x/8e=%04x/route=%u differs\n",n->name,f89,f8e,route);
        assert(f89==n->after89&&f8e==n->after8e&&route==n->route);
        assert(n->before8a==n->after8a&&n->flags8d==n->after8d);
    }
    uint16_t flags=0xffff,request=2;
    assert(npe_audio_service_begin(NULL,0,0,&request,0)==-1);
    assert(npe_audio_service_begin(&flags,0,0,NULL,0)==-1);
    assert(npe_audio_service_begin(&flags,0,0,&request,3)==-1);
    npe_audio_service_end(&flags,3);assert(flags==0xffff);
    printf("Callback services:72 original gate/dispatch/return paths, %u actual entries, live requests and busy/registration/storage/starvation/resume ordering passed; captured hardware boundaries only\n",calls);
    return 0;
}
