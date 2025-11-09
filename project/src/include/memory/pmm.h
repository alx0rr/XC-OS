#ifndef PMM_H
#define PMM_H
#include <stdint.h>

#define MMAP_ADDR 0x8000
#define MMAP_COUNT_ADDR 0x7FFC
#define PAGE_SIZE 4096
#define HEAP_START 0x100000
#define HEAP_SIZE 0x1000000

#define MMAP_TYPE_USABLE 1
#define MMAP_TYPE_RESERVED 2
#define MMAP_TYPE_ACPI_RECLAIMABLE 3
#define MMAP_TYPE_ACPI_NVS 4
#define MMAP_TYPE_BAD 5

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
} mmap_entry_t;

typedef struct {
    uint32_t count;
    mmap_entry_t* entries;
} memory_map_t;

typedef struct block_header {
    uint32_t size;
    uint8_t free;
    struct block_header* next;
    struct block_header* prev;
} block_header_t;

void init_pmm();
memory_map_t get_mmap();
void* pmm_malloc(uint32_t size);
void pmm_free(void* ptr);
void pmm_print_stats();
void pmm_defragment();

#endif