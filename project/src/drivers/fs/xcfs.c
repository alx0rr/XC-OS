#include "../../include/fs/xcfs.h"
#include "../../include/storage/ata.h"
#include "../../include/text.h"
#include "../../lib/string.h"
static xcfs_context_t xcfs_ctx = {0};
static xcfs_header_t xcfs_header = {0};
static xcfs_entry_t xcfs_entries[XCFS_MAX_FILES] = {0};
static int find_entry(const char* path);
static uint32_t find_free_space(uint32_t size);
static void save_header(void);
static void save_entry(uint32_t idx);
static int is_path_protected(const char* path);
void xcfs_normalize_path(const char* path, char* normalized) {
    if (!path || !normalized) return;
    if (path[0] == '/') {
        strcpy(normalized, path);
    } else {
        strcpy(normalized, xcfs_ctx.cwd);
        if (normalized[strlen(normalized) - 1] != '/') {
            strcat(normalized, "/");
        }
        strcat(normalized, path);
    }
    int len = strlen(normalized);
    if (len > 1 && normalized[len - 1] == '/') {
        normalized[len - 1] = '\0';
    }
}
static int is_path_protected(const char* path) {
    return (strcmp(path, "/") == 0 || strcmp(path, "/bin") == 0);
}
static int find_entry(const char* path) {
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].path, normalized) == 0) {
            return i;
        }
    }
    return -1;
}
static uint32_t find_free_space(uint32_t size) {
    uint32_t sectors_needed = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors_needed == 0) sectors_needed = 1;
    uint32_t best_start = XCFS_DATA_START;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (xcfs_entries[i].type == XCFS_TYPE_DIR) continue;
        uint32_t file_sectors = (xcfs_entries[i].size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
        if (file_sectors == 0) file_sectors = 1;
        uint32_t file_end = xcfs_entries[i].start_sector + file_sectors;
        if (file_end > best_start) {
            best_start = file_end;
        }
    }
    return best_start;
}
static void save_header(void) {
    uint8_t buffer[XCFS_SECTOR_SIZE];
    memset(buffer, 0, XCFS_SECTOR_SIZE);
    memcpy(buffer, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR, buffer);
}
static void save_entry(uint32_t idx) {
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    uint32_t sector_index = idx / entries_per_sector;
    uint8_t buffer[XCFS_SECTOR_SIZE];
    memset(buffer, 0, XCFS_SECTOR_SIZE);
    memcpy(buffer, &xcfs_entries[sector_index * entries_per_sector], XCFS_SECTOR_SIZE);
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + sector_index, buffer);
}
void xcfs_init(uint8_t drive) {
    xcfs_ctx.drive = drive;
    strcpy(xcfs_ctx.cwd, "/");
    uint8_t buffer[512];
    if (ata_read_sector(drive, XCFS_START_SECTOR, buffer) < 0) {
        printf("{FG(255,0,0)}XCFS init failed\n");
        return;
    }
    memcpy(&xcfs_header, buffer, sizeof(xcfs_header_t));
    if (xcfs_header.magic != XCFS_MAGIC || xcfs_header.version != XCFS_VERSION) {
        printf("{FG(255,0,0}[!]{FG(255,165,0)}XCFS v2 not found, formatting...\n");
        xcfs_format(drive, 131072);
        return;
    }
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    uint32_t header_sectors = 0;
    if (xcfs_header.file_count > 0) {
        header_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;
    }
    for (uint32_t i = 0; i < header_sectors; i++) {
        if (ata_read_sector(drive, XCFS_START_SECTOR + 1 + i, buffer) < 0) return;
        memcpy(&xcfs_entries[i * entries_per_sector], buffer, XCFS_SECTOR_SIZE);
    }
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}[OK]{FG(255,255,255}XCFS v2 initialized (%u entries)\n", xcfs_header.file_count);
}
void xcfs_format(uint8_t drive, uint32_t total_sectors) {
    xcfs_header.magic = XCFS_MAGIC;
    xcfs_header.version = XCFS_VERSION;
    xcfs_header.total_sectors = total_sectors;
    xcfs_header.file_count = 0;
    memset(xcfs_entries, 0, sizeof(xcfs_entries));
    strcpy(xcfs_entries[0].path, "/");
    xcfs_entries[0].type = XCFS_TYPE_DIR;
    xcfs_entries[0].flags = XCFS_FLAG_PROTECTED;
    xcfs_entries[0].parent_idx = 0;
    xcfs_entries[0].start_sector = 0;
    xcfs_entries[0].size = 0;
    xcfs_header.file_count = 1;
    strcpy(xcfs_entries[1].path, "/bin");
    xcfs_entries[1].type = XCFS_TYPE_DIR;
    xcfs_entries[1].flags = XCFS_FLAG_PROTECTED;
    xcfs_entries[1].parent_idx = 0;
    xcfs_entries[1].start_sector = 0;
    xcfs_entries[1].size = 0;
    xcfs_header.file_count = 2;
    uint8_t buffer[512] = {0};
    memcpy(buffer, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(drive, XCFS_START_SECTOR, buffer);
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    memset(buffer, 0, 512);
    memcpy(buffer, xcfs_entries, XCFS_SECTOR_SIZE);
    ata_write_sector(drive, XCFS_START_SECTOR + 1, buffer);
    memset(buffer, 0, 512);
    for (uint32_t i = XCFS_START_SECTOR + 2; i < XCFS_DATA_START; i++) {
        ata_write_sector(drive, i, buffer);
    }
    xcfs_ctx.drive = drive;
    xcfs_ctx.initialized = 1;
    strcpy(xcfs_ctx.cwd, "/");
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)}XCFS v2 formatted with / and /bin\n");
}
int xcfs_mkdir(const char* path) {
    if (!xcfs_ctx.initialized) return -1;
    if (xcfs_header.file_count >= XCFS_MAX_FILES) return -1;
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    if (find_entry(normalized) >= 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[xcfs_header.file_count];
    strncpy(entry->path, normalized, XCFS_MAX_PATH - 1);
    entry->type = XCFS_TYPE_DIR;
    entry->flags = 0;
    entry->start_sector = 0;
    entry->size = 0;
    entry->parent_idx = 0;
    xcfs_header.file_count++;
    save_header();
    save_entry(xcfs_header.file_count - 1);
    return 0;
}
int xcfs_create(const char* path, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    if (xcfs_header.file_count >= XCFS_MAX_FILES) return -1;
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    if (find_entry(normalized) >= 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[xcfs_header.file_count];
    strncpy(entry->path, normalized, XCFS_MAX_PATH - 1);
    entry->type = XCFS_TYPE_FILE;
    entry->flags = 0;
    entry->start_sector = find_free_space(size);
    entry->size = size;
    entry->parent_idx = 0;
    xcfs_header.file_count++;
    save_header();
    save_entry(xcfs_header.file_count - 1);
    uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors == 0) sectors = 1;
    uint8_t buffer[XCFS_SECTOR_SIZE];
    memset(buffer, 0, XCFS_SECTOR_SIZE);
    for (uint32_t s = 0; s < sectors; s++) {
        ata_write_sector(xcfs_ctx.drive, entry->start_sector + s, buffer);
    }
    return 0;
}
int xcfs_delete(const char* path) {
    if (!xcfs_ctx.initialized) return -1;
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    if (is_path_protected(normalized)) return -1;
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    if (xcfs_entries[idx].type == XCFS_TYPE_DIR) {
        for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
            if (i == (uint32_t)idx) continue;
            int plen = strlen(normalized);
            if (strncmp(xcfs_entries[i].path, normalized, plen) == 0) {
                if (xcfs_entries[i].path[plen] == '/' || xcfs_entries[i].path[plen] == '\0') {
                    return -1;
                }
            }
        }
    }
    for (uint32_t i = idx; i < xcfs_header.file_count - 1; i++) {
        xcfs_entries[i] = xcfs_entries[i + 1];
    }
    xcfs_header.file_count--;
    save_header();
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    uint32_t header_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;
    for (uint32_t s = 0; s < header_sectors; s++) {
        save_entry(s * entries_per_sector);
    }
    return 0;
}
int xcfs_read(const char* path, uint8_t* buffer, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[idx];
    if (entry->type != XCFS_TYPE_FILE) return -1;
    uint32_t to_read = (size < entry->size) ? size : entry->size;
    uint32_t sectors = (to_read + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors == 0 && entry->size > 0) sectors = 1;
    uint8_t sector_buf[XCFS_SECTOR_SIZE];
    for (uint32_t s = 0; s < sectors; s++) {
        if (ata_read_sector(xcfs_ctx.drive, entry->start_sector + s, sector_buf) < 0) {
            return -1;
        }
        uint32_t copy_size = (to_read > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : to_read;
        memcpy(buffer + (s * XCFS_SECTOR_SIZE), sector_buf, copy_size);
        to_read -= copy_size;
    }
    return 0;
}
int xcfs_write(const char* path, uint8_t* buffer, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[idx];
    if (entry->type != XCFS_TYPE_FILE) return -1;
    if (entry->flags & XCFS_FLAG_READONLY) return -1;
    if (size > entry->size) {
        entry->start_sector = find_free_space(size);
        entry->size = size;
    }
    uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors == 0) sectors = 1;
    uint8_t sector_buf[XCFS_SECTOR_SIZE];
    for (uint32_t s = 0; s < sectors; s++) {
        memset(sector_buf, 0, XCFS_SECTOR_SIZE);
        uint32_t copy_size = (size > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : size;
        memcpy(sector_buf, buffer + (s * XCFS_SECTOR_SIZE), copy_size);
        if (ata_write_sector(xcfs_ctx.drive, entry->start_sector + s, sector_buf) < 0) {
            return -1;
        }
        size -= copy_size;
    }
    save_entry(idx);
    return 0;
}
int xcfs_readdir(const char* path, xcfs_dirent_t* entries, uint32_t max_entries) {
    if (!xcfs_ctx.initialized) return -1;
    if (!entries || max_entries == 0) return -1;
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    int dir_len = strlen(normalized);
    if (dir_len == 1 && normalized[0] == '/') {
        dir_len = 0;
    }
    uint32_t count = 0;
    uint32_t safe_limit = xcfs_header.file_count;
    if (safe_limit > XCFS_MAX_FILES) safe_limit = XCFS_MAX_FILES;
    for (uint32_t i = 0; i < safe_limit && count < max_entries; i++) {
        if (xcfs_entries[i].path[0] == '\0') continue;
        if (dir_len == 0) {
            if (xcfs_entries[i].path[0] == '/' && strchr(xcfs_entries[i].path + 1, '/') == 0) {
                const char* name = xcfs_entries[i].path + 1;
                if (strlen(name) == 0) continue;
                strncpy(entries[count].name, name, 63);
                entries[count].name[63] = '\0';
                entries[count].type = xcfs_entries[i].type;
                entries[count].flags = xcfs_entries[i].flags;
                entries[count].size = xcfs_entries[i].size;
                count++;
            }
        } else {
            if (strncmp(xcfs_entries[i].path, normalized, dir_len) == 0 &&
                xcfs_entries[i].path[dir_len] == '/') {
                const char* subpath = xcfs_entries[i].path + dir_len + 1;
                if (strchr(subpath, '/') == 0 && strlen(subpath) > 0) {
                    strncpy(entries[count].name, subpath, 63);
                    entries[count].name[63] = '\0';
                    entries[count].type = xcfs_entries[i].type;
                    entries[count].flags = xcfs_entries[i].flags;
                    entries[count].size = xcfs_entries[i].size;
                    count++;
                }
            }
        }
    }
    return count;
}
int xcfs_stat(const char* path, xcfs_dirent_t* info) {
    if (!xcfs_ctx.initialized || !info) return -1;
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    const char* name = strrchr(normalized, '/');
    if (name) {
        name++;
    } else {
        name = normalized;
    }
    strcpy(info->name, name);
    info->type = xcfs_entries[idx].type;
    info->flags = xcfs_entries[idx].flags;
    info->size = xcfs_entries[idx].size;
    return 0;
}
int xcfs_chdir(const char* path) {
    if (!xcfs_ctx.initialized) return -1;
    char normalized[XCFS_MAX_PATH];
    xcfs_normalize_path(path, normalized);
    int idx = find_entry(normalized);
    if (idx < 0) return -1;
    if (xcfs_entries[idx].type != XCFS_TYPE_DIR) return -1;
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
        xcfs_normalize_path(path, normalized);
    } else {
        strcpy(normalized, xcfs_ctx.cwd);
    }
    if (xcfs_header.file_count == 0) {
        printf("{FG(0,255,255)}%s:\n{FG(255,165,0)}Empty filesystem\n", normalized);
        return 0;
    }
    if (xcfs_header.file_count > XCFS_MAX_FILES) {
        printf("{FG(255,0,0)}Corrupted FS (count=%u)\n", xcfs_header.file_count);
        return -1;
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
