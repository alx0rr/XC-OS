#include "../../include/memory/vmm.h"
#include "../../include/memory/pmm.h"
#include "../../include/text.h"
#include "../../lib/string.h"

static page_directory_t* kernel_directory = 0;
static page_directory_t* current_directory = 0;
static uint32_t total_mapped_pages = 0;
static uint32_t kernel_page_tables_phys[256];

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
    
    if (!(dir->entries[pd_index] & PAGE_PRESENT)) {
        if (!create) return 0;
        
        void* table_phys = pmm_malloc(PAGE_SIZE);
        if (!table_phys) return 0;
        
        // pmm_malloc already clears the memory, no need for memset
        dir->entries[pd_index] = ((uint32_t)table_phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_WRITE;
        
        return (page_table_t*)table_phys;
    }
    
    uint32_t table_phys = dir->entries[pd_index] & 0xFFFFF000;
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
    
    // Identity map first 256MB (covers kernel, stack, VBE info, etc)
    uint32_t identity_end = 0x10000000; // 256MB
    for (uint32_t addr = 0; addr < identity_end; addr += PAGE_SIZE) {
        vmm_map_page(addr, addr, PAGE_PRESENT | PAGE_WRITE);
    }
    
    // Also identity map high memory region where framebuffer might be located
    // Common framebuffer locations: 0xE0000000+ (QEMU/VirtualBox)
    // Map 32MB starting from 0xE0000000 (enough for most framebuffers)
    uint32_t fb_start = 0xE0000000;
    uint32_t fb_end = 0xE2000000; // 32MB instead of 256MB
    for (uint32_t addr = fb_start; addr < fb_end; addr += PAGE_SIZE) {
        vmm_map_page(addr, addr, PAGE_PRESENT | PAGE_WRITE);
    }
    
    current_directory = kernel_directory;
    
    printf("{FG(0,255,0)}VMM initialized: %u pages mapped\n", total_mapped_pages);
}

void vmm_enable_paging() {
    if (!kernel_directory) {
        printf("{FG(255,0,0)}VMM Error: kernel_directory is NULL\n");
        return;
    }
    
    uint32_t dir_phys = (uint32_t)kernel_directory;
    
    // Load page directory and enable paging
    load_page_directory(dir_phys);
    enable_paging_asm();
    
    // Flush TLB to ensure all mappings are active
    __asm__ volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
    
    // Now it's safe to print - framebuffer should be identity mapped
    printf("{FG(0,255,0)}Paging enabled at 0x%x\n", dir_phys);
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
    
    page_table_t* table = vmm_get_page_table(kernel_directory, virt, 0);
    if (!table) return 0;
    
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    
    if (!(table->entries[pt_index] & PAGE_PRESENT)) return 0;
    
    return (table->entries[pt_index] & 0xFFFFF000) | (virt & 0xFFF);
}

void* vmm_alloc_pages(uint32_t count, uint32_t flags) {
    if (count == 0) return 0;
    
    static uint32_t next_virt = KERNEL_HEAP_VIRT;
    uint32_t start_virt = next_virt;
    
    // Check if paging is enabled by reading CR0
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    uint8_t paging_enabled = (cr0 & 0x80000000) ? 1 : 0;
    
    for (uint32_t i = 0; i < count; i++) {
        void* phys = pmm_malloc(PAGE_SIZE);
        if (!phys) {
            for (uint32_t j = 0; j < i; j++) {
                uint32_t v = start_virt + (j * PAGE_SIZE);
                uint32_t p = vmm_get_physical(v);
                vmm_unmap_page(v);
                pmm_free((void*)(p & 0xFFFFF000));
            }
            return 0;
        }
        
        uint32_t v = start_virt + (i * PAGE_SIZE);
        vmm_map_page(v, (uint32_t)phys, flags | PAGE_PRESENT | PAGE_WRITE);
        
        // Clear memory - use physical address if paging is disabled
        if (paging_enabled) {
            memset((void*)v, 0, PAGE_SIZE);
        } else {
            memset(phys, 0, PAGE_SIZE);
        }
    }
    
    next_virt += (count * PAGE_SIZE);
    
    // If paging is not enabled, return physical address instead
    if (!paging_enabled) {
        // Get the physical address of the first allocated page
        uint32_t phys_start = vmm_get_physical(start_virt);
        return (void*)(phys_start & 0xFFFFF000);
    }
    
    return (void*)start_virt;
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
    
    page_table_t* table = vmm_get_page_table(kernel_directory, virt, 0);
    if (!table) return 0;
    
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    return (table->entries[pt_index] & PAGE_PRESENT) ? 1 : 0;
}
