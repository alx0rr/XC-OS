#include "../../include/fs/xcfs.h"
#include "../../include/storage/ata.h"
#include "../../include/text.h"
#include "../../lib/string.h"

static xcfs_context_t xcfs_ctx = {0};
static xcfs_header_t xcfs_header = {0};
static xcfs_entry_t xcfs_entries[XCFS_MAX_FILES] = {0};

static int find_entry(const char* path);
static int find_parent_idx(const char* path);
static uint32_t find_free_space(uint32_t size);
static void save_header(void);
static void save_entry(uint32_t idx);
static void save_bitmap(void);
static int is_path_protected(const char* path);
static void normalize_path(const char* path, char* normalized);
static int bitmap_set(uint32_t sector, int used);
static int bitmap_test(uint32_t sector);
static uint32_t bitmap_find_free(uint32_t count);

void xcfs_normalize_path(const char* path, char* normalized) {
    normalize_path(path, normalized);
}

static void normalize_path(const char* path, char* normalized) {
    if (!path || !normalized) return;
    if (path[0] == '/') {
        strcpy(normalized, path);
    } else {
        strcpy(normalized, xcfs_ctx.cwd);
        if (normalized[strlen(normalized)-1] != '/') {
            strcat(normalized, "/");
        }
        strcat(normalized, path);
    }
    int len = strlen(normalized);
    if (len > 1 && normalized[len-1] == '/') {
        normalized[len-1] = '\0';
    }
}

static int is_path_protected(const char* path) {
    return (strcmp(path, "/") == 0);
}

static int find_entry(const char* path) {
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].path, normalized) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_parent_idx(const char* path) {
    char parent_path[XCFS_MAX_PATH];
    strcpy(parent_path, path);
    char* last_slash = strrchr(parent_path, '/');
    if (!last_slash) return -1;
    if (last_slash == parent_path) {
        return 0;
    }
    *last_slash = '\0';
    return find_entry(parent_path);
}

static uint32_t find_free_space(uint32_t size) {
    uint32_t sectors_needed = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors_needed == 0) sectors_needed = 1;
    return bitmap_find_free(sectors_needed);
}

static void save_header(void) {
    uint8_t buffer[XCFS_SECTOR_SIZE];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR, buffer);
}

static void save_entry(uint32_t idx) {
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    uint32_t sector = idx / entries_per_sector;
    uint8_t buffer[XCFS_SECTOR_SIZE];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &xcfs_entries[sector * entries_per_sector], XCFS_SECTOR_SIZE);
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + sector, buffer);
}

static void save_bitmap(void) {
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    uint32_t entries_sectors = (XCFS_MAX_FILES + entries_per_sector - 1) / entries_per_sector;
    uint32_t bitmap_start = XCFS_START_SECTOR + 1 + entries_sectors;
    for (uint32_t i = 0; i < xcfs_ctx.bitmap_sectors; i++) {
        ata_write_sector(xcfs_ctx.drive, bitmap_start + i, &xcfs_ctx.bitmap[i * XCFS_SECTOR_SIZE]);
    }
}

static int bitmap_set(uint32_t sector, int used) {
    if (sector < xcfs_ctx.data_start || sector >= xcfs_header.total_sectors) return -1;
    uint32_t idx = sector - xcfs_ctx.data_start;
    uint32_t byte_off = idx / 8;
    uint32_t bit_off = idx % 8;
    if (byte_off >= xcfs_ctx.bitmap_sectors * XCFS_SECTOR_SIZE) return -1;
    uint8_t* bitmap_byte = &xcfs_ctx.bitmap[byte_off];
    if (used) {
        *bitmap_byte |= (1 << bit_off);
    } else {
        *bitmap_byte &= ~(1 << bit_off);
    }
    return 0;
}

static int bitmap_test(uint32_t sector) {
    if (sector < xcfs_ctx.data_start || sector >= xcfs_header.total_sectors) return -1;
    uint32_t idx = sector - xcfs_ctx.data_start;
    uint32_t byte_off = idx / 8;
    uint32_t bit_off = idx % 8;
    if (byte_off >= xcfs_ctx.bitmap_sectors * XCFS_SECTOR_SIZE) return -1;
    return (xcfs_ctx.bitmap[byte_off] >> bit_off) & 1;
}

static uint32_t bitmap_find_free(uint32_t count) {
    uint32_t total_data_sectors = xcfs_header.total_sectors - xcfs_ctx.data_start;
    uint32_t consecutive = 0;
    uint32_t start = 0;
    for (uint32_t i = 0; i < total_data_sectors; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        if (!(xcfs_ctx.bitmap[byte] & (1 << bit))) {
            if (consecutive == 0) start = i;
            consecutive++;
            if (consecutive >= count) {
                return xcfs_ctx.data_start + start;
            }
        } else {
            consecutive = 0;
        }
    }
    return 0;
}

