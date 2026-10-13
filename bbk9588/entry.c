#include "bda_sdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int npe_host_main(int,char **);
extern char npe_bbk_log[4096];
extern int npe_bbk_on_stack(void *,int (*)(void));
static bda_file_selector_t selector;
static int run_game(void){char *args[]={"NPE9588",selector.path,NULL};return npe_host_main(2,args);}
__attribute__((section(".text.bda_main")))
int bda_main(void){
    void *stack;int result;
    /* main/UI use large automatic world/frame/save buffers. Do not borrow
       the small firmware caller stack for the desktop engine's call tree. */
    memset(&selector,0,sizeof(selector));
    bda_fs_mkdir("A:\\NPE");bda_fs_mkdir("A:\\NPE\\saves");
    /* Firmware matches extensions case-insensitively; duplicate aliases
       enumerate the same file twice. A single filter accepts .npe and .NPE. */
    if(bda_gui_select_file(&selector,"A:\\NPE\\","npe","NPE Game")!=BDA_FILE_SELECTOR_SELECTED)return 0;
    stack=malloc(2*1024*1024);
    if(!stack){bda_msgbox("NPE9588","Cannot allocate engine stack.");return 1;}
    result=npe_bbk_on_stack((char *)stack+2*1024*1024,run_game);
    free(stack);
    FILE *log=fopen("A:\\NPE\\NPE.LOG","wb");
    if(log){fprintf(log,"NPE9588 result=%d\n",result);fwrite(npe_bbk_log,1,strlen(npe_bbk_log),log);fclose(log);}
    bda_fs_flush_all();
    if(result)bda_msgbox("NPE9588",npe_bbk_log[0]?npe_bbk_log:"Failed to load game. Check A:\\NPE\\assets fonts.");
    return result;
}
