#include "touch_ui.h"
#include "assets/header_rgb565.h"

#define RGB(r,g,b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
enum {
    BG = RGB(17,26,36), SHADOW = RGB(9,15,23), FACE = RGB(39,53,70),
    EDGE = RGB(67,84,104), INK = RGB(234,241,247), MUTED = RGB(139,160,181),
    MINT = RGB(99,217,198), BLUE = RGB(173,199,240), ROSE = RGB(242,170,180),
    DOWN = RGB(58,82,105)
};

/* Small native pixel lettering stays sharp on the 240-pixel display. */
static const Uint8 alphabet[26][5] = {
    {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22}, {0x7f,0x41,0x41,0x22,0x1c},
    {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01},
    {0x3e,0x41,0x49,0x49,0x7a}, {0x7f,0x08,0x08,0x08,0x7f},
    {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01},
    {0x7f,0x08,0x14,0x22,0x41}, {0x7f,0x40,0x40,0x40,0x40},
    {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f},
    {0x3e,0x41,0x41,0x41,0x3e}, {0x7f,0x09,0x09,0x09,0x06},
    {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7f,0x01,0x01},
    {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f},
    {0x3f,0x40,0x38,0x40,0x3f}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};
static const Uint8 digits[10][5] = {
    {0x3e,0x51,0x49,0x45,0x3e}, {0,0x42,0x7f,0x40,0},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e}
};
typedef struct { int x,y,w,h,key; const char *label; } Key;
static const Key pad[] = {
    {4,153,46,26,SDLK_F4,"MENU"}, {54,153,46,26,NPE_TOUCH_ALPHA,"KEYS"},
    {104,153,28,26,NPE_TOUCH_VOL_DOWN,"V-"}, {136,153,28,26,NPE_TOUCH_VOL_UP,"V+"},
    {168,153,30,26,NPE_TOUCH_MUTE,"MUTE"}, {202,153,34,26,NPE_TOUCH_QUIT,"EXIT"},
    {42,188,36,34,SDLK_UP,""}, {4,226,36,34,SDLK_LEFT,""},
    {42,226,36,34,SDLK_SPACE,"ACT"}, {80,226,36,34,SDLK_RIGHT,""},
    {42,264,36,34,SDLK_DOWN,""},
    {124,222,108,44,SDLK_RETURN,"OK"},
    {124,274,52,26,SDLK_p,"PREV"}, {180,274,52,26,SDLK_n,"NEXT"},
    {124,188,24,26,SDLK_a,"A"}, {152,188,24,26,SDLK_b,"B"},
    {180,188,24,26,SDLK_c,"C"}, {208,188,24,26,SDLK_d,"D"}
};
static const char *const rows[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
static const char *const row_labels[3][10] = {
    {"Q","W","E","R","T","Y","U","I","O","P"},
    {"A","S","D","F","G","H","J","K","L"},
    {"Z","X","C","V","B","N","M"}
};
static const char *const symbols[4]={"!@#$%^&*()","-_=+[]{}\\|",";:'\",.<>/?","`~"};
static const char *const function_labels[25]={
    "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10",
    "F11","F12","HOME","END","DEL","","","","","INS",
    "PGUP","PGDN","CAPS","CTRL","ALT"
};
static const int function_keys[25]={
    SDLK_F1,SDLK_F1+1,SDLK_F1+2,SDLK_F4,SDLK_F1+4,
    SDLK_F1+5,SDLK_F1+6,SDLK_F1+7,SDLK_F1+8,SDLK_F1+9,
    SDLK_F1+10,SDLK_F12,SDLK_HOME,SDLK_END,SDLK_DELETE,
    SDLK_LEFT,SDLK_UP,SDLK_DOWN,SDLK_RIGHT,SDLK_INSERT,
    SDLK_PAGEUP,SDLK_PAGEDOWN,NPE_TOUCH_CAPS,NPE_TOUCH_CTRL,NPE_TOUCH_ALT
};

static int key_count(int page) {return page==NPE_UI_PAD?18:page==NPE_UI_ALPHA?47:page==NPE_UI_SYMBOLS?42:35;}
static Key key_at(int page, int index) {
    Key k;
    if(page==NPE_UI_PAD)return pad[index];
    if(index<5) {
        static const int xs[5]={4,48,94,144,188},ws[5]={40,42,46,40,48};
        static const int codes[5]={NPE_TOUCH_PAD,0,NPE_TOUCH_SHIFT,NPE_TOUCH_FUNCTIONS,SDLK_ESCAPE};
        static const char *const labels[5]={"PAD",0,"SHIFT","FN","ESC"};
        k=(Key){xs[index],151,ws[index],23,codes[index],labels[index]};
        if(index==1){k.key=page==NPE_UI_ALPHA?NPE_TOUCH_SYMBOLS:NPE_TOUCH_ALPHA;k.label=page==NPE_UI_ALPHA?"SYM":"ABC";}
        return k;
    }
    index-=5;
    int count=page==NPE_UI_ALPHA?38:page==NPE_UI_SYMBOLS?33:26;
    if(index>=count) {
        static const int codes[4]={NPE_TOUCH_SYMBOLS,SDLK_TAB,SDLK_SPACE,SDLK_RETURN};
        static const int xs[4]={4,50,86,176},ws[4]={42,32,86,60};
        static const char *const labels[4]={"SYM","TAB","SPACE","ENTER"};
        int col=index-count;
        k=(Key){xs[col],299,ws[col],19,codes[col],labels[col]};
        if(col==0&&page==NPE_UI_SYMBOLS){k.key=NPE_TOUCH_ALPHA;k.label="ABC";}
        return k;
    }
    if(page==NPE_UI_ALPHA) {
        static const char *const nums[10]={"1","2","3","4","5","6","7","8","9","0"};
        if(index<10)return (Key){1+24*index,179,22,25,"1234567890"[index],nums[index]};
        if(index==36)return (Key){4,269,31,25,NPE_TOUCH_SHIFT,"SHFT"};
        if(index==37)return (Key){205,269,31,25,SDLK_BACKSPACE,"BS"};
        index-=10;
        int row=index<10?0:index<19?1:2;
        int col=index-(row==0?0:row==1?10:19);
        k=(Key){(row==0?1:row==1?13:39)+24*col,209+30*row,22,25,
                rows[row][col],row_labels[row][col]};
    } else if(page==NPE_UI_SYMBOLS) {
        if(index==32)return (Key){190,269,46,25,SDLK_BACKSPACE,"BS"};
        int row=index/10,col=index%10;
        k=(Key){1+24*col,179+30*row,22,25,symbols[row][col],0};
    } else {
        if(index==25)return (Key){201,275,38,20,SDLK_BACKSPACE,"BS"};
        k=(Key){1+48*(index%5),179+24*(index/5),46,20,function_keys[index],function_labels[index]};
        if(index>=20)k=(Key){1+40*(index-20),275,38,20,function_keys[index],function_labels[index]};
    }
    return k;
}

int npe_touch_hit(int page, int x, int y) {
    int count=key_count(page);
    for(int i=0;i<count;++i) {
        Key k=key_at(page,i);
        if(x>=k.x&&x<k.x+k.w&&y>=k.y&&y<k.y+k.h)return k.key;
    }
    return 0;
}

static void fill(Uint8 *pixels,int x,int y,int w,int h,Uint16 color) {
    for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i) {
        unsigned p=(j*240+i)*2;
        pixels[p]=color;pixels[p+1]=color>>8;
    }
}
static void rounded(Uint8 *p,int x,int y,int w,int h,int radius,Uint16 c) {
    static const Uint8 inset[6]={4,2,1,1,0,0};
    for(int j=0;j<h;++j) {
        int edge=j<h/2?j:h-1-j;
        int pad=edge<radius?inset[edge+(6-radius)]:0;
        fill(p,x+pad,y+j,w-2*pad,1,c);
    }
}
static int text_width(const char *s,int scale) {
    int n=0;while(*s++)++n;
    return n?(n*6-1)*scale:0;
}
static void text(Uint8 *p,int x,int y,const char *s,int scale,Uint16 c) {
    static const Uint8 equals[5]={0x14,0x14,0x14,0x14,0x14};
    static const Uint8 slash[5]={0x40,0x20,0x08,0x04,0x02};
    static const char punctuation[]="!\"#$%&'()*+,-.:;<>?@[\\]^_`{|}~";
    static const Uint8 marks[][5]={
        {0,0,0x5f,0,0},{0,7,0,7,0},{0x14,0x7f,0x14,0x7f,0x14},
        {0x24,0x2a,0x7f,0x2a,0x12},{0x23,0x13,8,0x64,0x62},
        {0x36,0x49,0x55,0x22,0x50},{0,0,7,0,0},
        {0,0x1c,0x22,0x41,0},{0,0x41,0x22,0x1c,0},
        {0x14,8,0x3e,8,0x14},{8,8,0x3e,8,8},{0,0x50,0x30,0,0},
        {8,8,8,8,8},{0,0x60,0x60,0,0},{0,0x36,0x36,0,0},
        {0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},
        {0,0x41,0x22,0x14,8},{2,1,0x51,9,6},
        {0x32,0x49,0x79,0x41,0x3e},{0,0x7f,0x41,0x41,0},
        {2,4,8,0x10,0x20},{0,0x41,0x41,0x7f,0},
        {4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40},{0,1,2,0,0},
        {8,0x36,0x41,0x41,0},{0,0,0x7f,0,0},
        {0,0x41,0x41,0x36,8},{8,4,8,0x10,8}
    };
    while(*s) {
        const Uint8 *g=0;
        char ch=*s++;
        if(ch>='A'&&ch<='Z')g=alphabet[ch-'A'];
        else if(ch>='0'&&ch<='9')g=digits[ch-'0'];
        else if(ch=='=')g=equals;
        else if(ch=='/')g=slash;
        else for(unsigned i=0;i<sizeof(punctuation)-1;++i)if(ch==punctuation[i]){g=marks[i];break;}
        if(g)for(int col=0;col<5;++col)for(int row=0;row<7;++row)
            if(g[col]&(1<<row))fill(p,x+col*scale,y+row*scale,scale,scale,c);
        x+=6*scale;
    }
}
static void arrow(Uint8 *p,int x,int y,int key,Uint16 c) {
    /* Fifteen-pixel arrows have the same visual weight as the large letters. */
    for(int row=0;row<15;++row)for(int col=0;col<15;++col) {
        int a=col,b=row;
        if(key==SDLK_DOWN)b=14-row;
        if(key==SDLK_LEFT){a=row;b=col;}
        if(key==SDLK_RIGHT){a=row;b=14-col;}
        int d=a>7?a-7:7-a;
        if((b<8&&d<=b)||(b>=8&&a>=5&&a<=9))fill(p,x+col,y+row,1,1,c);
    }
}

