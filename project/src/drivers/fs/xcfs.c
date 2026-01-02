#include "../../include/storage/ata.h"
#include "../../lib/io.h"
#include "../../include/text.h"

static ata_device_t ata_devices[4];

static void ata_delay(uint16_t io_base) {
    for(int i = 0; i < 4; i++)
        inb(io_base + ATA_REG_STATUS);
}

static int ata_wait(uint16_t io_base, uint8_t mask, uint8_t value, uint32_t timeout) {
    while(timeout--) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if((status & ATA_SR_BSY) == 0 && (status & mask) == value)
            return 0;
    }
    return -1;
}

static int ata_detect(uint16_t io_base, uint16_t ctrl_base, uint8_t master) {
    outb(io_base + ATA_REG_DRIVE, master ? ATA_MASTER : ATA_SLAVE);
    ata_delay(io_base);
    
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(io_base);
    
    uint8_t status = inb(io_base + ATA_REG_STATUS);
    if(status == 0) return 0;
    
    if(ata_wait(io_base, ATA_SR_DRQ, ATA_SR_DRQ, 100000) < 0)
        return 0;
    
    uint16_t data[256];
    for(int i = 0; i < 256; i++)
        data[i] = inw(io_base + ATA_REG_DATA);
    
    return 1;
}

void ata_init(void) {
    ata_devices[0].io_base = ATA_PRIMARY_IO;
    ata_devices[0].ctrl_base = ATA_PRIMARY_CTRL;
    ata_devices[0].master = 1;
    ata_devices[0].exists = ata_detect(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 1);
    
    ata_devices[1].io_base = ATA_PRIMARY_IO;
    ata_devices[1].ctrl_base = ATA_PRIMARY_CTRL;
    ata_devices[1].master = 0;
    ata_devices[1].exists = ata_detect(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 0);
    
    ata_devices[2].io_base = ATA_SECONDARY_IO;
    ata_devices[2].ctrl_base = ATA_SECONDARY_CTRL;
    ata_devices[2].master = 1;
    ata_devices[2].exists = ata_detect(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 1);
    
    ata_devices[3].io_base = ATA_SECONDARY_IO;
    ata_devices[3].ctrl_base = ATA_SECONDARY_CTRL;
    ata_devices[3].master = 0;
    ata_devices[3].exists = ata_detect(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 0);
}

int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t* buffer) {
    if(drive > 3 || !ata_devices[drive].exists) return -1;
    
    ata_device_t* dev = &ata_devices[drive];
    
    if(ata_wait(dev->io_base, 0, 0, 100000) < 0) return -1;
    
    outb(dev->io_base + ATA_REG_DRIVE, 
         (dev->master ? ATA_MASTER : ATA_SLAVE) | ((lba >> 24) & 0x0F));
    outb(dev->io_base + ATA_REG_SECCOUNT, 1);
    outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    if(ata_wait(dev->io_base, ATA_SR_DRQ, ATA_SR_DRQ, 100000) < 0) return -1;
    
    uint16_t* buf16 = (uint16_t*)buffer;
    for(int i = 0; i < 256; i++)
        buf16[i] = inw(dev->io_base + ATA_REG_DATA);
    
    return 0;
}

int ata_write_sector(uint8_t drive, uint32_t lba, uint8_t* buffer) {
    if(drive > 3 || !ata_devices[drive].exists) return -1;
    
    ata_device_t* dev = &ata_devices[drive];
    
    if(ata_wait(dev->io_base, 0, 0, 100000) < 0) return -1;
    
    outb(dev->io_base + ATA_REG_DRIVE, 
         (dev->master ? ATA_MASTER : ATA_SLAVE) | ((lba >> 24) & 0x0F));
    outb(dev->io_base + ATA_REG_SECCOUNT, 1);
    outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    if(ata_wait(dev->io_base, ATA_SR_DRQ, ATA_SR_DRQ, 100000) < 0) return -1;
    
    uint16_t* buf16 = (uint16_t*)buffer;
    for(int i = 0; i < 256; i++)
        outw(dev->io_base + ATA_REG_DATA, buf16[i]);
    
    outb(dev->io_base + ATA_REG_COMMAND, 0xE7);
    
    if(ata_wait(dev->io_base, ATA_SR_BSY, 0, 100000) < 0) return -1;
    
    return 0;
}}

void xcfs_format(uint8_t drive, uint32_t total_sectors) {
    xcfs_header.magic = XCFS_MAGIC;
    xcfs_header.version = XCFS_VERSION;
    xcfs_header.total_sectors = total_sectors;
    xcfs_header.file_count = 0;
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    memcpy(xcfs_sector_buf, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(drive, XCFS_START_SECTOR, xcfs_sector_buf);
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    for (uint32_t i = XCFS_START_SECTOR + 1; i < XCFS_DATA_START; i++)
        ata_write_sector(drive, i, xcfs_sector_buf);
    xcfs_ctx.drive = drive;
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}XCFS formatted\n");
}

