#include "../../include/fs/xcfs.h"
#include "../../include/storage/ata.h"
#include "../../include/text.h"
#include "../../lib/string.h"
#include <string.h>
#include <stdio.h>

#define XCFS_START_SECTOR 100

static xcfs_context_t xcfs_ctx = {0};
static xcfs_header_t xcfs_header = {0};
static xcfs_entry_t xcfs_entries[XCFS_MAX_FILES] = {0};
static uint8_t xcfs_sector_buf[XCFS_SECTOR_SIZE];

static void xcfs_sort_entries_by_start(void) {
    for (uint32_t i = 0; i + 1 < xcfs_header.file_count; i++) {
        for (uint32_t j = i + 1; j < xcfs_header.file_count; j++) {
            if (xcfs_entries[i].start_sector > xcfs_entries[j].start_sector) {
                xcfs_entry_t tmp = xcfs_entries[i];
                xcfs_entries[i] = xcfs_entries[j];
                xcfs_entries[j] = tmp;
            }
        }
    }
}

static uint32_t xcfs_get_data_start(void) {
    if (xcfs_header.data_start_sector == 0)
        return XCFS_DATA_START;
    return xcfs_header.data_start_sector;
}

static uint32_t xcfs_find_free_space(uint32_t size) {
    uint32_t sectors_needed = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
    uint32_t data_start = xcfs_get_data_start();
    if (xcfs_header.file_count == 0)
        return data_start;

    xcfs_sort_entries_by_start();

    uint32_t candidate = data_start;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        uint32_t start = xcfs_entries[i].start_sector;
        uint32_t used_sectors = (xcfs_entries[i].size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
        uint32_t end = start + used_sectors;

        if (start >= candidate + sectors_needed)
            return candidate;

        if (end > candidate)
            candidate = end;
    }

    return candidate;
}

void xcfs_init(uint8_t drive) {
    xcfs_ctx.drive = drive;
    if (ata_read_sector(drive, XCFS_START_SECTOR, xcfs_sector_buf) < 0) {
        printf("{FG(255,0,0)}XCFS init failed\n");
        return;
    }
    memcpy(&xcfs_header, xcfs_sector_buf, sizeof(xcfs_header_t));
    if (xcfs_header.magic != XCFS_MAGIC) {
        printf("{FG(255,165,0)}XCFS not found, formatting...\n");
        xcfs_format(drive, 16384);
        return;
    }
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    uint32_t header_sectors = 0;
    if (xcfs_header.file_count > 0)
        header_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;

    uint32_t max_entry_bytes = XCFS_MAX_FILES * sizeof(xcfs_entry_t);
    for (uint32_t i = 0; i < header_sectors; i++) {
        if (ata_read_sector(drive, XCFS_START_SECTOR + 1 + i, xcfs_sector_buf) < 0) return;
        uint32_t dest_offset = i * entries_per_sector * sizeof(xcfs_entry_t);
        if (dest_offset >= max_entry_bytes) break;
        uint32_t bytes_to_copy = XCFS_SECTOR_SIZE;
        if (dest_offset + bytes_to_copy > max_entry_bytes)
            bytes_to_copy = max_entry_bytes - dest_offset;
        memcpy(((uint8_t*)xcfs_entries) + dest_offset, xcfs_sector_buf, bytes_to_copy);
    }
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}XCFS initialized (%u files)\n", xcfs_header.file_count);
}

void xcfs_format(uint8_t drive, uint32_t total_sectors) {
    xcfs_header.magic = XCFS_MAGIC;
    xcfs_header.version = XCFS_VERSION;
    xcfs_header.total_sectors = total_sectors;
    xcfs_header.file_count = 0;
    xcfs_header.data_start_sector = XCFS_DATA_START;
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    memcpy(xcfs_sector_buf, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(drive, XCFS_START_SECTOR, xcfs_sector_buf);
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    for (uint32_t i = XCFS_START_SECTOR + 1; i < xcfs_header.data_start_sector; i++)
        ata_write_sector(drive, i, xcfs_sector_buf);
    xcfs_ctx.drive = drive;
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}XCFS formatted\n");
}

int xcfs_create(const char* name, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    uint32_t max_header_sectors = xcfs_get_data_start() - (XCFS_START_SECTOR + 1);
    uint32_t max_files_possible = max_header_sectors * entries_per_sector;

    if (xcfs_header.file_count >= XCFS_MAX_FILES) return -1;
    if (XCFS_MAX_FILES > max_files_possible) {
        if (xcfs_header.file_count >= max_files_possible) return -1;
    }
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].name, name) == 0)
            return -1;
    }
    xcfs_entry_t* entry = &xcfs_entries[xcfs_header.file_count];
    strncpy(entry->name, name, XCFS_MAX_NAME - 1);
    entry->name[XCFS_MAX_NAME - 1] = '\0';
    entry->start_sector = xcfs_find_free_space(size);
    entry->size = size;
    entry->flags = 0;
    xcfs_header.file_count++;

    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    memcpy(xcfs_sector_buf, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR, xcfs_sector_buf);

    uint32_t entry_index = xcfs_header.file_count - 1;
    uint32_t entry_sector = entry_index / entries_per_sector;
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    uint32_t src_offset = entry_sector * entries_per_sector * sizeof(xcfs_entry_t);
    uint32_t max_entry_bytes = XCFS_MAX_FILES * sizeof(xcfs_entry_t);
    uint32_t bytes_to_copy = XCFS_SECTOR_SIZE;
    if (src_offset >= max_entry_bytes) bytes_to_copy = 0;
    else if (src_offset + bytes_to_copy > max_entry_bytes) bytes_to_copy = max_entry_bytes - src_offset;
    if (bytes_to_copy) memcpy(xcfs_sector_buf, ((uint8_t*)xcfs_entries) + src_offset, bytes_to_copy);
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + entry_sector, xcfs_sector_buf);

    printf("{FG(0,255,0)}File created: %s (%u bytes)\n", name, size);
    return 0;
}

