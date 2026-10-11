/* Native desktop menus; the game still consumes SDL input events. */
#ifdef _WIN32
#include <SDL_syswm.h>
static HWND desktop_window;
static WNDPROC desktop_original_proc;
static HMENU desktop_menu;
static char *desktop_next_game;
enum { DESKTOP_OPEN=41001,DESKTOP_EXIT,DESKTOP_ABOUT };
static char *desktop_choose_game(HWND owner){
    WCHAR path[32768]={0};OPENFILENAMEW dialog;
    memset(&dialog,0,sizeof(dialog));dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=owner;
    dialog.lpstrFilter=L"NPE games (*.npe)\0*.npe\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=path;dialog.nMaxFile=32768;
    dialog.lpstrTitle=L"\u9009\u62e9 NPE \u6e38\u620f";
    dialog.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_HIDEREADONLY;
    if(!GetOpenFileNameW(&dialog)){
        if(CommDlgExtendedError())MessageBoxW(owner,L"\u65e0\u6cd5\u6253\u5f00\u6587\u4ef6\u9009\u62e9\u7a97\u53e3\u3002",L"NPE",MB_OK|MB_ICONERROR);
        return NULL;
    }
    int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path,-1,NULL,0,NULL,NULL);
    char *selected=length>0?malloc((size_t)length):NULL;
    if(!selected || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path,-1,selected,length,NULL,NULL)){
        free(selected);return NULL;
    }
    return selected;
}
static void desktop_quit(void){SDL_Event event;SDL_zero(event);event.type=SDL_QUIT;SDL_PushEvent(&event);}
static LRESULT CALLBACK desktop_window_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){
    if(message==WM_COMMAND && HIWORD(wparam)==0){
        switch(LOWORD(wparam)){
        case DESKTOP_OPEN:{
            char *selected=desktop_choose_game(hwnd);
            if(selected){free(desktop_next_game);desktop_next_game=selected;game_pad_reset();desktop_quit();}
            return 0;
        }
        case DESKTOP_EXIT:desktop_quit();return 0;
        case DESKTOP_ABOUT:
            MessageBoxW(hwnd,L"\u8bfa\u4e9a\u821f-NPE-\u5f15\u64ce\r\n\r\n\u4f5c\u8005\uff1aHelloClyde\r\n\r\n\u7eaf C11 + SDL2 \u5b9e\u73b0\u7684 NPE \u6e38\u620f\u5f15\u64ce\u3002",
                L"\u5173\u4e8e\u8bfa\u4e9a\u821f-NPE-\u5f15\u64ce",MB_OK|MB_ICONINFORMATION);
            game_pad_reset();return 0;
        }
    }
    return CallWindowProcW(desktop_original_proc,hwnd,message,wparam,lparam);
}
static int desktop_menu_attach(SDL_Window *window){
    SDL_SysWMinfo info;SDL_VERSION(&info.version);
    if(!SDL_GetWindowWMInfo(window,&info) || info.subsystem!=SDL_SYSWM_WINDOWS)return 0;
    HWND hwnd=info.info.win.window;HMENU bar=CreateMenu(),file=CreatePopupMenu(),about=CreatePopupMenu();
    if(!bar||!file||!about){if(bar)DestroyMenu(bar);if(file)DestroyMenu(file);if(about)DestroyMenu(about);return -1;}
    AppendMenuW(file,MF_STRING,DESKTOP_OPEN,L"\u6253\u5f00 NPE...(&O)");
    AppendMenuW(file,MF_SEPARATOR,0,NULL);AppendMenuW(file,MF_STRING,DESKTOP_EXIT,L"\u9000\u51fa(&X)");
    AppendMenuW(about,MF_STRING,DESKTOP_ABOUT,L"\u5173\u4e8e\u8bfa\u4e9a\u821f-NPE-\u5f15\u64ce...");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)file,L"\u6587\u4ef6(&F)");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)about,L"\u5173\u4e8e(&A)");
    if(!SetMenu(hwnd,bar)){DestroyMenu(bar);return -1;}
    desktop_window=hwnd;desktop_menu=bar;
    desktop_original_proc=(WNDPROC)SetWindowLongPtrW(hwnd,GWLP_WNDPROC,(LONG_PTR)desktop_window_proc);
    DrawMenuBar(hwnd);SDL_SetWindowSize(window,960,480);return 0;
}
static void desktop_menu_detach(void){
    if(desktop_window){
        if(desktop_original_proc)SetWindowLongPtrW(desktop_window,GWLP_WNDPROC,(LONG_PTR)desktop_original_proc);
        SetMenu(desktop_window,NULL);
    }
    if(desktop_menu)DestroyMenu(desktop_menu);
    desktop_window=NULL;desktop_original_proc=NULL;desktop_menu=NULL;
}
#else
static int desktop_menu_attach(SDL_Window *window){(void)window;return 0;}
static void desktop_menu_detach(void){}
#endif