static uint32_t xcfs_find_free_space(uint32_t size) {
    uint32_t best_start = XCFS_DATA_START;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        uint32_t occupied_sectors = (xcfs_entries[i].size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
        if (occupied_sectors == 0) occupied_sectors = 1;
        uint32_t file_end = xcfs_entries[i].start_sector + occupied_sectors;
        if (file_end > best_start)
            best_start = file_end;
    }
    return best_start;
}

int xcfs_create(const char* name, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    if (size == 0) return -1;
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    uint32_t max_header_sectors = XCFS_DATA_START - (XCFS_START_SECTOR + 1);
    uint32_t max_files_possible = max_header_sectors * entries_per_sector;
    if (xcfs_header.file_count >= XCFS_MAX_FILES) return -1;
    if (XCFS_MAX_FILES > max_files_possible) {
        if (xcfs_header.file_count >= max_files_possible) return -1;
    }
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].name, name) == 0)
            return -1;
    }
    uint32_t start = xcfs_find_free_space(size);
    if (xcfs_check_bounds(start, size) < 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[xcfs_header.file_count];
    strncpy(entry->name, name, XCFS_MAX_NAME - 1);
    entry->start_sector = start;
    entry->size = size;
    entry->flags = 0;
    xcfs_header.file_count++;
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    memcpy(xcfs_sector_buf, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR, xcfs_sector_buf);
    uint32_t entry_index = xcfs_header.file_count - 1;
    uint32_t entry_sector = entry_index / entries_per_sector;
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    memcpy(xcfs_sector_buf, &xcfs_entries[entry_sector * entries_per_sector], XCFS_SECTOR_SIZE);
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + entry_sector, xcfs_sector_buf);
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
            for (uint32_t s = 0; s < header_sectors; s++) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                memcpy(xcfs_sector_buf, &xcfs_entries[s * entries_per_sector], XCFS_SECTOR_SIZE);
                ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + s, xcfs_sector_buf);
            }
            if (header_sectors < (XCFS_DATA_START - (XCFS_START_SECTOR + 1))) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + header_sectors, xcfs_sector_buf);
            }
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
            if (xcfs_check_bounds(entry->start_sector, entry->size) < 0) return -1;
            uint32_t to_read = (size < entry->size) ? size : entry->size;
            uint32_t sectors = (to_read + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            for (uint32_t s = 0; s < sectors; s++) {
                if (ata_read_sector(xcfs_ctx.drive, entry->start_sector + s, xcfs_sector_buf) < 0) return -1;
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
    if (size == 0) return -1;
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].name, name) == 0) {
            xcfs_entry_t* entry = &xcfs_entries[i];
            if (size > entry->size) {
                uint32_t new_start = xcfs_find_free_space(size);
                if (xcfs_check_bounds(new_start, size) < 0) return -1;
                entry->start_sector = new_start;
                entry->size = size;
            }
            uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            for (uint32_t s = 0; s < sectors; s++) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                uint32_t copy_size = (size > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : size;
                memcpy(xcfs_sector_buf, buffer + (s * XCFS_SECTOR_SIZE), copy_size);
                if (ata_write_sector(xcfs_ctx.drive, entry->start_sector + s, xcfs_sector_buf) < 0) return -1;
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
    if (!xcfs_ctx.initialized) return -1;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        printf("%s - %u bytes @ sector %u\n",
               xcfs_entries[i].name,
               xcfs_entries[i].size,
               xcfs_entries[i].start_sector);
    }
    printf("{FG(0,255,0)}Total: %u files\n", xcfs_header.file_count);
    return 0;
}        header_sectors = (xcfs_header.file_count + entries_per_sector - 1) / entries_per_sector;
    for (uint32_t i = 0; i < header_sectors; i++) {
        if (ata_read_sector(drive, XCFS_START_SECTOR + 1 + i, xcfs_sector_buf) < 0) return;
        memcpy(&xcfs_entries[i * entries_per_sector], xcfs_sector_buf, XCFS_SECTOR_SIZE);
    }
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}XCFS initialized (%u files)\n", xcfs_header.file_count);
}