int xcfs_delete(const char* name) {
    if (!xcfs_ctx.initialized) return -1;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].name, name) == 0) {
            for (uint32_t j = i; j < xcfs_header.file_count - 1; j++)
                xcfs_entries[j] = xcfs_entries[j + 1];
            xcfs_header.file_count--;
            uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
            if (entries_per_sector == 0) entries_per_sector = 1;
            memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
            memcpy(xcfs_sector_buf, &xcfs_header, sizeof(xcfs_header_t));
            ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR, xcfs_sector_buf);

            uint32_t header_sectors = 0;
            if (xcfs_header.file_count > 0)
                header_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;
            uint32_t max_entry_bytes = XCFS_MAX_FILES * sizeof(xcfs_entry_t);
            for (uint32_t s = 0; s < header_sectors; s++) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                uint32_t src_offset = s * entries_per_sector * sizeof(xcfs_entry_t);
                uint32_t bytes_to_copy = XCFS_SECTOR_SIZE;
                if (src_offset >= max_entry_bytes) bytes_to_copy = 0;
                else if (src_offset + bytes_to_copy > max_entry_bytes) bytes_to_copy = max_entry_bytes - src_offset;
                if (bytes_to_copy) memcpy(xcfs_sector_buf, ((uint8_t*)xcfs_entries) + src_offset, bytes_to_copy);
                ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + s, xcfs_sector_buf);
            }

            if (header_sectors < (xcfs_get_data_start() - (XCFS_START_SECTOR + 1))) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + header_sectors, xcfs_sector_buf);
            }

            printf("{FG(0,255,0)}File deleted: %s\n", name);
            return 0;
        }
    }
    return -1;
}

int xcfs_read(const char* name, uint8_t* buffer, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].name, name) == 0) {
            xcfs_entry_t* entry = &xcfs_entries[i];
            uint32_t to_read = (size < entry->size) ? size : entry->size;
            uint32_t sectors = (to_read + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            for (uint32_t s = 0; s < sectors; s++) {
                if (ata_read_sector(xcfs_ctx.drive, entry->start_sector + s, xcfs_sector_buf) < 0)
                    return -1;
                uint32_t copy_size = (to_read > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : to_read;
                memcpy(buffer + (s * XCFS_SECTOR_SIZE), xcfs_sector_buf, copy_size);
                to_read -= copy_size;
            }
            return 0;
        }
    }
    return -1;
}

int xcfs_write(const char* name, uint8_t* buffer, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].name, name) == 0) {
            xcfs_entry_t* entry = &xcfs_entries[i];
            if (size > entry->size) {
                entry->start_sector = xcfs_find_free_space(size);
                entry->size = size;
            }
            uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            uint32_t remaining = size;
            for (uint32_t s = 0; s < sectors; s++) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                uint32_t copy_size = (remaining > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : remaining;
                memcpy(xcfs_sector_buf, buffer + (s * XCFS_SECTOR_SIZE), copy_size);
                if (ata_write_sector(xcfs_ctx.drive, entry->start_sector + s, xcfs_sector_buf) < 0)
                    return -1;
                remaining -= copy_size;
            }
            uint32_t sector_index = i / entries_per_sector;
            memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
            uint32_t src_offset = sector_index * entries_per_sector * sizeof(xcfs_entry_t);
            uint32_t max_entry_bytes = XCFS_MAX_FILES * sizeof(xcfs_entry_t);
            uint32_t bytes_to_copy = XCFS_SECTOR_SIZE;
            if (src_offset >= max_entry_bytes) bytes_to_copy = 0;
            else if (src_offset + bytes_to_copy > max_entry_bytes) bytes_to_copy = max_entry_bytes - src_offset;
            if (bytes_to_copy) memcpy(xcfs_sector_buf, ((uint8_t*)xcfs_entries) + src_offset, bytes_to_copy);
            ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + sector_index, xcfs_sector_buf);
            return 0;
        }
    }
    return -1;
}

int xcfs_list(void) {
    if (!xcfs_ctx.initialized) return -1;
    printf("{FG(0,255,255)}=== XCFS Files ===\n");
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        printf("%s - %u bytes @ sector %u\n",
               xcfs_entries[i].name,
               xcfs_entries[i].size,
               xcfs_entries[i].start_sector);
    }
    printf("{FG(0,255,0)}Total: %u files\n", xcfs_header.file_count);
    return 0;
}
