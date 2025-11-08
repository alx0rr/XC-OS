// include/memory.h
#pragma once
#include <stdint.h>
#include <stddef.h>

#define HEAP_START 0x200000
#define HEAP_SIZE 0x100000

typedef struct {
    uint32_t size;
    uint8_t is_free;
} block_meta_t;

#ifdef __cplusplus
extern "C" {
#endif

void memory_init();
void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);
void memory_stats(uint32_t* used, uint32_t* free_mem, uint32_t* blocks);

#ifdef __cplusplus
}
#endif
