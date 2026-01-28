#include <stdint.h>
#include "../include/text.h"
#include "../include/memory/pmm.h"
#include "../include/input/keyboard.h"

void memory_stress_test(void) {
    printf("{FG(100,200,255)}=== Memory Stress Test ===\n\n");

    uint32_t total_mem = pmm_get_total_memory();
    uint32_t free_mem  = pmm_get_free_memory();
    uint32_t used_mem  = pmm_get_used_memory();

    printf("Total RAM: %u MB\n", total_mem / (1024 * 1024));
    printf("Used RAM:  %u MB\n", used_mem / (1024 * 1024));
    printf("Free RAM:  %u MB\n\n", free_mem / (1024 * 1024));

    const int blocks = 10;
    uint32_t min_block_size = 1 * 1024 * 1024;
    uint64_t desired_each = 50ULL * 1024ULL * 1024ULL;

    uint64_t min_required_total = (uint64_t)min_block_size * blocks;
    
    if (free_mem < min_required_total) {
        printf("{FG(255,0,0)}Not enough free RAM to run the stress test\n");
        printf("{FG(255,0,0)}Required: %u MB, Available: %u MB\n",
               (uint32_t)(min_required_total / (1024 * 1024)),
               free_mem / (1024 * 1024));
        return;
    }

    uint32_t alloc_each = (uint32_t)desired_each;
    uint64_t desired_total = desired_each * blocks;
    
    if (desired_total > free_mem) {
        alloc_each = free_mem / blocks;
        alloc_each = (alloc_each / (1024 * 1024)) * (1024 * 1024);
        
        if (alloc_each < min_block_size) {
            alloc_each = min_block_size;
        }
        
        printf("{FG(255,200,0)}Adjusting: can only allocate %u MB per block\n",
               alloc_each / (1024 * 1024));
    }

    void* ptrs[blocks];
    for (int i = 0; i < blocks; i++) ptrs[i] = 0;

    printf("{FG(255,255,0)}Allocating %d blocks of %u MB each...\n", blocks, alloc_each / (1024 * 1024));

    int allocated = 0;
    for (int i = 0; i < blocks; i++) {
        ptrs[i] = pmm_malloc(alloc_each);
        if (!ptrs[i]) {
            printf("{FG(255,0,0)}Allocation failed at block %d\n", i);
            printf("{FG(255,100,0)}Free memory before this allocation: %u MB\n", 
                   pmm_get_free_memory() / (1024 * 1024));
            break;
        }
        allocated++;
        printf("{FG(0,255,0)}Block %d allocated at 0x%x\n", i, (uint32_t)ptrs[i]);
    }

    printf("\n");
    pmm_print_stats();

    printf("\n{FG(255,255,0)}Press any key to free allocated blocks...\n");
    keyboard_input();

    printf("{FG(255,255,0)}Freeing allocated blocks...\n\n");
    for (int i = 0; i < allocated; i++) {
        if (ptrs[i]) {
            pmm_free(ptrs[i]);
            ptrs[i] = 0;
        }
    }

    pmm_defragment();
    pmm_print_stats();

    printf("\n{FG(100,255,100)}Memory stress test finished.\n");
}
