#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
#include "../../include/interrupts/idt.h"

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_GETPID  4
#define SYS_SLEEP   5
#define SYS_MMAP    6
#define SYS_MUNMAP  7
#define SYS_OPEN    8
#define SYS_CLOSE   9
#define SYS_BRK     10
#define SYS_EXEC    11

#define SYSCALL_INT  0x80
#define SYSCALL_MAX  16

typedef uint32_t (*syscall_fn_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

void syscall_init();
void syscall_handler(registers_t *regs);
void syscall_register(uint32_t n, syscall_fn_t fn);

#endif
