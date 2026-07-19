#ifndef _SYSCALL_H
#define _SYSCALL_H

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_READ   2
#define SYS_YIELD  3
#define SYS_GETPID 4
#define SYS_SLEEP  5
#define SYS_MMAP   6
#define SYS_MUNMAP 7
#define SYS_OPEN   8
#define SYS_CLOSE  9
#define SYS_BRK    10

#include <stdint.h>

static inline uint32_t syscall(uint32_t n,
    uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t r;
    __asm__ volatile(
        "int $0x80"
        : "=a"(r)
        : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e)
        : "memory"
    );
    return r;
}

static inline void sys_exit(int code) {
    syscall(SYS_EXIT, (uint32_t)code, 0, 0, 0, 0);
}
static inline int sys_write(int fd, const void *buf, uint32_t len) {
    return (int)syscall(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, len, 0, 0);
}
static inline int sys_read(int fd, void *buf, uint32_t len) {
    return (int)syscall(SYS_READ, (uint32_t)fd, (uint32_t)buf, len, 0, 0);
}
static inline void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}
static inline uint32_t sys_getpid(void) {
    return syscall(SYS_GETPID, 0, 0, 0, 0, 0);
}
static inline void sys_sleep(uint32_t ms) {
    syscall(SYS_SLEEP, ms, 0, 0, 0, 0);
}
static inline void *sys_mmap(void *addr, uint32_t len) {
    return (void *)syscall(SYS_MMAP, (uint32_t)addr, len, 0, 0, 0);
}
static inline int sys_munmap(void *addr, uint32_t len) {
    return (int)syscall(SYS_MUNMAP, (uint32_t)addr, len, 0, 0, 0);
}
static inline int sys_open(const char *path) {
    return (int)syscall(SYS_OPEN, (uint32_t)path, 0, 0, 0, 0);
}
static inline int sys_close(int fd) {
    return (int)syscall(SYS_CLOSE, (uint32_t)fd, 0, 0, 0, 0);
}
static inline void *sys_brk(void *addr) {
    return (void *)syscall(SYS_BRK, (uint32_t)addr, 0, 0, 0, 0);
}

#endif
