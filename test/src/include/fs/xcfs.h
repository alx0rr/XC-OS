#ifndef XCFS_H
#define XCFS_H

#include <stdint.h>
#include "config.h"

#define XCFS_MAGIC           0x58434653
#define XCFS_VERSION         2
#define XCFS_SECTOR_SIZE     512
#define XCFS_MAX_PATH        248
#define XCFS_MAX_FILES       4096

#define XCFS_START_SECTOR    CFG_XCFS_META_SECTOR
#define XCFS_METADATA_SECTORS CFG_XCFS_META_SECTORS
#define XCFS_DATA_START      CFG_XCFS_DATA_SECTOR
#define XCFS_DATA_START_SECTOR XCFS_DATA_START

#define XCFS_TYPE_FILE       0x01
#define XCFS_TYPE_DIR        0x02
#define XCFS_FLAG_PROTECTED  0x04
#define XCFS_FLAG_EXECUTABLE 0x08
#define XCFS_FLAG_READONLY   0x10

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_sectors;
    uint32_t file_count;
    uint32_t reserved[124];
} __attribute__((packed)) xcfs_header_t;

typedef struct {
    char     path[XCFS_MAX_PATH];
    uint32_t start_sector;
    uint32_t size;
    uint32_t parent_idx;
    uint8_t  type;
    uint8_t  flags;
    uint16_t children_count;
    uint32_t first_child_idx;
    uint32_t created;
    uint32_t modified;
    uint8_t  reserved[236];
} __attribute__((packed)) xcfs_entry_t;

typedef struct {
    uint8_t  drive;
    uint8_t  initialized;
    char     cwd[XCFS_MAX_PATH];
    uint32_t data_start;
    uint32_t bitmap_sectors;
    uint8_t  bitmap[1024 * 1024];
} xcfs_context_t;

typedef struct {
    char    name[256];
    uint8_t type;
    uint8_t flags;
    uint32_t size;
} xcfs_dirent_t;

void        xcfs_init(uint8_t drive);
void        xcfs_format(uint8_t drive, uint32_t total_sectors);
int         xcfs_mkdir(const char* path);
int         xcfs_create(const char* path, uint32_t size);
int         xcfs_delete(const char* path);
int         xcfs_read(const char* path, uint8_t* buf, uint32_t size);
int         xcfs_write(const char* path, uint8_t* buf, uint32_t size);
int         xcfs_readdir(const char* path, xcfs_dirent_t* entries, uint32_t max);
int         xcfs_stat(const char* path, xcfs_dirent_t* info);
int         xcfs_chdir(const char* path);
const char* xcfs_getcwd(void);
void        xcfs_normalize_path(const char* path, char* out);
int         xcfs_list(const char* path);

#endif
