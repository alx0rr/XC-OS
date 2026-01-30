#include "../../include/memory/pmm.h"
#include "../../include/text.h"
#define MAX_ORDER 20
#define MIN_BLOCK_SIZE 4096
typedef struct free_block {
    struct free_block* next;
} free_block_t;
typedef struct {
    volatile uint32_t lock;
} spinlock_t;
static memory_map_t mmap;
static free_block_t* free_lists[MAX_ORDER];
static spinlock_t list_locks[MAX_ORDER];
static uint32_t total_memory = 0;
static uint32_t used_memory = 0;
static uint8_t* bitmap = (uint8_t*)BITMAP_ADDR;
static uint32_t bitmap_size = 0;
static uint8_t* order_map = (uint8_t*)(BITMAP_ADDR + 0x100000);
static uint32_t actual_heap_size = 0;
static uint32_t max_blocks = 0;
static inline void spin_lock(spinlock_t* lock) {
    while (__sync_lock_test_and_set(&lock->lock, 1)) {
        while (lock->lock) {
            __asm__ volatile("pause");
        }
    }
}
static inline void spin_unlock(spinlock_t* lock) {
    __sync_lock_release(&lock->lock);
}
static inline void set_bit_atomic(uint32_t bit) {
    if (bit >= max_blocks) return;
    __asm__ volatile(
        "lock orb %1, %0"
        : "+m"(bitmap[bit / 8])
        : "r"((uint8_t)(1 << (bit % 8)))
        : "cc", "memory"
    );
}
static inline void clear_bit_atomic(uint32_t bit) {
    if (bit >= max_blocks) return;
    __asm__ volatile(
        "lock andb %1, %0"
        : "+m"(bitmap[bit / 8])
        : "r"((uint8_t)~(1 << (bit % 8)))
        : "cc", "memory"
    );
}
static inline uint8_t get_bit(uint32_t bit) {
    if (bit >= max_blocks) return 1;
    return (bitmap[bit / 8] >> (bit % 8)) & 1;
}
static inline void set_order(uint32_t block, uint8_t order) {
    if (block >= max_blocks) return;
    order_map[block] = order;
}
static inline uint8_t get_order_stored(uint32_t block) {
    if (block >= max_blocks) return 0xFF;
    return order_map[block];
}
static inline void fast_memset(void* dest, uint8_t value, uint32_t size) {
    if (size == 0) return;
    uint8_t* ptr = (uint8_t*)dest;
    uint32_t dwords = size / 4;
    uint32_t value32 = value | (value << 8) | (value << 16) | (value << 24);
    if (dwords > 0) {
        __asm__ volatile(
            "cld\n\t"
            "rep stosl"
            : "+D"(ptr), "+c"(dwords)
            : "a"(value32)
            : "memory"
        );
    }
    uint32_t remainder = size % 4;
    for (uint32_t i = 0; i < remainder; i++) {
        ptr[i] = value;
    }
}
static inline uint32_t get_order(uint32_t size) {
    if (size <= MIN_BLOCK_SIZE) return 0;
    if (size == 0) return 0;
    uint32_t order = 0;
    uint32_t block_size = MIN_BLOCK_SIZE;
    while (block_size < size && order < MAX_ORDER - 1) {
        block_size <<= 1;
        order++;
    }
    return order;
}
static inline uint32_t order_to_size(uint32_t order) {
    if (order >= MAX_ORDER) return 0;
    return MIN_BLOCK_SIZE << order;
}
static inline uint32_t addr_to_block(void* addr) {
    uint32_t offset = (uint32_t)addr - HEAP_START;
    return offset / MIN_BLOCK_SIZE;
}
static inline void* block_to_addr(uint32_t block) {
    if (block >= max_blocks) return 0;
    return (void*)(HEAP_START + (block * MIN_BLOCK_SIZE));
}
static inline uint32_t get_buddy(uint32_t block, uint32_t order) {
    if (order >= MAX_ORDER) return 0xFFFFFFFF;
    uint32_t buddy = block ^ (1 << order);
    if (buddy >= max_blocks) return 0xFFFFFFFF;
    return buddy;
}
static inline uint8_t is_aligned(uint32_t addr, uint32_t alignment) {
    return (addr & (alignment - 1)) == 0;
}
static inline uint8_t is_buddy_free(uint32_t buddy_num, uint32_t order) {
    uint32_t blocks_in_buddy = 1 << order;
    if (buddy_num >= max_blocks) return 0;
    if (buddy_num + blocks_in_buddy > max_blocks) return 0;
    for (uint32_t i = 0; i < blocks_in_buddy; i++) {
        if (get_bit(buddy_num + i)) {
            return 0;
        }
    }
    return 1;
}
static void add_block_to_list(uint32_t order, void* addr) {
    if (order >= MAX_ORDER || !addr) return;
    free_block_t* block = (free_block_t*)addr;
    spin_lock(&list_locks[order]);
    block->next = free_lists[order];
    free_lists[order] = block;
    spin_unlock(&list_locks[order]);
}
static void* remove_block_from_list(uint32_t order) {
    if (order >= MAX_ORDER) return 0;
    spin_lock(&list_locks[order]);
    free_block_t* block = free_lists[order];
    if (block) {
        free_lists[order] = block->next;
    }
    spin_unlock(&list_locks[order]);
    return block;
}
static uint8_t remove_specific_block(uint32_t order, void* target) {
    if (order >= MAX_ORDER || !target) return 0;
    spin_lock(&list_locks[order]);
    free_block_t** list = &free_lists[order];
    free_block_t* prev = 0;
    uint8_t found = 0;
    while (*list) {
        if (*list == target) {
            if (prev) {
                prev->next = (*list)->next;
            } else {
                free_lists[order] = (*list)->next;
            }
            found = 1;
            break;
        }
        prev = *list;
        list = &((*list)->next);
    }
    spin_unlock(&list_locks[order]);
    return found;
}
void init_pmm() {
    mmap.count = *(uint32_t*)MMAP_COUNT_ADDR;
    mmap.entries = (mmap_entry_t*)MMAP_ADDR;
    printf("PMM Init: mmap.count = %u\n", mmap.count);
    for (uint32_t i = 0; i < MAX_ORDER; i++) {
        free_lists[i] = 0;
        list_locks[i].lock = 0;
    }
    total_memory = 0;
    used_memory = 0;
    actual_heap_size = 0;
    uint32_t max_end = HEAP_START;
    uint32_t total_usable = 0;
    for (uint32_t i = 0; i < mmap.count; i++) {
        printf("Region %u: base=0x%x%x len=0x%x%x type=%u\n",
               i,
               (uint32_t)(mmap.entries[i].base_addr >> 32),
               (uint32_t)(mmap.entries[i].base_addr),
               (uint32_t)(mmap.entries[i].length >> 32),
               (uint32_t)(mmap.entries[i].length),
               mmap.entries[i].type);
        if (mmap.entries[i].type == MMAP_TYPE_USABLE) {
            uint64_t base = mmap.entries[i].base_addr;
            uint64_t length = mmap.entries[i].length;
            if (base + length <= HEAP_START) continue;
            if (base < HEAP_START) {
                if (length <= (HEAP_START - base)) continue;
                length -= (HEAP_START - base);
                base = HEAP_START;
            }
            if (base >= (uint64_t)HEAP_START + HEAP_SIZE) continue;
            uint32_t start_addr = (uint32_t)base;
            uint64_t end64 = base + length;
            uint32_t end_addr;
            if (end64 > (uint64_t)HEAP_START + HEAP_SIZE) {
                end_addr = HEAP_START + HEAP_SIZE;
            } else {
                end_addr = (uint32_t)end64;
            }
            if (end_addr <= start_addr) continue;
            if (end_addr > max_end) max_end = end_addr;
            uint32_t region_size = end_addr - start_addr;
            total_usable += region_size;
            uint32_t aligned_start = (start_addr + MIN_BLOCK_SIZE - 1) & ~(MIN_BLOCK_SIZE - 1);
            uint32_t aligned_end = end_addr & ~(MIN_BLOCK_SIZE - 1);
            if (aligned_end <= aligned_start) continue;
            total_memory += (aligned_end - aligned_start);
            while (aligned_start < aligned_end) {
                uint32_t size = aligned_end - aligned_start;
                uint32_t order = MAX_ORDER - 1;
                while (order > 0 && order_to_size(order) > size) {
                    order--;
                }
                while (order > 0 && !is_aligned(aligned_start, order_to_size(order))) {
                    order--;
                }
                uint32_t block_size = order_to_size(order);
                if (block_size == 0 || block_size > size) break;
                add_block_to_list(order, (void*)aligned_start);
                aligned_start += block_size;
            }
        }
    }
    if (total_usable > 0) {
        actual_heap_size = (max_end > HEAP_START) ? (max_end - HEAP_START) : 0;
    } else {
        printf("[ERROR] No usable memory regions from E820!\n");
        printf("[ERROR] Check QEMU/BIOS E820 implementation\n");
        actual_heap_size = 0;
        total_memory = 0;
    }
    max_blocks = actual_heap_size > 0 ? ((actual_heap_size + MIN_BLOCK_SIZE - 1) / MIN_BLOCK_SIZE) : 0;
    bitmap_size = (max_blocks + 7) / 8;
    if (max_blocks > 0) {
        fast_memset(bitmap, 0, bitmap_size);
        fast_memset(order_map, 0, max_blocks);
    }
    printf("PMM initialized: total_memory=%uMB, max_blocks=%u\n",
           total_memory / (1024*1024), max_blocks);
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
    free_block_t* block = remove_block_from_list(current_order);
    if (!block) return 0;
    uint32_t block_num = addr_to_block(block);
    if (block_num >= max_blocks) {
        add_block_to_list(current_order, block);
        return 0;
    }
    while (current_order > order) {
        current_order--;
        uint32_t buddy_num = get_buddy(block_num, current_order);
        if (buddy_num == 0xFFFFFFFF || buddy_num >= max_blocks) {
            add_block_to_list(current_order + 1, block);
            return 0;
        }
        free_block_t* buddy = block_to_addr(buddy_num);
        if (!buddy) {
            add_block_to_list(current_order + 1, block);
            return 0;
        }
        add_block_to_list(current_order, buddy);
        if (buddy_num < block_num) {
            block = buddy;
            block_num = buddy_num;
        }
    }
    uint32_t blocks_count = 1 << order;
    if (block_num + blocks_count > max_blocks) {
        add_block_to_list(order, block);
        return 0;
    }
    for (uint32_t i = 0; i < blocks_count; i++) {
        set_bit_atomic(block_num + i);
    }
    set_order(block_num, (uint8_t)order);
    uint32_t alloc_size = order_to_size(order);
    if (alloc_size > 0) {
        __sync_fetch_and_add(&used_memory, alloc_size);
    }
    fast_memset(block, 0, alloc_size);
    return block;
}
void* pmm_calloc(uint32_t size) {
    return pmm_malloc(size);
}
void pmm_free(void* ptr) {
    if (!ptr) return;
    uint32_t addr = (uint32_t)ptr;
    if (addr < HEAP_START || addr >= HEAP_START + actual_heap_size) {
        return;
    }
    if (!is_aligned(addr, MIN_BLOCK_SIZE)) {
        return;
    }
    uint32_t block_num = addr_to_block(ptr);
    if (block_num >= max_blocks) return;
    if (!get_bit(block_num)) return;
    uint8_t order = get_order_stored(block_num);
    if (order == 0xFF || order >= MAX_ORDER) {
        return;
    }
    uint32_t blocks_count = 1 << order;
    if (block_num + blocks_count > max_blocks) {
        blocks_count = max_blocks - block_num;
    }
    for (uint32_t i = 0; i < blocks_count; i++) {
        clear_bit_atomic(block_num + i);
    }
    set_order(block_num, 0);
    uint32_t freed_size = order_to_size(order);
    if (freed_size > 0) {
        __sync_fetch_and_sub(&used_memory, freed_size);
    }
    free_block_t* block = (free_block_t*)ptr;
    while (order < MAX_ORDER - 1) {
        uint32_t buddy_num = get_buddy(block_num, order);
        if (buddy_num == 0xFFFFFFFF || buddy_num >= max_blocks) break;
        if (!is_buddy_free(buddy_num, order)) break;
        free_block_t* buddy = block_to_addr(buddy_num);
        if (!buddy) break;
        if (!remove_specific_block(order, buddy)) break;
        if (buddy_num < block_num) {
            block = buddy;
            block_num = buddy_num;
        }
        order++;
    }
    add_block_to_list(order, block);
}
void pmm_print_stats() {
    uint32_t used = used_memory;
    uint32_t total = total_memory;
    uint32_t free_memory = (total > used) ? (total - used) : 0;
    uint32_t percent_used = total > 0 ? (used * 100) / total : 0;
    uint32_t percent_free = total > 0 ? (free_memory * 100) / total : 0;
    printf("Mem: %uMB used/%uMB free (%u%%/%u%%)\n",
           used / 1048576, free_memory / 1048576,
           percent_used, percent_free);
    printf("Total: %uMB\n", total / 1048576);
}
void pmm_defragment() {
    for (uint32_t order = 0; order < MAX_ORDER - 1; order++) {
        spin_lock(&list_locks[order]);
        free_block_t* current = free_lists[order];
        while (current) {
            uint32_t block_num = addr_to_block(current);
            uint32_t buddy_num = get_buddy(block_num, order);
            if (buddy_num == 0xFFFFFFFF || buddy_num >= max_blocks) {
                current = current->next;
                continue;
            }
            if (!is_buddy_free(buddy_num, order)) {
                current = current->next;
                continue;
            }
            free_block_t* buddy = block_to_addr(buddy_num);
            if (!buddy) {
                current = current->next;
                continue;
            }
            free_block_t** list = &free_lists[order];
            free_block_t* prev_curr = 0;
            while (*list && *list != current) {
                prev_curr = *list;
                list = &((*list)->next);
            }
            if (*list) {
                if (prev_curr) prev_curr->next = current->next;
                else free_lists[order] = current->next;
            }
            list = &free_lists[order];
            free_block_t* prev_buddy = 0;
            while (*list && *list != buddy) {
                prev_buddy = *list;
                list = &((*list)->next);
            }
            if (*list) {
                if (prev_buddy) prev_buddy->next = buddy->next;
                else free_lists[order] = buddy->next;
            }
            free_block_t* merged = (buddy_num < block_num) ? buddy : current;
            spin_unlock(&list_locks[order]);
            add_block_to_list(order + 1, merged);
            spin_lock(&list_locks[order]);
            current = free_lists[order];
        }
        spin_unlock(&list_locks[order]);
    }
}
uint32_t pmm_get_total_memory() {
    return total_memory;
}
uint32_t pmm_get_free_memory() {
    uint32_t used = used_memory;
    uint32_t total = total_memory;
    return (total > used) ? (total - used) : 0;
}
uint32_t pmm_get_used_memory() {
    return used_memory;
}
