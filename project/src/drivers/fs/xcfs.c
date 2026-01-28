#include "../../include/fs/xcfs.h"
#include "../../include/storage/ata.h"
#include "../../include/text.h"
#include "../../lib/string.h"

static xcfs_context_t xcfs_ctx = {0};
static xcfs_header_t xcfs_header = {0};
static xcfs_entry_t xcfs_entries[XCFS_MAX_FILES] = {0};

void xcfs_init(uint8_t drive) {
    xcfs_ctx.drive = drive;
    
    uint8_t buffer[512];
    if(ata_read_sector(drive, XCFS_START_SECTOR, buffer) < 0) {
        printf("{FG(255,0,0)}XCFS init failed\n");
        return;
    }
    
    memcpy(&xcfs_header, buffer, sizeof(xcfs_header_t));

    if(xcfs_header.magic != XCFS_MAGIC) {
        printf("{FG(255,165,0)}XCFS not found, formatting...\n");
        xcfs_format(drive, 131072);
        return;
    }
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    uint32_t header_sectors = 0;
    if (xcfs_header.file_count > 0)
        header_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;

    for (uint32_t i = 0; i < header_sectors; i++) {
        if (ata_read_sector(drive, XCFS_START_SECTOR + 1 + i, buffer) < 0) return;
        memcpy(&xcfs_entries[i * entries_per_sector], buffer, XCFS_SECTOR_SIZE);
    }
    
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}XCFS initialized (%u files)\n", xcfs_header.file_count);
}

void xcfs_format(uint8_t drive, uint32_t total_sectors) {
    xcfs_header.magic = XCFS_MAGIC;
    xcfs_header.version = XCFS_VERSION;
    xcfs_header.total_sectors = total_sectors;
    xcfs_header.file_count = 0;
    
    uint8_t buffer[512] = {0};
    memcpy(buffer, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(drive, XCFS_START_SECTOR, buffer);
    
    memset(buffer, 0, 512);
    for(uint32_t i = XCFS_START_SECTOR + 1; i < XCFS_DATA_START; i++)
        ata_write_sector(drive, i, buffer);
    
    xcfs_ctx.drive = drive;
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}XCFS formatted\n");
}

static uint32_t xcfs_find_free_space(uint32_t size) {
    uint32_t sectors_needed = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors_needed == 0) sectors_needed = 1;
    uint32_t best_start = XCFS_DATA_START;
    
    for(uint32_t i = 0; i < xcfs_header.file_count; i++) {
        uint32_t file_sectors = (xcfs_entries[i].size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
        if (file_sectors == 0) file_sectors = 1;
        uint32_t file_end = xcfs_entries[i].start_sector + file_sectors;
        if(file_end > best_start)
            best_start = file_end;
    }
    
    return best_start;
}

int xcfs_create(const char* name, uint32_t size) {
    if(!xcfs_ctx.initialized) return -1;
    
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    uint32_t max_header_sectors = XCFS_DATA_START_SECTOR - (XCFS_START_SECTOR + 1);
    uint32_t max_files_possible = max_header_sectors * entries_per_sector;

    if (xcfs_header.file_count >= XCFS_MAX_FILES) return -1;
    if (XCFS_MAX_FILES > max_files_possible) {
        if (xcfs_header.file_count >= max_files_possible) return -1;
    }
    
    for(uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if(strcmp(xcfs_entries[i].name, name) == 0)
            return -1;
    }
    
    xcfs_entry_t* entry = &xcfs_entries[xcfs_header.file_count];
    strncpy(entry->name, name, XCFS_MAX_NAME - 1);
    entry->start_sector = xcfs_find_free_space(size);
    entry->size = size;
    entry->flags = 0;

    xcfs_header.file_count++;

    uint8_t buffer[XCFS_SECTOR_SIZE];
    memset(buffer, 0, XCFS_SECTOR_SIZE);
    memcpy(buffer, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR, buffer);

    uint32_t entry_index = xcfs_header.file_count - 1;
    uint32_t entry_sector = entry_index / entries_per_sector;
    memset(buffer, 0, XCFS_SECTOR_SIZE);
    memcpy(buffer, &xcfs_entries[entry_sector * entries_per_sector], XCFS_SECTOR_SIZE);
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + entry_sector, buffer);
    
    uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    if (sectors == 0) sectors = 1;
    memset(buffer, 0, XCFS_SECTOR_SIZE);
    for(uint32_t s = 0; s < sectors; s++) {
        ata_write_sector(xcfs_ctx.drive, entry->start_sector + s, buffer);
    }
    
    printf("{FG(0,255,0)}File created: %s (%u bytes)\n", name, size);
    return 0;
}

