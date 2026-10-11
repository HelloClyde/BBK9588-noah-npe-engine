#ifndef NPE_BBK_STDLIB_H
#define NPE_BBK_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void free(void *);
void *calloc(size_t,size_t);
void *realloc(void *,size_t);
int abs(int);
int atoi(const char *);
unsigned long strtoul(const char *,char **,int);
#endif