void xcfs_init(uint8_t drive) {
    xcfs_ctx.drive = drive;
    strcpy(xcfs_ctx.cwd, "/");
    uint8_t buffer[XCFS_SECTOR_SIZE];
    if (ata_read_sector(drive, XCFS_START_SECTOR, buffer) < 0) {
        printf("{FG(255,0,0)}XCFS init failed\n");
        return;
    }
    memcpy(&xcfs_header, buffer, sizeof(xcfs_header_t));
    if (xcfs_header.magic != XCFS_MAGIC || xcfs_header.version != XCFS_VERSION) {
        printf("{FG(255,0,0)}[!]{FG(255,255,255)} XCFS v%d not found, formatting...\n", XCFS_VERSION);
        xcfs_format(drive, 131072);
        return;
    }
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    uint32_t entries_sectors = (XCFS_MAX_FILES + entries_per_sector - 1) / entries_per_sector;
    uint32_t file_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;
    for (uint32_t i = 0; i < file_sectors; i++) {
        if (ata_read_sector(drive, XCFS_START_SECTOR + 1 + i, buffer) < 0) return;
        memcpy(&xcfs_entries[i * entries_per_sector], buffer, XCFS_SECTOR_SIZE);
    }
    xcfs_ctx.bitmap_sectors = (xcfs_header.total_sectors + 8 * XCFS_SECTOR_SIZE - 1) / (8 * XCFS_SECTOR_SIZE);
    xcfs_ctx.data_start = XCFS_START_SECTOR + 1 + entries_sectors + xcfs_ctx.bitmap_sectors;
    uint32_t bitmap_start = XCFS_START_SECTOR + 1 + entries_sectors;
    for (uint32_t i = 0; i < xcfs_ctx.bitmap_sectors; i++) {
        ata_read_sector(drive, bitmap_start + i, &xcfs_ctx.bitmap[i * XCFS_SECTOR_SIZE]);
    }
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} XCFS v%d initialized (%u entries)\n", XCFS_VERSION, xcfs_header.file_count);
}

void xcfs_format(uint8_t drive, uint32_t total_sectors) {
    xcfs_header.magic = XCFS_MAGIC;
    xcfs_header.version = XCFS_VERSION;
    xcfs_header.total_sectors = total_sectors;
    xcfs_header.file_count = 0;
    memset(xcfs_entries, 0, sizeof(xcfs_entries));
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    uint32_t entries_sectors = (XCFS_MAX_FILES + entries_per_sector - 1) / entries_per_sector;
    xcfs_ctx.bitmap_sectors = (total_sectors + 8 * XCFS_SECTOR_SIZE - 1) / (8 * XCFS_SECTOR_SIZE);
    xcfs_ctx.data_start = XCFS_START_SECTOR + 1 + entries_sectors + xcfs_ctx.bitmap_sectors;
    
    strcpy(xcfs_entries[0].path, "/");
    xcfs_entries[0].type = XCFS_TYPE_DIR;
    xcfs_entries[0].flags = XCFS_FLAG_PROTECTED;
    xcfs_entries[0].parent_idx = 0;
    xcfs_entries[0].start_sector = 0;
    xcfs_entries[0].size = 0;
    xcfs_entries[0].created = 0;
    xcfs_entries[0].modified = 0;
    xcfs_header.file_count = 1; 
    
    uint8_t buffer[XCFS_SECTOR_SIZE] = {0};
    memcpy(buffer, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(drive, XCFS_START_SECTOR, buffer);
    
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, xcfs_entries, XCFS_SECTOR_SIZE);
    ata_write_sector(drive, XCFS_START_SECTOR + 1, buffer);
    
    uint32_t bitmap_start = XCFS_START_SECTOR + 1 + entries_sectors;
    memset(xcfs_ctx.bitmap, 0, XCFS_SECTOR_SIZE * xcfs_ctx.bitmap_sectors);
    for (uint32_t i = 0; i < xcfs_ctx.bitmap_sectors; i++) {
        ata_write_sector(drive, bitmap_start + i, &xcfs_ctx.bitmap[i * XCFS_SECTOR_SIZE]);
    }
    
    xcfs_ctx.drive = drive;
    xcfs_ctx.initialized = 1;
    strcpy(xcfs_ctx.cwd, "/");
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} XCFS v%d formatted\n", XCFS_VERSION);
}