void npe_touch_draw(Uint8 *p,int page,Uint16 modifiers,int volume,int audio_error,int pressed_x,int pressed_y) {
    fill(p,0,0,240,28,BG);
    for(int y=0;y<NPE_HEADER_HEIGHT;++y)for(int x=0;x<NPE_HEADER_WIDTH;++x)
        fill(p,7+x,1+y,1,1,npe_header_pixels[y*NPE_HEADER_WIDTH+x]);
    char status[16];
    if(audio_error)memcpy(status,"SOUND ERR",10);
    else if(!volume)memcpy(status,"MUTE",5);
    else snprintf(status,sizeof(status),"VOL %d",volume);
    text(p,232-text_width(status,1),10,status,1,audio_error?ROSE:MUTED);
    fill(p,0,26,240,1,EDGE);
    fill(p,0,148,240,172,BG);
    if(page==NPE_UI_PAD)text(p,8,307,"HOLD TO MOVE",1,MUTED);
    int count=key_count(page);
    for(int i=0;i<count;++i) {
        Key k=key_at(page,i);
        char literal[2]={(char)k.key,0};
        const char *label=k.label?k.label:literal;
        if(page==NPE_UI_ALPHA&&(modifiers&KMOD_SHIFT)&&k.key>='0'&&k.key<='9') {
            literal[0]=k.key=='0'?')':"!@#$%^&*("[k.key-'1'];label=literal;
        }
        Uint16 face=FACE,ink=INK,edge=EDGE;
        if(k.key==SDLK_RETURN){face=MINT;ink=BG;edge=MINT;}
        if(k.key==NPE_TOUCH_PAD||k.key==NPE_TOUCH_ALPHA||k.key==NPE_TOUCH_SYMBOLS){face=BLUE;ink=BG;edge=BLUE;}
        if((k.key==NPE_TOUCH_SHIFT&&(modifiers&KMOD_SHIFT))||
           (k.key==NPE_TOUCH_CAPS&&(modifiers&KMOD_CAPS))||
           (k.key==NPE_TOUCH_CTRL&&(modifiers&KMOD_CTRL))||
           (k.key==NPE_TOUCH_ALT&&(modifiers&KMOD_ALT))||
           (k.key==NPE_TOUCH_FUNCTIONS&&page==NPE_UI_FUNCTIONS)) {face=BLUE;ink=BG;edge=BLUE;}
        if(k.key==NPE_TOUCH_QUIT)ink=ROSE;
        if(k.key==NPE_TOUCH_MUTE&&!volume){face=ROSE;ink=BG;edge=ROSE;}
        int pressed=pressed_x>=k.x&&pressed_x<k.x+k.w&&pressed_y>=k.y&&pressed_y<k.y+k.h;
        if(pressed){face=DOWN;edge=MINT;ink=INK;}
        int y=k.y+(pressed?2:0);
        rounded(p,k.x,k.y+2,k.w,k.h,6,SHADOW);
        rounded(p,k.x,y,k.w,k.h,6,edge);
        rounded(p,k.x+1,y+1,k.w-2,k.h-2,5,face);
        if(k.key==SDLK_UP||k.key==SDLK_DOWN||k.key==SDLK_LEFT||k.key==SDLK_RIGHT)
            arrow(p,k.x+(k.w-15)/2,y+(k.h-15)/2,k.key,ink);
        else {
            int scale=label[0]&&!label[1]?2:1;
            if(k.key==SDLK_RETURN&&page==NPE_UI_PAD)scale=2;
            text(p,k.x+(k.w-text_width(label,scale))/2,
                 y+(k.h-7*scale)/2,label,scale,ink);
        }
    }
}
