#include "../include/memory.h"

static block_meta_t* heap_start = (block_meta_t*)HEAP_START;
static uint32_t heap_end = HEAP_START + HEAP_SIZE;
static uint8_t heap_initialized = 0;
static uint32_t memory_used = 0;
static uint32_t memory_free = HEAP_SIZE;
static uint32_t memory_blocks = 0;

void memory_init() {
    if (heap_initialized) return;
    heap_start->size = HEAP_SIZE - sizeof(block_meta_t);
    heap_start->is_free = 1;
    memory_used = sizeof(block_meta_t);
    memory_free = HEAP_SIZE - sizeof(block_meta_t);
    memory_blocks = 1;
    heap_initialized = 1;
}

static block_meta_t* find_free_block(block_meta_t** last, size_t size) {
    block_meta_t* current = heap_start;
    while (current && !(current->is_free && current->size >= size)) {
        *last = current;
        current = (block_meta_t*)((char*)current + current->size + sizeof(block_meta_t));
        if ((uint32_t)current >= heap_end) current = NULL;
    }
    return current;
}

static block_meta_t* request_space(block_meta_t* last, size_t size) {
    block_meta_t* block;
    if (last) {
        block = (block_meta_t*)((char*)last + last->size + sizeof(block_meta_t));
        if ((uint32_t)block + size + sizeof(block_meta_t) > heap_end) return NULL;
    } else {
        block = heap_start;
    }
    block->size = size;
    block->is_free = 0;
    memory_used += size + sizeof(block_meta_t);
    memory_free -= size + sizeof(block_meta_t);
    memory_blocks++;
    return block;
}

void* malloc(size_t size) {
    if (!heap_initialized) memory_init();
    if (size == 0) return NULL;
    size = (size + 7) & ~7;
    block_meta_t* block;
    block_meta_t* last = NULL;
    block = find_free_block(&last, size);
    if (block) {
        if (block->size >= size + sizeof(block_meta_t) + 8) {
            block_meta_t* new_block = (block_meta_t*)((char*)block + sizeof(block_meta_t) + size);
            new_block->size = block->size - size - sizeof(block_meta_t);
            new_block->is_free = 1;
            block->size = size;
            memory_used += sizeof(block_meta_t);
            memory_free -= sizeof(block_meta_t);
            memory_blocks++;
        }
        block->is_free = 0;
        memory_used += block->size;
        memory_free -= block->size;
        return (void*)((char*)block + sizeof(block_meta_t));
    }
    block = request_space(last, size);
    if (!block) return NULL;
    return (void*)((char*)block + sizeof(block_meta_t));
}

void free(void* ptr) {
    if (!ptr) return;
    block_meta_t* block = (block_meta_t*)((char*)ptr - sizeof(block_meta_t));
    memory_used -= block->size;
    memory_free += block->size;
    block->is_free = 1;
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) for (size_t i = 0; i < total; i++) ((char*)ptr)[i] = 0;
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }
    block_meta_t* block = (block_meta_t*)((char*)ptr - sizeof(block_meta_t));
    if (block->size >= size) return ptr;
    void* new_ptr = malloc(size);
    if (!new_ptr) return NULL;
    for (size_t i = 0; i < block->size; i++) ((char*)new_ptr)[i] = ((char*)ptr)[i];
    free(ptr);
    return new_ptr;
}

void memory_stats(uint32_t* used, uint32_t* free_mem, uint32_t* blocks) {
    if (used) *used = memory_used;
    if (free_mem) *free_mem = memory_free;
    if (blocks) *blocks = memory_blocks;
}