int xcfs_mkdir(const char* path) {
    if (!xcfs_ctx.initialized) return -1;
    if (xcfs_header.file_count >= XCFS_MAX_FILES) return -1;
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    if (find_entry(normalized) >= 0) return -1;
    int parent = find_parent_idx(normalized);
    if (parent < 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[xcfs_header.file_count];
    strncpy(entry->path, normalized, XCFS_MAX_PATH - 1);
    entry->type = XCFS_TYPE_DIR;
    entry->flags = 0;
    entry->parent_idx = parent;
    entry->start_sector = 0;
    entry->size = 0;
    entry->created = 0;
    entry->modified = 0;
    xcfs_header.file_count++;
    save_header();
    save_entry(xcfs_header.file_count - 1);
    return 0;
}

int xcfs_create(const char* path, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    if (xcfs_header.file_count >= XCFS_MAX_FILES) return -1;
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    if (find_entry(normalized) >= 0) return -1;
    int parent = find_parent_idx(normalized);
    if (parent < 0) return -1;
    uint32_t start = find_free_space(size);
    if (start == 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[xcfs_header.file_count];
    strncpy(entry->path, normalized, XCFS_MAX_PATH - 1);
    entry->type = XCFS_TYPE_FILE;
    entry->flags = 0;
    entry->parent_idx = parent;
    entry->start_sector = start;
    entry->size = size;
    entry->created = 0;
    entry->modified = 0;
    uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors == 0) sectors = 1;
    for (uint32_t i = 0; i < sectors; i++) {
        bitmap_set(start + i, 1);
    }
    xcfs_header.file_count++;
    save_header();
    save_entry(xcfs_header.file_count - 1);
    save_bitmap();
    uint8_t buffer[XCFS_SECTOR_SIZE] = {0};
    for (uint32_t s = 0; s < sectors; s++) {
        ata_write_sector(xcfs_ctx.drive, start + s, buffer);
    }
    return 0;
}

int xcfs_delete(const char* path) {
    if (!xcfs_ctx.initialized) return -1;
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    if (is_path_protected(normalized)) return -1;
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    if (xcfs_entries[idx].type == XCFS_TYPE_DIR) {
        for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
            if (xcfs_entries[i].parent_idx == (uint32_t)idx) {
                return -1;
            }
        }
    } else {
        uint32_t sectors = (xcfs_entries[idx].size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
        if (sectors == 0) sectors = 1;
        for (uint32_t i = 0; i < sectors; i++) {
            bitmap_set(xcfs_entries[idx].start_sector + i, 0);
        }
    }
    for (uint32_t i = idx; i < xcfs_header.file_count - 1; i++) {
        xcfs_entries[i] = xcfs_entries[i + 1];
    }
    memset(&xcfs_entries[xcfs_header.file_count - 1], 0, sizeof(xcfs_entry_t));
    xcfs_header.file_count--;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (xcfs_entries[i].parent_idx > (uint32_t)idx) {
            xcfs_entries[i].parent_idx--;
        }
    }
    save_header();
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    uint32_t header_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;
    for (uint32_t s = 0; s < header_sectors; s++) {
        save_entry(s * entries_per_sector);
    }
    save_bitmap();
    return 0;
}

