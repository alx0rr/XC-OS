#ifndef _LIBC_H
#define _LIBC_H

#include <stdint.h>
#include <stddef.h>
#include "syscall.h"

void  putchar(char c);
void  puts(const char *s);
int   printf(const char *fmt, ...);
int   putstr(const char *s, int fd);

size_t strlen(const char *s);
char  *strcpy(char *d, const char *s);
char  *strncpy(char *d, const char *s, size_t n);
int    strcmp(const char *a, const char *b);
void  *memset(void *p, int v, size_t n);
void  *memcpy(void *d, const void *s, size_t n);

void  *malloc(size_t n);
void   free(void *p);

#endif
