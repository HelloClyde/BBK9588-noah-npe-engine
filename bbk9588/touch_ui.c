#include "touch_ui.h"

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
static const int pad_keys[16] = {
    SDLK_UP,SDLK_p,SDLK_n,-2, SDLK_LEFT,SDLK_DOWN,SDLK_RIGHT,SDLK_RETURN,
    SDLK_SPACE,SDLK_F4,SDLK_F4,-1, SDLK_a,SDLK_b,SDLK_c,SDLK_d
};
static const char *const pad_labels[16] = {
    "", "PREV", "NEXT", "EXIT", "", "", "", "OK",
    "ACT", "BACK", "MENU", "ABC", "A", "B", "C", "D"
};
static const char *const rows[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
static const char *const row_labels[3][10] = {
    {"Q","W","E","R","T","Y","U","I","O","P"},
    {"A","S","D","F","G","H","J","K","L"},
    {"Z","X","C","V","B","N","M"}
};

static Key key_at(int keyboard, int index) {
    Key k;
    if (!keyboard) {
        k=(Key){3+60*(index%4),158+40*(index/4),54,34,
                pad_keys[index],pad_labels[index]};
    } else if (index<26) {
        int row=index<10?0:index<19?1:2;
        int col=index-(row==0?0:row==1?10:19);
        k=(Key){(row==0?1:row==1?13:37)+24*col,174+38*row,22,32,
                rows[row][col],row_labels[row][col]};
    } else {
        static const int codes[4]={SDLK_BACKSPACE,SDLK_EQUALS,SDLK_RETURN,-1};
        static const char *const labels[4]={"DEL","=","OK","PAD"};
        int col=index-26;
        k=(Key){3+60*col,290,54,26,codes[col],labels[col]};
    }
    return k;
}

int npe_touch_hit(int keyboard, int x, int y) {
    int count=keyboard?30:16;
    for(int i=0;i<count;++i) {
        Key k=key_at(keyboard,i);
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
    while(*s) {
        const Uint8 *g=0;
        char ch=*s++;
        if(ch>='A'&&ch<='Z')g=alphabet[ch-'A'];
        else if(ch>='0'&&ch<='9')g=digits[ch-'0'];
        else if(ch=='=')g=equals;
        else if(ch=='/')g=slash;
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

void npe_touch_draw(Uint8 *p,int keyboard,int pressed_x,int pressed_y) {
    fill(p,0,0,240,28,BG);
    /* Compact emblem, title and device mark leave the game unobstructed. */
    rounded(p,7,6,14,14,5,MINT);
    fill(p,11,10,2,6,BG);fill(p,13,12,2,2,BG);fill(p,15,10,2,6,BG);
    text(p,28,6,"NOAH",2,INK);
    text(p,145,10,"NPE / 9588",1,MUTED);
    fill(p,0,26,240,1,EDGE);
    fill(p,0,148,240,172,BG);
    if(keyboard) {
        text(p,9,156,"ENGLISH",1,MUTED);
        text(p,196,156,"QWERTY",1,MUTED);
    }
    int count=keyboard?30:16;
    for(int i=0;i<count;++i) {
        Key k=key_at(keyboard,i);
        Uint16 face=FACE,ink=INK,edge=EDGE;
        if(k.key==SDLK_RETURN){face=MINT;ink=BG;edge=MINT;}
        if(k.key==-1){face=BLUE;ink=BG;edge=BLUE;}
        if(k.key==-2)ink=ROSE;
        int pressed=pressed_x>=k.x&&pressed_x<k.x+k.w&&pressed_y>=k.y&&pressed_y<k.y+k.h;
        if(pressed){face=DOWN;edge=MINT;ink=INK;}
        int y=k.y+(pressed?2:0);
        rounded(p,k.x,k.y+2,k.w,k.h,6,SHADOW);
        rounded(p,k.x,y,k.w,k.h,6,edge);
        rounded(p,k.x+1,y+1,k.w-2,k.h-2,5,face);
        if(k.key==SDLK_UP||k.key==SDLK_DOWN||k.key==SDLK_LEFT||k.key==SDLK_RIGHT)
            arrow(p,k.x+(k.w-15)/2,y+(k.h-15)/2,k.key,ink);
        else {
            int scale=k.label[0]&&!k.label[1]?2:1;
            if(k.key==SDLK_RETURN&&!keyboard)scale=2;
            text(p,k.x+(k.w-text_width(k.label,scale))/2,
                 y+(k.h-7*scale)/2,k.label,scale,ink);
        }
    }
}
