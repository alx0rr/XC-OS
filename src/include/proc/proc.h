#ifndef PROC_H
#define PROC_H
#include <stdint.h>
#include "../memory/vmm.h"

#define PROC_MAX        32
#define PROC_NAME_LEN   32
#define KSTACK_SIZE     8192
#define USTACK_SIZE     8192
#define USTACK_TOP      0xBFFFF000
#define UCODE_BASE      0x08000000

#define PROC_PRIORITY_HIGH   0
#define PROC_PRIORITY_NORMAL 1
#define PROC_PRIORITY_LOW    2

typedef enum {
    PS_FREE = 0,
    PS_READY,
    PS_RUNNING,
    PS_BLOCKED,
    PS_ZOMBIE
} proc_state_t;

typedef struct {
    uint32_t eip, esp, ebp;
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi;
    uint32_t eflags;
    uint32_t cs, ds, ss;
} cpu_ctx_t;

typedef struct proc {
    uint32_t     pid;
    char         name[PROC_NAME_LEN];
    proc_state_t state;

    cpu_ctx_t    ctx;

    vm_space_t  *vm;
    void        *kstack;
    uint32_t     kstack_top;

    uint32_t     ticks;
    uint32_t     exit_code;
    uint8_t      priority;

    struct proc *next;
} proc_t;

proc_t* proc_create_kernel(const char *name, void (*fn)());
proc_t* proc_create_user(const char *name, void *code, uint32_t code_sz);
void    proc_free(proc_t *p);
proc_t* proc_get(uint32_t pid);

#endif
