// xcfs.h
#ifndef XCFS_H
#define XCFS_H

#include <stdint.h>

#define XCFS_MAGIC 0x58434653
#define XCFS_VERSION 1
#define XCFS_SECTOR_SIZE 512
#define XCFS_MAX_NAME 56
#define XCFS_MAX_FILES 1024
#define XCFS_DATA_START 248

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_sectors;
    uint32_t file_count;
    uint32_t reserved[12];
} __attribute__((packed)) xcfs_header_t;

typedef struct {
    char name[XCFS_MAX_NAME];
    uint32_t start_sector;
    uint32_t size;
    uint8_t flags;
    uint8_t reserved[3];
} __attribute__((packed)) xcfs_entry_t;

typedef struct {
    uint8_t drive;
    uint8_t initialized;
} xcfs_context_t;

void xcfs_init(uint8_t drive);
void xcfs_format(uint8_t drive, uint32_t total_sectors);
int xcfs_create(const char* name, uint32_t size);
int xcfs_delete(const char* name);
int xcfs_read(const char* name, uint8_t* buffer, uint32_t size);
int xcfs_write(const char* name, uint8_t* buffer, uint32_t size);
int xcfs_list(void);

#endif