int xcfs_read(const char* path, uint8_t* buffer, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[idx];
    if (entry->type != XCFS_TYPE_FILE) return -1;
    uint32_t to_read = (size < entry->size) ? size : entry->size;
    uint32_t sectors = (to_read + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    uint8_t sector_buf[XCFS_SECTOR_SIZE];
    for (uint32_t s = 0; s < sectors; s++) {
        if (ata_read_sector(xcfs_ctx.drive, entry->start_sector + s, sector_buf) < 0) {
            return -1;
        }
        uint32_t copy = (to_read > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : to_read;
        memcpy(buffer + s * XCFS_SECTOR_SIZE, sector_buf, copy);
        to_read -= copy;
    }
    return 0;
}

int xcfs_write(const char* path, uint8_t* buffer, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[idx];
    if (entry->type != XCFS_TYPE_FILE) return -1;
    if (entry->flags & XCFS_FLAG_READONLY) return -1;
    if (size > entry->size) return -1;
    uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors == 0) sectors = 1;
    uint8_t sector_buf[XCFS_SECTOR_SIZE];
    for (uint32_t s = 0; s < sectors; s++) {
        memset(sector_buf, 0, XCFS_SECTOR_SIZE);
        uint32_t copy = (size > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : size;
        memcpy(sector_buf, buffer + s * XCFS_SECTOR_SIZE, copy);
        if (ata_write_sector(xcfs_ctx.drive, entry->start_sector + s, sector_buf) < 0) {
            return -1;
        }
        size -= copy;
    }
    entry->modified = 0;
    save_entry(idx);
    return 0;
}

int xcfs_readdir(const char* path, xcfs_dirent_t* entries, uint32_t max_entries) {
    if (!xcfs_ctx.initialized || !entries || max_entries == 0) return -1;
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    int dir_idx = find_entry(normalized);
    if (dir_idx < 0 || xcfs_entries[dir_idx].type != XCFS_TYPE_DIR) return -1;
    uint32_t count = 0;
    for (uint32_t i = 0; i < xcfs_header.file_count && count < max_entries; i++) {
        if (xcfs_entries[i].parent_idx == (uint32_t)dir_idx) {
            const char* name = strrchr(xcfs_entries[i].path, '/');
            if (name) name++; else name = xcfs_entries[i].path;
            strncpy(entries[count].name, name, 63);
            entries[count].name[63] = '\0';
            entries[count].type = xcfs_entries[i].type;
            entries[count].flags = xcfs_entries[i].flags;
            entries[count].size = xcfs_entries[i].size;
            count++;
        }
    }
    return count;
}

int xcfs_stat(const char* path, xcfs_dirent_t* info) {
    if (!xcfs_ctx.initialized || !info) return -1;
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    const char* name = strrchr(normalized, '/');
    if (name) name++; else name = normalized;
    strcpy(info->name, name);
    info->type = xcfs_entries[idx].type;
    info->flags = xcfs_entries[idx].flags;
    info->size = xcfs_entries[idx].size;
    return 0;
}

int xcfs_chdir(const char* path) {
    if (!xcfs_ctx.initialized) return -1;
    char normalized[XCFS_MAX_PATH];
    normalize_path(path, normalized);
    int idx = find_entry(normalized);
    if (idx < 0 || xcfs_entries[idx].type != XCFS_TYPE_DIR) return -1;
    strcpy(xcfs_ctx.cwd, normalized);
    return 0;
}

const char* xcfs_getcwd(void) {
    return xcfs_ctx.cwd;
}

int xcfs_list(const char* path) {
    if (!xcfs_ctx.initialized) {
        printf("{FG(255,0,0)}XCFS not initialized\n");
        return -1;
    }
    char normalized[XCFS_MAX_PATH];
    if (path) {
        normalize_path(path, normalized);
    } else {
        strcpy(normalized, xcfs_ctx.cwd);
    }
    if (xcfs_header.file_count == 0) {
        printf("{FG(0,255,255)}%s:\n{FG(255,165,0)}Empty filesystem\n", normalized);
        return 0;
    }
    xcfs_dirent_t entries[32];
    int count = xcfs_readdir(normalized, entries, 32);
    if (count < 0) {
        printf("{FG(255,0,0)}Error reading directory\n");
        return -1;
    }
    if (count == 0) {
        printf("{FG(0,255,255)}%s:\n{FG(255,165,0)}Empty\n", normalized);
        return 0;
    }
    printf("{FG(0,255,255)}=== %s ===\n", normalized);
    uint32_t dirs = 0, files = 0, total_size = 0;
    for (int i = 0; i < count; i++) {
        if (entries[i].type == XCFS_TYPE_DIR) {
            printf("{FG(100,200,255)}[DIR]{FG(255,255,255)}  %s\n", entries[i].name);
            dirs++;
        } else {
            if (entries[i].flags & XCFS_FLAG_EXECUTABLE) {
                if (entries[i].size >= 1024*1024) {
                    printf("{FG(0,255,0)}%s {FG(150,150,150)}(%u MB)\n",
                        entries[i].name, entries[i].size / (1024*1024));
                } else if (entries[i].size >= 1024) {
                    printf("{FG(0,255,0)}%s {FG(150,150,150)}(%u KB)\n",
                        entries[i].name, entries[i].size / 1024);
                } else {
                    printf("{FG(0,255,0)}%s {FG(150,150,150)}(%u B)\n",
                        entries[i].name, entries[i].size);
                }
            } else {
                if (entries[i].size >= 1024*1024) {
                    printf("{FG(255,255,255)}%s {FG(150,150,150)}(%u MB)\n",
                        entries[i].name, entries[i].size / (1024*1024));
                } else if (entries[i].size >= 1024) {
                    printf("{FG(255,255,255)}%s {FG(150,150,150)}(%u KB)\n",
                        entries[i].name, entries[i].size / 1024);
                } else {
                    printf("{FG(255,255,255)}%s {FG(150,150,150)}(%u B)\n",
                        entries[i].name, entries[i].size);
                }
            }
            files++;
            total_size += entries[i].size;
        }
    }
    printf("{FG(0,255,0)}");
    if (dirs > 0) printf("%u dirs ", dirs);
    if (files > 0) printf("%u files", files);
    if (total_size > 0) {
        if (total_size >= 1024*1024) {
            printf(" (%u MB)", total_size / (1024*1024));
        } else if (total_size >= 1024) {
            printf(" (%u KB)", total_size / 1024);
        } else {
            printf(" (%u B)", total_size);
        }
    }
    printf("\n");
    return 0;
}