int xcfs_delete(const char* name) {
    if(!xcfs_ctx.initialized) return -1;
    
    for(uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if(strcmp(xcfs_entries[i].name, name) == 0) {
            for(uint32_t j = i; j < xcfs_header.file_count - 1; j++)
                xcfs_entries[j] = xcfs_entries[j + 1];
            
            xcfs_header.file_count--;
            uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
            if (entries_per_sector == 0) entries_per_sector = 1;
            uint8_t buffer[XCFS_SECTOR_SIZE];
            memset(buffer, 0, XCFS_SECTOR_SIZE);
            memcpy(buffer, &xcfs_header, sizeof(xcfs_header_t));
            ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR, buffer);

            uint32_t header_sectors = 0;
            if (xcfs_header.file_count > 0)
                header_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;
            for (uint32_t s = 0; s < header_sectors; s++) {
                memset(buffer, 0, XCFS_SECTOR_SIZE);
                memcpy(buffer, &xcfs_entries[s * entries_per_sector], XCFS_SECTOR_SIZE);
                ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + s, buffer);
            }

            if (header_sectors < (XCFS_DATA_START_SECTOR - (XCFS_START_SECTOR + 1))) {
                memset(buffer, 0, XCFS_SECTOR_SIZE);
                ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + header_sectors, buffer);
            }

            printf("{FG(0,255,0)}File deleted: %s\n", name);
            return 0;
        }
    }
    return -1;
}

int xcfs_read(const char* name, uint8_t* buffer, uint32_t size) {
    if(!xcfs_ctx.initialized) return -1;
    
    for(uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if(strcmp(xcfs_entries[i].name, name) == 0) {
            xcfs_entry_t* entry = &xcfs_entries[i];
            uint32_t to_read = (size < entry->size) ? size : entry->size;
            uint32_t sectors = (to_read + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            if (sectors == 0 && entry->size > 0) sectors = 1;
            
            uint8_t sector_buf[XCFS_SECTOR_SIZE];
            for(uint32_t s = 0; s < sectors; s++) {
                if(ata_read_sector(xcfs_ctx.drive, entry->start_sector + s, sector_buf) < 0)
                    return -1;
                
                uint32_t copy_size = (to_read > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : to_read;
                memcpy(buffer + (s * XCFS_SECTOR_SIZE), sector_buf, copy_size);
                to_read -= copy_size;
            }
            return 0;
        }
    }
    return -1;
}

int xcfs_write(const char* name, uint8_t* buffer, uint32_t size) {
    if(!xcfs_ctx.initialized) return -1;
    
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;

    for(uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if(strcmp(xcfs_entries[i].name, name) == 0) {
            xcfs_entry_t* entry = &xcfs_entries[i];

            if(size > entry->size) {
                entry->start_sector = xcfs_find_free_space(size);
                entry->size = size;
            }

            uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            if (sectors == 0) sectors = 1;
            uint8_t sector_buf[XCFS_SECTOR_SIZE];

            for(uint32_t s = 0; s < sectors; s++) {
                memset(sector_buf, 0, XCFS_SECTOR_SIZE);
                uint32_t copy_size = (size > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : size;
                memcpy(sector_buf, buffer + (s * XCFS_SECTOR_SIZE), copy_size);

                if(ata_write_sector(xcfs_ctx.drive, entry->start_sector + s, sector_buf) < 0)
                    return -1;
                size -= copy_size;
            }

            uint8_t hdr_buf[XCFS_SECTOR_SIZE];
            memset(hdr_buf, 0, XCFS_SECTOR_SIZE);
            uint32_t sector_index = i / entries_per_sector;
            memcpy(hdr_buf, &xcfs_entries[sector_index * entries_per_sector], XCFS_SECTOR_SIZE);
            ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + sector_index, hdr_buf);

            return 0;
        }
    }
    return -1;
}

int xcfs_list(void) {
    if(!xcfs_ctx.initialized) return -1;
    
    printf("{FG(0,255,255)}=== XCFS Files ===\n");
    for(uint32_t i = 0; i < xcfs_header.file_count; i++) {
        printf("%s - %u bytes @ sector %u\n", 
               xcfs_entries[i].name, 
               xcfs_entries[i].size,
               xcfs_entries[i].start_sector);
    }
    printf("{FG(0,255,0)}Total: %u files\n", xcfs_header.file_count);
    return 0;
}
