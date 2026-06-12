#ifndef VMM_H
#define VMM_H
#include <stdint.h>
#define PAGE_SIZE 4096
#define PAGE_PRESENT   0x001
#define PAGE_WRITE     0x002
#define PAGE_USER      0x004
#define PAGE_ACCESSED  0x020
#define PAGE_DIRTY     0x040
#define KERNEL_HEAP_VIRT  0xC0000000
#define USER_HEAP_VIRT    0x40000000
#define KERNEL_STACK_VIRT 0xF0000000
typedef struct {
    uint32_t entries[1024];
} __attribute__((aligned(4096))) page_directory_t;
typedef struct {
    uint32_t entries[1024];
} __attribute__((aligned(4096))) page_table_t;
typedef struct {
    page_directory_t* directory;
    uint32_t virt_start;
    uint32_t virt_end;
    uint32_t flags;
} vm_space_t;
void vmm_init();
void vmm_enable_paging();
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void vmm_unmap_page(uint32_t virt);
uint32_t vmm_get_physical(uint32_t virt);
void* vmm_alloc_pages(uint32_t count, uint32_t flags);
void vmm_free_pages(void* virt, uint32_t count);
vm_space_t* vmm_create_space();
void vmm_destroy_space(vm_space_t* space);
void vmm_switch_space(vm_space_t* space);
void vmm_print_stats();
void vmm_flush_tlb();
void vmm_flush_page(uint32_t virt);
uint8_t vmm_is_mapped(uint32_t virt);
void vmm_map_page_in(vm_space_t* space, uint32_t virt, uint32_t phys, uint32_t flags);
void* vmm_alloc_pages_in_space(vm_space_t* space, uint32_t count);
void vmm_free_pages_in_space(vm_space_t* space, void* virt, uint32_t count);
#endif
