#ifndef NPE_BBK_STDIO_H
#define NPE_BBK_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct {int handle,error;} FILE;
extern FILE *stderr,*stdout;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
FILE *fopen(const char *,const char *);
int fclose(FILE *);
size_t fread(void *,size_t,size_t,FILE *);
size_t fwrite(const void *,size_t,size_t,FILE *);
int fgetc(FILE *);
int fseek(FILE *,long,int);
long ftell(FILE *);
int snprintf(char *,size_t,const char *,...);
int fprintf(FILE *,const char *,...);
int printf(const char *,...);
int putchar(int);
int puts(const char *);
int fflush(FILE *);
int sscanf(const char *,const char *,...);
#endif
