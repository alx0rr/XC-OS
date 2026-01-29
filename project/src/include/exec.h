#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

#define EXEC_MAX_ARGS 16
#define EXEC_MAGIC 0x584F5845

typedef struct {
    uint32_t magic;
    uint32_t entry_point;
    uint32_t code_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t reserved[11];
} __attribute__((packed)) exec_header_t;

typedef int (*exec_func_t)(int argc, char** argv);

int exec_load(const char* path, int argc, char** argv);
int exec_find_in_path(const char* name, char* fullpath);

#endif
