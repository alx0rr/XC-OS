#ifndef IO_H
#define IO_H

#include "types.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "dN"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "dN"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a"(result) : "dN"(port));
    return result;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "dN"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    __asm__ volatile ("inl %1, %0" : "=a"(result) : "dN"(port));
    return result;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "dN"(port));
}

#endif
