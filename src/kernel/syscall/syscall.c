#include "../../include/syscall/syscall.h"
#include "../../include/interrupts/idt.h"
#include "../../include/scheduler/sched.h"
#include "../../include/memory/vmm.h"
#include "../../include/memory/pmm.h"
#include "../../include/fs/xcfs.h"
#include "../../include/proc/proc.h"
#include "../../include/text.h"
#include "../../include/input/keyboard.h"
#include "../../lib/string.h"

#define SYS_FILE_MAX (512 * 1024)

static syscall_fn_t tbl[SYSCALL_MAX];

extern void syscall_stub_asm(void);

#define USER_VIRT_MIN 0x00001000U
#define USER_VIRT_MAX 0xC0000000U
#define uptr_ok(p, n) ((uint32_t)(p) >= USER_VIRT_MIN && \
                       (uint32_t)(p) + (uint32_t)(n) <= USER_VIRT_MAX && \
                       (uint32_t)(p) + (uint32_t)(n) >= (uint32_t)(p))


static uint32_t sys_exit(uint32_t code, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a; (void)b; (void)c; (void)d;
    task_exit_by_code(code);
    while (1) asm volatile("hlt");
    return 0;
}

static uint32_t sys_write(uint32_t fd, uint32_t buf, uint32_t len, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    if (fd != 1 && fd != 2) return (uint32_t)-1;
    if (!uptr_ok(buf, len)) return (uint32_t)-1;
    const char *s = (const char *)buf;
    for (uint32_t i = 0; i < len; i++) {
        char tmp[2] = {s[i], 0};
        printf("%s", tmp);
    }
    return len;
}

