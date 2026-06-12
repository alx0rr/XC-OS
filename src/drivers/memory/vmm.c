#include "../../include/memory/vmm.h"
#include "../../include/memory/pmm.h"
#include "../../include/graphics/vbe.h"
#include "../../include/text.h"
#include "../../lib/string.h"
static page_directory_t* kernel_directory = 0;
static page_directory_t* current_directory = 0;
static uint32_t total_mapped_pages = 0;
static uint32_t kernel_page_tables_phys[256];

#define VA_HEAP_START  KERNEL_HEAP_VIRT
#define VA_HEAP_PAGES  ((0xFF000000u - KERNEL_HEAP_VIRT) / PAGE_SIZE)
#define VA_BMP_WORDS   ((VA_HEAP_PAGES + 31) / 32)
static uint32_t va_bitmap[VA_BMP_WORDS];
static uint8_t  va_bitmap_init = 0;
static inline void invlpg(uint32_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}
static inline void load_page_directory(uint32_t phys) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(phys));
}
static inline void enable_paging_asm() {
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}
static page_table_t* vmm_get_page_table(page_directory_t* dir, uint32_t virt, uint8_t create) {
    uint32_t pd_index = virt >> 22;
    uint32_t pde = dir->entries[pd_index];

    if (pde & 0x80) {
        return 0;
    }

    if (!(pde & PAGE_PRESENT)) {
        if (!create) return 0;
        void* table_phys = pmm_malloc(PAGE_SIZE);
        if (!table_phys) return 0;
        dir->entries[pd_index] = ((uint32_t)table_phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_WRITE;
        page_table_t* table = (page_table_t*)table_phys;
        memset(table, 0, PAGE_SIZE);
        return table;
    }
    uint32_t table_phys = pde & 0xFFFFF000;
    return (page_table_t*)table_phys;
}
void vmm_init() {
    kernel_directory = (page_directory_t*)pmm_malloc(PAGE_SIZE);
    if (!kernel_directory) {
        printf("{FG(255,0,0)}VMM Init Failed: Cannot allocate page directory\n");
        return;
    }
    memset(kernel_directory, 0, PAGE_SIZE);
    for (uint32_t i = 0; i < 256; i++) {
        kernel_page_tables_phys[i] = 0;
    }

    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x10;
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

    uint32_t identity_end = 0xE0000000;
    for (uint32_t addr = 0; addr < identity_end; addr += 0x400000) {
        uint32_t pd_index = addr >> 22;
        kernel_directory->entries[pd_index] = addr | PAGE_PRESENT | PAGE_WRITE | 0x80;
    }

    uint32_t fb_start = 0xE0000000;
    uint32_t fb_end   = 0xE2000000;
    for (uint32_t addr = fb_start; addr < fb_end; addr += 0x400000) {
        uint32_t pd_index = addr >> 22;
        kernel_directory->entries[pd_index] = addr | PAGE_PRESENT | PAGE_WRITE | 0x80;
    }

    current_directory = kernel_directory;
}
void vmm_enable_paging() {
    if (!kernel_directory) {
        printf("{FG(255,0,0)}VMM Error: kernel_directory is NULL\n");
        return;
    }

    uint32_t fb = vbe_mode_info_data.framebuffer & ~0x3FFFFF;
    uint32_t fb_end = fb + 0x800000;
    for (uint32_t addr = fb; addr < fb_end; addr += 0x400000) {
        uint32_t pd_index = addr >> 22;
        kernel_directory->entries[pd_index] = addr | PAGE_PRESENT | PAGE_WRITE | 0x80;
    }

    uint32_t dir_phys = (uint32_t)kernel_directory;
    load_page_directory(dir_phys);
    enable_paging_asm();
    __asm__ volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
}
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    if (!kernel_directory) return;
    page_table_t* table = vmm_get_page_table(kernel_directory, virt, 1);
    if (!table) return;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    if (!(table->entries[pt_index] & PAGE_PRESENT)) {
        total_mapped_pages++;
    }
    table->entries[pt_index] = (phys & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;
    invlpg(virt);
}
void vmm_unmap_page(uint32_t virt) {
    if (!kernel_directory) return;
    page_table_t* table = vmm_get_page_table(kernel_directory, virt, 0);
    if (!table) return;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    if (table->entries[pt_index] & PAGE_PRESENT) {
        table->entries[pt_index] = 0;
        total_mapped_pages--;
        invlpg(virt);
    }
}
uint32_t vmm_get_physical(uint32_t virt) {
    if (!kernel_directory) return 0;
    uint32_t pd_index = virt >> 22;
    uint32_t pde = kernel_directory->entries[pd_index];
    if (!(pde & PAGE_PRESENT)) return 0;
    if (pde & 0x80) {
        return (pde & 0xFFC00000) | (virt & 0x3FFFFF);
    }
    page_table_t* table = vmm_get_page_table(kernel_directory, virt, 0);
    if (!table) return 0;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    if (!(table->entries[pt_index] & PAGE_PRESENT)) return 0;
    return (table->entries[pt_index] & 0xFFFFF000) | (virt & 0xFFF);
}
void* vmm_alloc_pages(uint32_t count, uint32_t flags) {
    if (count == 0) return 0;

    if (!va_bitmap_init) {
        for (uint32_t i = 0; i < VA_BMP_WORDS; i++) va_bitmap[i] = 0;
        va_bitmap_init = 1;
    }

    uint32_t run = 0, start = 0;
    for (uint32_t i = 0; i < VA_HEAP_PAGES; i++) {
        if (!(va_bitmap[i >> 5] & (1u << (i & 31)))) {
            if (run == 0) start = i;
            if (++run >= count) {
                uint32_t cr0;
                __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
                uint8_t paging_enabled = (cr0 & 0x80000000) ? 1 : 0;

                void* first_phys = 0;
                for (uint32_t k = 0; k < count; k++) {
                    void* phys = pmm_malloc(PAGE_SIZE);
                    if (!phys) {
                        for (uint32_t j = 0; j < k; j++) {
                            uint32_t vi = start + j;
                            va_bitmap[vi >> 5] &= ~(1u << (vi & 31));
                            uint32_t v = VA_HEAP_START + (vi * PAGE_SIZE);
                            uint32_t p = vmm_get_physical(v);
                            vmm_unmap_page(v);
                            pmm_free((void*)(p & 0xFFFFF000));
                        }
                        return 0;
                    }
                    if (k == 0) first_phys = phys;
                    uint32_t vi = start + k;
                    va_bitmap[vi >> 5] |= (1u << (vi & 31));
                    if (paging_enabled) {
                        uint32_t v = VA_HEAP_START + (vi * PAGE_SIZE);
                        vmm_map_page(v, (uint32_t)phys, flags | PAGE_PRESENT | PAGE_WRITE);
                        memset((void*)v, 0, PAGE_SIZE);
                    } else {
                        uint32_t pa = (uint32_t)phys;
                        vmm_map_page(pa, pa, flags | PAGE_PRESENT | PAGE_WRITE);
                        memset(phys, 0, PAGE_SIZE);
                    }
                }
                uint32_t cr0_2;
                __asm__ volatile("mov %%cr0, %0" : "=r"(cr0_2));
                if (!(cr0_2 & 0x80000000)) return first_phys;
                return (void*)(VA_HEAP_START + (start * PAGE_SIZE));
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

void vmm_free_pages(void* virt, uint32_t count) {
    if (!virt || count == 0) return;
    uint32_t virt_addr = (uint32_t)virt;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t v = virt_addr + (i * PAGE_SIZE);
        uint32_t phys = vmm_get_physical(v);
        if (phys) {
            vmm_unmap_page(v);
            pmm_free((void*)(phys & 0xFFFFF000));
        }
        if (v >= VA_HEAP_START) {
            uint32_t vi = (v - VA_HEAP_START) / PAGE_SIZE;
            if (vi < VA_HEAP_PAGES)
                va_bitmap[vi >> 5] &= ~(1u << (vi & 31));
        }
    }
}
vm_space_t* vmm_create_space() {
    vm_space_t* space = (vm_space_t*)pmm_malloc(sizeof(vm_space_t));
    if (!space) return 0;
    space->directory = (page_directory_t*)pmm_malloc(PAGE_SIZE);
    if (!space->directory) {
        pmm_free(space);
        return 0;
    }
    memset(space->directory, 0, PAGE_SIZE);
    for (uint32_t i = 768; i < 1024; i++) {
        space->directory->entries[i] = kernel_directory->entries[i];
    }
    space->virt_start = USER_HEAP_VIRT;
    space->virt_end = KERNEL_HEAP_VIRT;
    space->flags = PAGE_USER | PAGE_WRITE;
    return space;
}
void vmm_destroy_space(vm_space_t* space) {
    if (!space) return;
    for (uint32_t i = 0; i < 768; i++) {
        if (space->directory->entries[i] & PAGE_PRESENT) {
            uint32_t table_phys = space->directory->entries[i] & 0xFFFFF000;
            page_table_t* table = (page_table_t*)table_phys;
            for (uint32_t j = 0; j < 1024; j++) {
                if (table->entries[j] & PAGE_PRESENT) {
                    uint32_t page_phys = table->entries[j] & 0xFFFFF000;
                    pmm_free((void*)page_phys);
                }
            }
            pmm_free(table);
        }
    }
    pmm_free(space->directory);
    pmm_free(space);
}
void vmm_switch_space(vm_space_t* space) {
    if (!space || !space->directory) return;
    for (uint32_t i = 768; i < 1024; i++) {
        space->directory->entries[i] = kernel_directory->entries[i];
    }
    current_directory = space->directory;
    load_page_directory((uint32_t)space->directory);
}
void vmm_print_stats() {
    printf("VMM Stats:\n");
    printf("  Mapped pages: %u (%uMB)\n",
           total_mapped_pages,
           (total_mapped_pages * PAGE_SIZE) / (1024 * 1024));
    printf("  Page directory: 0x%x\n", (uint32_t)kernel_directory);
    printf("  Current directory: 0x%x\n", (uint32_t)current_directory);
    printf("  Paging: %s\n",
           (current_directory == kernel_directory) ? "kernel" : "user");
}
void vmm_flush_tlb() {
    load_page_directory((uint32_t)current_directory);
}
void vmm_flush_page(uint32_t virt) {
    invlpg(virt);
}
uint8_t vmm_is_mapped(uint32_t virt) {
    if (!kernel_directory) return 0;
    uint32_t pd_index = virt >> 22;
    uint32_t pde = kernel_directory->entries[pd_index];
    if (!(pde & PAGE_PRESENT)) return 0;
    if (pde & 0x80) return 1;
    page_table_t* table = vmm_get_page_table(kernel_directory, virt, 0);
    if (!table) return 0;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    return (table->entries[pt_index] & PAGE_PRESENT) ? 1 : 0;
}

void vmm_map_page_in(vm_space_t* space, uint32_t virt, uint32_t phys, uint32_t flags) {
    if (!space || !space->directory) return;
    page_directory_t* dir = space->directory;
    uint32_t pd_index = virt >> 22;
    uint32_t pde = dir->entries[pd_index];

    page_table_t* table;
    if (!(pde & PAGE_PRESENT)) {
        table = (page_table_t*)pmm_malloc(PAGE_SIZE);
        if (!table) return;
        memset(table, 0, PAGE_SIZE);
        dir->entries[pd_index] = ((uint32_t)table & 0xFFFFF000)
                                 | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        table = (page_table_t*)(pde & 0xFFFFF000);
    }

    uint32_t pt_index = (virt >> 12) & 0x3FF;
    table->entries[pt_index] = (phys & 0xFFFFF000)
                               | (flags & 0xFFF) | PAGE_PRESENT;
}

void* vmm_alloc_pages_in_space(vm_space_t* space, uint32_t count) {
    if (!space || count == 0) return 0;

    uint32_t total = (space->virt_end - space->virt_start) / PAGE_SIZE;
    uint32_t run = 0, start = 0;
    uint8_t found = 0;

    for (uint32_t i = 0; i < total; i++) {
        uint32_t v = space->virt_start + i * PAGE_SIZE;
        uint32_t pd_index = v >> 22;
        uint32_t pt_index = (v >> 12) & 0x3FF;
        uint32_t pde = space->directory->entries[pd_index];
        uint8_t used = 0;
        if (pde & PAGE_PRESENT) {
            page_table_t* table = (page_table_t*)(pde & 0xFFFFF000);
            used = (table->entries[pt_index] & PAGE_PRESENT) ? 1 : 0;
        }
        if (!used) {
            if (run == 0) start = i;
            if (++run >= count) { found = 1; break; }
        } else {
            run = 0;
        }
    }

    if (!found) return 0;

    for (uint32_t k = 0; k < count; k++) {
        void* phys = pmm_malloc(PAGE_SIZE);
        if (!phys) {
            for (uint32_t j = 0; j < k; j++) {
                uint32_t v = space->virt_start + (start + j) * PAGE_SIZE;
                uint32_t pd_index = v >> 22;
                uint32_t pt_index = (v >> 12) & 0x3FF;
                uint32_t pde = space->directory->entries[pd_index];
                if (pde & PAGE_PRESENT) {
                    page_table_t* table = (page_table_t*)(pde & 0xFFFFF000);
                    uint32_t p = table->entries[pt_index] & 0xFFFFF000;
                    table->entries[pt_index] = 0;
                    pmm_free((void*)p);
                }
            }
            return 0;
        }
        memset(phys, 0, PAGE_SIZE);
        uint32_t v = space->virt_start + (start + k) * PAGE_SIZE;
        vmm_map_page_in(space, v, (uint32_t)phys, space->flags);
    }

    return (void*)(space->virt_start + start * PAGE_SIZE);
}

void vmm_free_pages_in_space(vm_space_t* space, void* virt, uint32_t count) {
    if (!space || !virt || count == 0) return;
    uint32_t virt_addr = (uint32_t)virt;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t v = virt_addr + i * PAGE_SIZE;
        uint32_t pd_index = v >> 22;
        uint32_t pt_index = (v >> 12) & 0x3FF;
        uint32_t pde = space->directory->entries[pd_index];
        if (!(pde & PAGE_PRESENT)) continue;
        page_table_t* table = (page_table_t*)(pde & 0xFFFFF000);
        if (!(table->entries[pt_index] & PAGE_PRESENT)) continue;
        uint32_t phys = table->entries[pt_index] & 0xFFFFF000;
        table->entries[pt_index] = 0;
        pmm_free((void*)phys);
    }
}
