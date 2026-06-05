#include "../include/elf.h"
#include "../include/memory/vmm.h"
#include "../include/memory/pmm.h"
#include "../include/proc/proc.h"
#include "../include/gdt/gdt.h"
#include "../include/text.h"
#include "../lib/string.h"

int elf_validate(const uint8_t *img, uint32_t sz) {
    if (sz < sizeof(elf32_ehdr_t)) return -1;
    const elf32_ehdr_t *e = (const elf32_ehdr_t *)img;
    if (e->e_magic   != ELF_MAGIC) return -1;
    if (e->e_class   != 1)         return -1;
    if (e->e_type    != ET_EXEC)   return -1;
    if (e->e_machine != EM_386)    return -1;
    if (e->e_entry   == 0)         return -1;
    return 0;
}

proc_t *elf_load(const char *nm, const uint8_t *img, uint32_t sz) {
    if (elf_validate(img, sz) < 0) {
        printf("{FG(255,0,0)}elf: invalid binary\n");
        return 0;
    }

    const elf32_ehdr_t *eh = (const elf32_ehdr_t *)img;

    if (eh->e_phoff + (uint32_t)eh->e_phnum * eh->e_phentsz > sz) return 0;

    proc_t *p = proc_create_user(nm, 0, 0);
    if (!p) return 0;

    for (int i = 0; i < eh->e_phnum; i++) {
        const elf32_phdr_t *ph = (const elf32_phdr_t *)(img + eh->e_phoff + i * eh->e_phentsz);
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_memsz == 0)      continue;

        uint32_t va    = ph->p_vaddr;
        uint32_t memsz = ph->p_memsz;

        if (va < 0x1000 || va + memsz > ELF_USER_MAX) {
            printf("{FG(255,0,0)}elf: segment out of range 0x%x\n", va);
            proc_free(p);
            return 0;
        }

        uint32_t flags = PAGE_PRESENT | PAGE_USER;
        if (ph->p_flags & PF_W) flags |= PAGE_WRITE;

        uint32_t pg_start = va & ~0xFFFU;
        uint32_t pg_end   = (va + memsz + 0xFFFU) & ~0xFFFU;

        for (uint32_t v = pg_start; v < pg_end; v += 0x1000) {
            void *phys = pmm_malloc(0x1000);
            if (!phys) { proc_free(p); return 0; }
            memset(phys, 0, 0x1000);
            vmm_map_page_in(p->vm->directory, v, (uint32_t)phys, flags);
        }

        if (ph->p_filesz > 0) {
            uint32_t copy = ph->p_filesz < ph->p_memsz ? ph->p_filesz : ph->p_memsz;
            if (ph->p_offset + copy > sz) { proc_free(p); return 0; }

            const uint8_t *src = img + ph->p_offset;

            for (uint32_t off = 0; off < copy; off++) {
                uint32_t tv   = va + off;
                uint32_t pd_i = tv >> 22;
                uint32_t pt_i = (tv >> 12) & 0x3FF;
                uint32_t pde  = p->vm->directory->entries[pd_i];
                if (!(pde & PAGE_PRESENT)) continue;
                page_table_t *pt  = (page_table_t *)(pde & 0xFFFFF000U);
                uint32_t      pte = pt->entries[pt_i];
                if (!(pte & PAGE_PRESENT)) continue;
                uint8_t *phys_page = (uint8_t *)(pte & 0xFFFFF000U);
                phys_page[tv & 0xFFF] = src[off];
            }
        }
    }

    p->ctx.eip = eh->e_entry;
    p->ctx.cs  = UCODE_SEL;
    p->ctx.ds  = UDATA_SEL;
    p->ctx.ss  = UDATA_SEL;

    return p;
}