void xcfs_format(uint8_t drive, uint32_t total_sectors) {
    xcfs_header.magic = XCFS_MAGIC;
    xcfs_header.version = XCFS_VERSION;
    xcfs_header.total_sectors = total_sectors;
    xcfs_header.file_count = 0;
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    memcpy(xcfs_sector_buf, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(drive, XCFS_START_SECTOR, xcfs_sector_buf);
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    for (uint32_t i = XCFS_START_SECTOR + 1; i < XCFS_DATA_START; i++)
        ata_write_sector(drive, i, xcfs_sector_buf);
    xcfs_ctx.drive = drive;
    xcfs_ctx.initialized = 1;
    printf("{FG(0,255,0)}XCFS formatted\n");
}

static uint32_t xcfs_find_free_space(uint32_t size) {
    uint32_t best_start = XCFS_DATA_START;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        uint32_t occupied_sectors = (xcfs_entries[i].size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
        if (occupied_sectors == 0) occupied_sectors = 1;
        uint32_t file_end = xcfs_entries[i].start_sector + occupied_sectors;
        if (file_end > best_start)
            best_start = file_end;
    }
    return best_start;
}

int xcfs_create(const char* name, uint32_t size) {
    if (!xcfs_ctx.initialized) return -1;
    if (size == 0) return -1;
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    uint32_t max_header_sectors = XCFS_DATA_START - (XCFS_START_SECTOR + 1);
    uint32_t max_files_possible = max_header_sectors * entries_per_sector;
    if (xcfs_header.file_count >= XCFS_MAX_FILES) return -1;
    if (XCFS_MAX_FILES > max_files_possible) {
        if (xcfs_header.file_count >= max_files_possible) return -1;
    }
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].name, name) == 0)
            return -1;
    }
    uint32_t start = xcfs_find_free_space(size);
    if (xcfs_check_bounds(start, size) < 0) return -1;
    xcfs_entry_t* entry = &xcfs_entries[xcfs_header.file_count];
    strncpy(entry->name, name, XCFS_MAX_NAME - 1);
    entry->start_sector = start;
    entry->size = size;
    entry->flags = 0;
    xcfs_header.file_count++;
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    memcpy(xcfs_sector_buf, &xcfs_header, sizeof(xcfs_header_t));
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR, xcfs_sector_buf);
    uint32_t entry_index = xcfs_header.file_count - 1;
    uint32_t entry_sector = entry_index / entries_per_sector;
    memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
    memcpy(xcfs_sector_buf, &xcfs_entries[entry_sector * entries_per_sector], XCFS_SECTOR_SIZE);
    ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + entry_sector, xcfs_sector_buf);
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
            for (uint32_t s = 0; s < header_sectors; s++) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                memcpy(xcfs_sector_buf, &xcfs_entries[s * entries_per_sector], XCFS_SECTOR_SIZE);
                ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + s, xcfs_sector_buf);
            }
            if (header_sectors < (XCFS_DATA_START - (XCFS_START_SECTOR + 1))) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                ata_write_sector(xcfs_ctx.drive, XCFS_START_SECTOR + 1 + header_sectors, xcfs_sector_buf);
            }
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
            if (xcfs_check_bounds(entry->start_sector, entry->size) < 0) return -1;
            uint32_t to_read = (size < entry->size) ? size : entry->size;
            uint32_t sectors = (to_read + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            for (uint32_t s = 0; s < sectors; s++) {
                if (ata_read_sector(xcfs_ctx.drive, entry->start_sector + s, xcfs_sector_buf) < 0) return -1;
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
    if (size == 0) return -1;
    uint32_t entries_per_sector = XCFS_SECTOR_SIZE / sizeof(xcfs_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        if (strcmp(xcfs_entries[i].name, name) == 0) {
            xcfs_entry_t* entry = &xcfs_entries[i];
            if (size > entry->size) {
                uint32_t new_start = xcfs_find_free_space(size);
                if (xcfs_check_bounds(new_start, size) < 0) return -1;
                entry->start_sector = new_start;
                entry->size = size;
            }
            uint32_t sectors = (size + XCFS_SECTOR_SIZE - 1) / XCFS_SECTOR_SIZE;
            for (uint32_t s = 0; s < sectors; s++) {
                memset(xcfs_sector_buf, 0, XCFS_SECTOR_SIZE);
                uint32_t copy_size = (size > XCFS_SECTOR_SIZE) ? XCFS_SECTOR_SIZE : size;
                memcpy(xcfs_sector_buf, buffer + (s * XCFS_SECTOR_SIZE), copy_size);
                if (ata_write_sector(xcfs_ctx.drive, entry->start_sector + s, xcfs_sector_buf) < 0) return -1;
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
    if (!xcfs_ctx.initialized) return -1;
    for (uint32_t i = 0; i < xcfs_header.file_count; i++) {
        printf("%s - %u bytes @ sector %u\n",
               xcfs_entries[i].name,
               xcfs_entries[i].size,
               xcfs_entries[i].start_sector);
    }
    printf("{FG(0,255,0)}Total: %u files\n", xcfs_header.file_count);
    return 0;
}
