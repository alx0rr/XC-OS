#include "../../include/memory/pmm.h"
#include "../../include/text.h"

#define MAX_ORDER 20
#define MIN_BLOCK_SIZE 4096

typedef struct free_block {
    struct free_block* next;
} free_block_t;

static memory_map_t mmap;
static free_block_t* free_lists[MAX_ORDER];
static uint32_t total_memory = 0;
static uint32_t used_memory = 0;
static uint8_t* bitmap = (uint8_t*)BITMAP_ADDR;
static uint32_t bitmap_size = 0;

static inline void set_bit(uint32_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void clear_bit(uint32_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline uint8_t get_bit(uint32_t bit) {
    return (bitmap[bit / 8] >> (bit % 8)) & 1;
}

static uint32_t get_order(uint32_t size) {
    uint32_t order = 0;
    uint32_t block_size = MIN_BLOCK_SIZE;
    
    while (block_size < size && order < MAX_ORDER) {
        block_size *= 2;
        order++;
    }
    
    return order;
}

static uint32_t order_to_size(uint32_t order) {
    return MIN_BLOCK_SIZE << order;
}

static uint32_t addr_to_block(void* addr) {
    return ((uint32_t)addr - HEAP_START) / MIN_BLOCK_SIZE;
}

static void* block_to_addr(uint32_t block) {
    return (void*)(HEAP_START + block * MIN_BLOCK_SIZE);
}

static uint32_t get_buddy(uint32_t block, uint32_t order) {
    return block ^ (1 << order);
}

void init_pmm() {
    mmap.count = *(uint32_t*)MMAP_COUNT_ADDR;
    mmap.entries = (mmap_entry_t*)MMAP_ADDR;
    
    for (uint32_t i = 0; i < MAX_ORDER; i++) {
        free_lists[i] = 0;
    }
    
    total_memory = 0;
    used_memory = 0;
    
    for (uint32_t i = 0; i < mmap.count; i++) {
        if (mmap.entries[i].type == MMAP_TYPE_USABLE) {
            uint64_t base = mmap.entries[i].base_addr;
            uint64_t length = mmap.entries[i].length;
            
            if (base < HEAP_START) {
                if (base + length <= HEAP_START) continue;
                length -= (HEAP_START - base);
                base = HEAP_START;
            }
            
            uint32_t start_addr = (uint32_t)base;
            uint32_t end_addr = (uint32_t)(base + length);
            
            if (start_addr >= HEAP_START + HEAP_SIZE) continue;
            if (end_addr > HEAP_START + HEAP_SIZE) {
                end_addr = HEAP_START + HEAP_SIZE;
            }
            
            total_memory += (end_addr - start_addr);
            
            start_addr = (start_addr + MIN_BLOCK_SIZE - 1) & ~(MIN_BLOCK_SIZE - 1);
            end_addr = end_addr & ~(MIN_BLOCK_SIZE - 1);
            
            while (start_addr < end_addr) {
                uint32_t size = end_addr - start_addr;
                uint32_t order = MAX_ORDER - 1;
                
                while (order > 0 && order_to_size(order) > size) {
                    order--;
                }
                
                while (order > 0 && (start_addr & ((1 << (order + 12)) - 1))) {
                    order--;
                }
                
                free_block_t* block = (free_block_t*)start_addr;
                block->next = free_lists[order];
                free_lists[order] = block;
                
                start_addr += order_to_size(order);
            }
        }
    }
    
    bitmap_size = (HEAP_SIZE / MIN_BLOCK_SIZE + 7) / 8;
    for (uint32_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0;
    }
}

memory_map_t get_mmap() {
    return mmap;
}

void* pmm_malloc(uint32_t size) {
    if (size == 0) return 0;
    
    uint32_t order = get_order(size);
    if (order >= MAX_ORDER) return 0;
    
    uint32_t current_order = order;
    
    while (current_order < MAX_ORDER && !free_lists[current_order]) {
        current_order++;
    }
    
    if (current_order >= MAX_ORDER) return 0;
    
    free_block_t* block = free_lists[current_order];
    free_lists[current_order] = block->next;
    
    while (current_order > order) {
        current_order--;
        
        uint32_t block_num = addr_to_block(block);
        uint32_t buddy_num = get_buddy(block_num, current_order);
        free_block_t* buddy = block_to_addr(buddy_num);
        
        buddy->next = free_lists[current_order];
        free_lists[current_order] = buddy;
        
        if (buddy < block) {
            block = buddy;
        }
    }
    
    uint32_t block_num = addr_to_block(block);
    uint32_t blocks_count = 1 << order;
    
    for (uint32_t i = 0; i < blocks_count; i++) {
        set_bit(block_num + i);
    }
    
    used_memory += order_to_size(order);
    
    for (uint32_t i = 0; i < order_to_size(order); i++) {
        ((uint8_t*)block)[i] = 0;
    }
    
    return block;
}

void pmm_free(void* ptr) {
    if (!ptr || (uint32_t)ptr < HEAP_START || (uint32_t)ptr >= HEAP_START + HEAP_SIZE) {
        return;
    }
    
    uint32_t block_num = addr_to_block(ptr);
    
    if (!get_bit(block_num)) return;
    
    uint32_t order = 0;
    while (order < MAX_ORDER - 1 && get_bit(block_num + (1 << order))) {
        order++;
    }
    
    uint32_t blocks_count = 1 << order;
    for (uint32_t i = 0; i < blocks_count; i++) {
        clear_bit(block_num + i);
    }
    
    used_memory -= order_to_size(order);
    
    free_block_t* block = (free_block_t*)ptr;
    
    while (order < MAX_ORDER - 1) {
        uint32_t buddy_num = get_buddy(block_num, order);
        
        if (get_bit(buddy_num)) break;
        
        free_block_t* buddy = block_to_addr(buddy_num);
        
        free_block_t** list = &free_lists[order];
        free_block_t* prev = 0;
        
        while (*list && *list != buddy) {
            prev = *list;
            list = &((*list)->next);
        }
        
        if (!*list) break;
        
        if (prev) {
            prev->next = buddy->next;
        } else {
            free_lists[order] = buddy->next;
        }
        
        if (buddy_num < block_num) {
            block = buddy;
            block_num = buddy_num;
        }
        
        order++;
    }
    
    block->next = free_lists[order];
    free_lists[order] = block;
}

void pmm_print_stats() {
    uint32_t free_memory = total_memory - used_memory;
    uint32_t percent_used = total_memory > 0 ? (used_memory * 100) / total_memory : 0;
    uint32_t percent_free = total_memory > 0 ? (free_memory * 100) / total_memory : 0;
    
    uint32_t free_blocks[MAX_ORDER] = {0};
    
    for (uint32_t order = 0; order < MAX_ORDER; order++) {
        free_block_t* block = free_lists[order];
        while (block) {
            free_blocks[order]++;
            block = block->next;
        }
    }
    
    printf("Mem: %uMB used/%uMB free (%u%%/%u%%)\n",
           used_memory / 1048576, free_memory / 1048576,
           percent_used, percent_free);
    
    printf("Total: %uMB\n", total_memory / 1048576);
}

void pmm_defragment() {
}

uint32_t pmm_get_total_memory() {
    return total_memory;
}

uint32_t pmm_get_free_memory() {
    return total_memory - used_memory;
}

uint32_t pmm_get_used_memory() {
    return used_memory;
}
