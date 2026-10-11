#include "bda_sdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>

/* Allocate through firmware, retaining size for realloc and eight-byte alignment. */
void *malloc(size_t n){uint32_t *p;if(n>UINT32_MAX-8)return NULL;p=bda_alloc(n+8);if(!p)return NULL;p[0]=n;p[1]=0;return p+2;}
void free(void *p){if(p)bda_free((uint32_t *)p-2);}
void *calloc(size_t n,size_t s){void *p;if(s&&n>UINT32_MAX/s)return NULL;p=malloc(n*s);if(p)memset(p,0,n*s);return p;}
void *realloc(void *p,size_t n){void *q;size_t old;if(!p)return malloc(n);if(!n){free(p);return NULL;}old=((uint32_t *)p)[-2];q=malloc(n);if(q){memcpy(q,p,old<n?old:n);free(p);}return q;}
void *memset(void *p,int v,size_t n){unsigned char *d=p;while(n--)*d++=v;return p;}
void *memcpy(void *p,const void *q,size_t n){unsigned char *d=p;const unsigned char *s=q;while(n--)*d++=*s++;return p;}
void *memmove(void *p,const void *q,size_t n){unsigned char *d=p;const unsigned char *s=q;if(d<s)return memcpy(p,q,n);while(n){--n;d[n]=s[n];}return p;}
int memcmp(const void *p,const void *q,size_t n){const unsigned char *a=p,*b=q;while(n--){if(*a!=*b)return *a-*b;++a;++b;}return 0;}
void *memchr(const void *p,int v,size_t n){const unsigned char *s=p;while(n--){if(*s==(unsigned char)v)return (void *)s;++s;}return NULL;}
size_t strlen(const char *s){const char *p=s;while(*p)++p;return p-s;}
int strcmp(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char *a,const char *b,size_t n){while(n--){int d=(unsigned char)*a-(unsigned char)*b;if(d||!*a)return d;++a;++b;}return 0;}
char *strchr(const char *s,int c){do{if(*s==(char)c)return (char *)s;}while(*s++);return NULL;}
char *strstr(const char *s,const char *p){size_t n=strlen(p);if(!n)return (char *)s;for(;*s;++s)if(!strncmp(s,p,n))return (char *)s;return NULL;}
size_t strcspn(const char *s,const char *reject){size_t n=0;while(s[n]&&!strchr(reject,s[n]))++n;return n;}
int abs(int v){return v<0?-v:v;}
unsigned long strtoul(const char *s,char **end,int base){unsigned long v=0;int negative=0;while(*s==' '||*s=='\t')++s;if(*s=='+'||*s=='-')negative=*s++=='-';if(!base)base=*s=='0'?8:10;if((base==0||base==16)&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')){base=16;s+=2;}for(;;++s){unsigned d=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:*s>='A'&&*s<='F'?*s-'A'+10:255;if(d>=(unsigned)base)break;v=v*base+d;}if(end)*end=(char *)s;return negative?-v:v;}
int atoi(const char *s){return (int)strtoul(s,NULL,10);}

static FILE console={0,0};
FILE *stderr=&console,*stdout=&console;
char npe_bbk_log[4096];
static size_t log_used;
void npe_bbk_log_reset(void){log_used=0;npe_bbk_log[0]=0;}
static void log_append(const char *s,size_t n){if(n>sizeof(npe_bbk_log)-1-log_used)n=sizeof(npe_bbk_log)-1-log_used;memcpy(npe_bbk_log+log_used,s,n);log_used+=n;npe_bbk_log[log_used]=0;}
FILE *fopen(const char *path,const char *mode){FILE *f;char normalized[512];size_t n=strlen(path);if(n>=sizeof(normalized))return NULL;for(size_t i=0;i<=n;++i)normalized[i]=path[i]=='/'?'\\':path[i];int h=bda_fs_fopen_raw(normalized,mode);if(!bda_fs_file_is_valid(h))return NULL;f=malloc(sizeof(*f));if(!f){bda_fs_close_raw(h);return NULL;}f->handle=h;f->error=0;return f;}
int fclose(FILE *f){int r;if(!f||f==&console)return -1;r=bda_fs_close_raw(f->handle);free(f);return r;}
size_t fread(void *p,size_t s,size_t n,FILE *f){int r;if(!f||!s||n>UINT32_MAX/s)return 0;r=bda_fs_fread_raw(p,s,n,f->handle);if(r<0){f->error=1;return 0;}return r;}
size_t fwrite(const void *p,size_t s,size_t n,FILE *f){int r;if(!f||!s||n>UINT32_MAX/s)return 0;if(f==&console){log_append(p,s*n);return n;}r=bda_fs_fwrite_raw(p,s,n,f->handle);if(r<0){f->error=1;return 0;}return r;}
int fgetc(FILE *f){unsigned char c;return fread(&c,1,1,f)==1?c:EOF;}
int fseek(FILE *f,long o,int w){return bda_fs_seek_raw(f->handle,o,w)<0?-1:0;}
long ftell(FILE *f){return bda_fs_tell_raw(f->handle);}

/* Bounded integer/string formatter: covers engine UI and 64-bit save fingerprints. */
typedef struct {char *p;size_t cap,n;} Output;
static void emit(Output *o,char c){if(o->n+1<o->cap)o->p[o->n]=c;++o->n;}
static int format(char *p,size_t cap,const char *fmt,va_list ap){Output o={p,cap,0};while(*fmt){if(*fmt!='%'){emit(&o,*fmt++);continue;}++fmt;int left=0,zero=0,width=0,prec=-1,len=0;while(*fmt=='-'||*fmt=='0'||*fmt=='+'||*fmt==' '||*fmt=='#'){if(*fmt=='-')left=1;if(*fmt=='0')zero=1;++fmt;}if(*fmt=='*'){width=va_arg(ap,int);++fmt;}else while(*fmt>='0'&&*fmt<='9')width=width*10+*fmt++-'0';if(*fmt=='.'){++fmt;prec=0;if(*fmt=='*'){prec=va_arg(ap,int);++fmt;}else while(*fmt>='0'&&*fmt<='9')prec=prec*10+*fmt++-'0';}if(*fmt=='z'){len=1;++fmt;}else if(*fmt=='l'){len=1;++fmt;if(*fmt=='l'){len=2;++fmt;}}else if(*fmt=='h'){++fmt;if(*fmt=='h')++fmt;}char kind=*fmt++;char buf[66],sign=0;const char *s=buf;int n=0;if(kind=='%'){emit(&o,'%');continue;}if(kind=='s'){s=va_arg(ap,const char *);if(!s)s="(null)";n=strlen(s);if(prec>=0&&prec<n)n=prec;}else if(kind=='c'){buf[0]=(char)va_arg(ap,int);n=1;}else if(strchr("diuxXp",kind)){uint64_t v;unsigned base=kind=='x'||kind=='X'||kind=='p'?16:10;if(kind=='d'||kind=='i'){int64_t sv=len==2?va_arg(ap,long long):len==1?va_arg(ap,long):va_arg(ap,int);if(sv<0){sign='-';v=0-(uint64_t)sv;}else v=sv;}else v=kind=='p'?(uintptr_t)va_arg(ap,void *):len==2?va_arg(ap,unsigned long long):len==1?va_arg(ap,unsigned long):va_arg(ap,unsigned);do{unsigned d=v%base;buf[n++]=d<10?'0'+d:(kind=='X'?'A':'a')+d-10;v/=base;}while(v);for(int i=0;i<n/2;++i){char c=buf[i];buf[i]=buf[n-1-i];buf[n-1-i]=c;}}else{emit(&o,'?');continue;}int pad=width-n-(sign?1:0),zeros=prec>n?prec-n:0;if(pad<zeros)pad=zeros;if(!left&&!zero)while(pad-->0)emit(&o,' ');if(sign)emit(&o,sign);if(!left&&zero)while(pad-->0)emit(&o,'0');else while(zeros-->0)emit(&o,'0');for(int i=0;i<n;++i)emit(&o,s[i]);if(left)while(pad-->0)emit(&o,' ');}if(cap)p[o.n<cap?o.n:cap-1]=0;return o.n;}
int snprintf(char *p,size_t cap,const char *fmt,...){va_list a;va_start(a,fmt);int n=format(p,cap,fmt,a);va_end(a);return n;}
int fprintf(FILE *f,const char *fmt,...){char b[512];va_list a;va_start(a,fmt);int n=format(b,sizeof(b),fmt,a);va_end(a);fwrite(b,1,strlen(b),f);return n;}
int printf(const char *fmt,...){char b[512];va_list a;va_start(a,fmt);int n=format(b,sizeof(b),fmt,a);va_end(a);log_append(b,strlen(b));return n;}
int sscanf(const char *s,const char *fmt,...){const char *start=s;int count=0;va_list a;va_start(a,fmt);while(*fmt){if(*fmt!='%'){if(*s++!=*fmt++)break;continue;}++fmt;if(*fmt=='n'){*va_arg(a,int *)=s-start;++fmt;continue;}if(*fmt=='u'){char *end;unsigned long v=strtoul(s,&end,10);if(end==s)break;*va_arg(a,unsigned *)=v;s=end;++count;++fmt;continue;}break;}va_end(a);return count;}
int putchar(int c){char b=c;log_append(&b,1);return c;}
int puts(const char *s){log_append(s,strlen(s));putchar('\n');return 0;}
int fflush(FILE *f){(void)f;bda_fs_flush_all();return 0;}
