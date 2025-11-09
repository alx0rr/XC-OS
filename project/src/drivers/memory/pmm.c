#include "../../include/memory/pmm.h"
#include "../../include/text.h"

static memory_map_t mmap;
static block_header_t* heap_start = (block_header_t*)HEAP_START;
static uint32_t heap_end = HEAP_START;
static uint32_t total_heap_size = 0;

void init_pmm() {
    mmap.count = *(uint32_t*)MMAP_COUNT_ADDR;
    mmap.entries = (mmap_entry_t*)MMAP_ADDR;
    
    // Рассчитать общий размер доступной памяти
    total_heap_size = 0;
    for (uint32_t i = 0; i < mmap.count; i++) {
        if (mmap.entries[i].type == MMAP_TYPE_USABLE) {
            total_heap_size += (uint32_t)mmap.entries[i].length;
        }
    }
    
    // Инициализировать первый блок памяти
    heap_start->size = total_heap_size - sizeof(block_header_t);
    heap_start->free = 1;
    heap_start->next = 0;
    heap_start->prev = 0;
    
    heap_end = HEAP_START + sizeof(block_header_t);
}

memory_map_t get_mmap() {
    return mmap;
}

void* pmm_malloc(uint32_t size) {
    if (size == 0) return 0;
    
    // Выровнять размер для лучшей производительности
    size = (size + 3) & ~3;
    
    block_header_t* current = heap_start;
    
    // Поиск свободного блока (First Fit)
    while (current) {
        if (current->free && current->size >= size) {
            // Если блок значительно больше, чем нужно, разделить его
            if (current->size > size + sizeof(block_header_t)) {
                block_header_t* new_block = 
                    (block_header_t*)((uint8_t*)current + sizeof(block_header_t) + size);
                
                new_block->size = current->size - size - sizeof(block_header_t);
                new_block->free = 1;
                new_block->next = current->next;
                new_block->prev = current;
                
                if (current->next) {
                    current->next->prev = new_block;
                }
                
                current->next = new_block;
                current->size = size;
            }
            
            current->free = 0;
            return (void*)((uint8_t*)current + sizeof(block_header_t));
        }
        
        current = current->next;
    }
    
    return 0; // Недостаточно памяти
}

void pmm_free(void* ptr) {
    if (!ptr) return;
    
    // Получить заголовок блока
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    block->free = 1;
    
    // Объединить с предыдущим блоком, если он свободен
    if (block->prev && block->prev->free) {
        block->prev->size += block->size + sizeof(block_header_t);
        block->prev->next = block->next;
        
        if (block->next) {
            block->next->prev = block->prev;
        }
        
        block = block->prev;
    }
    
    // Объединить со следующим блоком, если он свободен
    if (block->next && block->next->free) {
        block->size += block->next->size + sizeof(block_header_t);
        block->next = block->next->next;
        
        if (block->next) {
            block->next->prev = block;
        }
    }
}

void pmm_print_stats() {
    uint32_t total_free = 0;
    uint32_t total_used = 0;
    uint32_t free_blocks = 0;
    uint32_t used_blocks = 0;
    uint32_t fragmentation = 0;
    
    block_header_t* current = heap_start;
    
    while (current) {
        if (current->free) {
            total_free += current->size;
            free_blocks++;
        } else {
            total_used += current->size;
            used_blocks++;
        }
        
        current = current->next;
    }
    
    // Вычислить фрагментацию (количество свободных блоков)
    if (free_blocks > 1) {
        fragmentation = free_blocks - 1;
    }
    
    uint32_t total_memory = total_free + total_used;
    uint32_t percent_used = (total_used * 100) / total_memory;
    uint32_t percent_free = (total_free * 100) / total_memory;
    
    // Вывод информации
    // Используйте printf или собственную функцию вывода в консоль
    printf("=== Memory Statistics ===\n");
    printf("Total Memory: %u MB\n", total_memory / (1024 * 1024));
    printf("Used: %u MB (%u%%)\n", total_used / (1024 * 1024), percent_used);
    printf("Free: %u MB (%u%%)\n", total_free / (1024 * 1024), percent_free);
    printf("Used Blocks: %u\n", used_blocks);
    printf("Free Blocks: %u\n", free_blocks);
    printf("Fragmentation: %u\n", fragmentation);
    printf("========================\n");
}

void pmm_defragment() {
    block_header_t* current = heap_start;
    
    while (current && current->next) {
        // Если текущий блок свободен и следующий тоже свободен, объединить
        if (current->free && current->next->free) {
            current->size += current->next->size + sizeof(block_header_t);
            current->next = current->next->next;
            
            if (current->next) {
                current->next->prev = current;
            }
            
            // Не переходить к следующему, проверить снова
            continue;
        }
        
        current = current->next;
    }
}