static uint32_t sys_read(uint32_t fd, uint32_t buf, uint32_t len, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    if (fd == 0) {
        if (!uptr_ok(buf, len)) return (uint32_t)-1;
        char *dst = (char *)buf;
        uint32_t i = 0;
        while (i < len) {
            char c = keyboard_getchar_raw();
            dst[i++] = c;
            if (c == '\n') break;
        }
        return i;
    }
    if (fd < 3 || fd >= PROC_FD_MAX) return (uint32_t)-1;
    if (!uptr_ok(buf, len)) return (uint32_t)-1;
    proc_t *cur = sched_current();
    if (!cur || !cur->fds[fd].used) return (uint32_t)-1;
    proc_fd_t *f = &cur->fds[fd];
    if (f->pos >= f->size) return 0;
    uint32_t avail = f->size - f->pos;
    uint32_t n = (len < avail) ? len : avail;
    if (cur->vm) {
        uint32_t dst = buf & ~0xFFFU;
        uint32_t end = (buf + n + 0xFFFU) & ~0xFFFU;
        for (uint32_t v = dst; v < end; v += 0x1000) {
            uint32_t pd = cur->vm->directory->entries[v >> 22];
            if (pd & PAGE_PRESENT) {
                page_table_t *pt = (page_table_t *)(pd & 0xFFFFF000U);
                if (pt->entries[(v >> 12) & 0x3FF] & PAGE_PRESENT) continue;
            }
            void *pg = pmm_malloc(0x1000);
            if (!pg) return (uint32_t)-1;
            vmm_map_page_in(cur->vm->directory, v, (uint32_t)pg,
                            PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        }
    }
    memcpy((void *)buf, f->buf + f->pos, n);
    f->pos += n;
    return n;
}

static uint32_t sys_yield(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    task_yield();
    return 0;
}

static uint32_t sys_getpid(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    return sched_current_pid();
}

static uint32_t sys_sleep(uint32_t ms, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a; (void)b; (void)c; (void)d;
    task_sleep(ms);
    return 0;
}

static uint32_t sys_mmap(uint32_t addr, uint32_t len, uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    if (len == 0) return (uint32_t)-1;
    uint32_t pages = (len + 0xFFF) >> 12;
    uint32_t base = addr ? (addr & ~0xFFF) : 0x20000000;
    for (uint32_t i = 0; i < pages; i++) {
        void *phys = pmm_malloc(0x1000);
        if (!phys) return (uint32_t)-1;
        proc_t *cur = sched_current();
        if (cur && cur->vm)
            vmm_map_page_in(cur->vm->directory, base + i * 0x1000,
                            (uint32_t)phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        else
            vmm_map_page(base + i * 0x1000, (uint32_t)phys,
                         PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    return base;
}

static uint32_t sys_munmap(uint32_t addr, uint32_t len, uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    if (len == 0 || addr == 0) return (uint32_t)-1;
    proc_t *cur_proc = sched_current();
    page_directory_t *dir = (cur_proc && cur_proc->vm)
                            ? cur_proc->vm->directory
                            : 0;
    if (!dir) return (uint32_t)-1;
    uint32_t pages = (len + 0xFFF) >> 12;
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t virt = (addr & ~0xFFF) + i * 0x1000;
        uint32_t pd_index = virt >> 22;
        uint32_t pde = dir->entries[pd_index];
        if (!(pde & PAGE_PRESENT) || (pde & 0x80)) continue;
        page_table_t *table = (page_table_t *)(pde & 0xFFFFF000);
        uint32_t pt_index = (virt >> 12) & 0x3FF;
        uint32_t pte = table->entries[pt_index];
        if (!(pte & PAGE_PRESENT)) continue;
        uint32_t phys = pte & 0xFFFFF000;
        table->entries[pt_index] = 0;
        __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
        pmm_free((void *)phys);
    }
    return 0;
}

static uint32_t sys_open(uint32_t path_ptr, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a; (void)b; (void)c; (void)d;
    if (!uptr_ok(path_ptr, 1)) return (uint32_t)-1;
    const char *path = (const char *)path_ptr;
    proc_t *p = sched_current();
    if (!p) return (uint32_t)-1;
    xcfs_dirent_t info;
    if (xcfs_stat(path, &info) != 0) return (uint32_t)-1;
    if (info.size > SYS_FILE_MAX) return (uint32_t)-1;
    for (int i = 3; i < PROC_FD_MAX; i++) {
        if (!p->fds[i].used) {
            uint8_t *fbuf = (uint8_t *)pmm_malloc(info.size + 1);
            if (!fbuf) return (uint32_t)-1;
            if (xcfs_read(path, fbuf, info.size) < 0) {
                pmm_free(fbuf);
                return (uint32_t)-1;
            }
            p->fds[i].used = 1;
            p->fds[i].size = info.size;
            p->fds[i].pos  = 0;
            p->fds[i].buf  = fbuf;
            strncpy(p->fds[i].path, path, XCFS_MAX_PATH - 1);
            return (uint32_t)i;
        }
    }
    return (uint32_t)-1;
}

static uint32_t sys_close(uint32_t fd, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a; (void)b; (void)c; (void)d;
    if (fd < 3 || fd >= PROC_FD_MAX) return (uint32_t)-1;
    proc_t *p = sched_current();
    if (!p || !p->fds[fd].used) return (uint32_t)-1;
    if (p->fds[fd].buf) pmm_free(p->fds[fd].buf);
    p->fds[fd].used = 0;
    p->fds[fd].buf  = 0;
    return 0;
}

void syscall_register(uint32_t n, syscall_fn_t fn) {
    if (n < SYSCALL_MAX) tbl[n] = fn;
}

void syscall_handler(registers_t *regs) {
    uint32_t n = regs->eax;
    if (n >= SYSCALL_MAX || !tbl[n]) {
        regs->eax = (uint32_t)-1;
        return;
    }
    regs->eax = tbl[n](regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
}

void syscall_init() {
    for (int i = 0; i < SYSCALL_MAX; i++) tbl[i] = 0;

    tbl[SYS_EXIT]   = sys_exit;
    tbl[SYS_WRITE]  = sys_write;
    tbl[SYS_READ]   = sys_read;
    tbl[SYS_YIELD]  = sys_yield;
    tbl[SYS_GETPID] = sys_getpid;
    tbl[SYS_SLEEP]  = sys_sleep;
    tbl[SYS_MMAP]   = sys_mmap;
    tbl[SYS_MUNMAP] = sys_munmap;
    tbl[SYS_OPEN]   = sys_open;
    tbl[SYS_CLOSE]  = sys_close;

    idt_set_gate_dpl3(SYSCALL_INT, (uint32_t)syscall_stub_asm, 0x08,
                      0x80 | 0x60 | 0x0F);
}
