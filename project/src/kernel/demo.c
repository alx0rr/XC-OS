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

    uint32_t max_each = free_mem / blocks;

    if (max_each < min_block_size) {
        printf("{FG(255,0,0)}Not enough free RAM to run the stress test (need at least %u MB total free)\n",
               (min_block_size * blocks) / (1024 * 1024));
        return;
    }

    uint32_t alloc_each = (uint32_t)desired_each;
    if (alloc_each > max_each) {
        alloc_each = max_each;
        printf("{FG(255,200,0)}Adjusting block size to %u MB per block to fit free RAM\n",
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
            break;
        }
        allocated++;
        printf("{FG(0,255,0)}Block %d allocated\n", i);
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