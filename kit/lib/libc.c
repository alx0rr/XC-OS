#include "libc.h"
#include <stdarg.h>

void putchar(char c) {
    volatile char tmp = c;
    sys_write(1, (const void *)&tmp, 1);
}

void puts(const char *s) {
    sys_write(1, s, strlen(s));
}

int putstr(const char *s, int fd) {
    size_t n = strlen(s);
    return sys_write(fd, s, (uint32_t)n);
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

char *strcpy(char *d, const char *s) {
    char *r = d;
    while ((*d++ = *s++));
    return r;
}

char *strncpy(char *d, const char *s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

void *memset(void *p, int v, size_t n) {
    unsigned char *b = p;
    while (n--) *b++ = (unsigned char)v;
    return p;
}

void *memcpy(void *d, const void *s, size_t n) {
    unsigned char *dd = d;
    const unsigned char *ss = s;
    while (n--) *dd++ = *ss++;
    return d;
}

static void pnum(uint32_t v, int base, int pad, char pc) {
    static const char digs[] = "0123456789abcdef";
    char buf[34];
    int  i = 0;
    if (v == 0) { buf[i++] = '0'; }
    else while (v) { buf[i++] = digs[v % base]; v /= base; }
    while (i < pad) buf[i++] = pc;
    char out[34]; int oi = 0;
    while (i--) out[oi++] = buf[i];
    sys_write(1, out, oi);
}

int printf(const char *fmt, ...) {
    char out[512];
    int  oi = 0;
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p && oi < 510; p++) {
        if (*p != '%') { out[oi++] = *p; continue; }
        p++;
        int pad = 0; char pc = ' ';
        if (*p == '0') { pc = '0'; p++; }
        while (*p >= '0' && *p <= '9') { pad = pad*10 + (*p-'0'); p++; }
        char tmp[34]; int ti = 0;
        switch (*p) {
        case 'd': { int v = va_arg(ap, int);
                    if (v < 0 && oi < 509) { out[oi++] = '-'; v = -v; }
                    uint32_t u = (uint32_t)v; int i = 0; char b[32];
                    if (u==0) b[i++]='0'; else while(u){b[i++]="0123456789abcdef"[u%10];u/=10;}
                    while(i<pad) b[i++]=pc; while(i--&&oi<510) out[oi++]=b[i]; break; }
        case 'u': { uint32_t u=va_arg(ap,uint32_t); int i=0; char b[32];
                    if(u==0)b[i++]='0'; else while(u){b[i++]="0123456789"[u%10];u/=10;}
                    while(i<pad)b[i++]=pc; while(i--&&oi<510)out[oi++]=b[i]; break; }
        case 'x': { uint32_t u=va_arg(ap,uint32_t); int i=0; char b[32];
                    if(u==0)b[i++]='0'; else while(u){b[i++]="0123456789abcdef"[u%16];u/=16;}
                    while(i<pad)b[i++]=pc; while(i--&&oi<510)out[oi++]=b[i]; break; }
        case 'c': if(oi<510) out[oi++]=(char)va_arg(ap,int); break;
        case 's': { const char *s=va_arg(ap,const char*); if(!s)s="(null)";
                    while(*s&&oi<510) out[oi++]=*s++; break; }
        case '%': if(oi<510) out[oi++]='%'; break;
        default:  if(oi<509){out[oi++]='%';out[oi++]=*p;} break;
        }
        (void)tmp; (void)ti;
    }
    va_end(ap);
    sys_write(1, out, oi);
    return oi;
}

static void *heap_cur = 0;

static void heap_init(void) {
    heap_cur = sys_brk(0);
}

void *malloc(size_t n) {
    if (!heap_cur) heap_init();
    n = (n + 7) & ~7u;
    void *p = heap_cur;
    void *nxt = (char *)heap_cur + n + sizeof(size_t);
    if (sys_brk(nxt) == (void *)-1) return 0;
    *(size_t *)p = n;
    heap_cur = nxt;
    return (char *)p + sizeof(size_t);
}

void free(void *p) {
    (void)p;
}
