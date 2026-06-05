#include "libc.h"
#include <stdarg.h>
#include <stdint.h>

void putchar(char c) {
    sys_write(1, &c, 1);
}

void puts(const char *s) {
    while (*s) putchar(*s++);
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
    char buf[32];
    int  i = 0;
    if (v == 0) { buf[i++] = '0'; }
    else while (v) { buf[i++] = digs[v % base]; v /= base; }
    while (i < pad) buf[i++] = pc;
    while (i--) putchar(buf[i]);
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int cnt = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { putchar(*p); cnt++; continue; }
        p++;
        int pad = 0; char pc = ' ';
        if (*p == '0') { pc = '0'; p++; }
        while (*p >= '0' && *p <= '9') { pad = pad * 10 + (*p - '0'); p++; }
        switch (*p) {
        case 'd': { int v = va_arg(ap, int);
                    if (v < 0) { putchar('-'); v = -v; }
                    pnum((uint32_t)v, 10, pad, pc); break; }
        case 'u': pnum(va_arg(ap, uint32_t), 10, pad, pc); break;
        case 'x': pnum(va_arg(ap, uint32_t), 16, pad, pc); break;
        case 'c': putchar((char)va_arg(ap, int)); break;
        case 's': { const char *s = va_arg(ap, const char *);
                    if (!s) s = "(null)";
                    while (*s) { putchar(*s++); cnt++; } break; }
        case '%': putchar('%'); break;
        default:  putchar('%'); putchar(*p); break;
        }
        cnt++;
    }
    va_end(ap);
    return cnt;
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
