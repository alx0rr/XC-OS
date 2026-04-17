//  /ᐠ - ˕ -マ
#include "../../include/storage/ata.h"
#include "../../lib/io.h"
#include "../../include/text.h"
#include "../../include/interrupts/idt.h"

static ata_device_t ata_devices[4];
static volatile uint8_t ata_irq_invoked = 0;
static volatile uint8_t ata_irq_error = 0;

static void ata_irq_handler_primary(registers_t* regs) {
    (void)regs;
    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if(status & ATA_SR_ERR) ata_irq_error = 1;
    ata_irq_invoked = 1;
}

static void ata_irq_handler_secondary(registers_t* regs) {
    (void)regs;
    uint8_t status = inb(ATA_SECONDARY_IO + ATA_REG_STATUS);
    if(status & ATA_SR_ERR) ata_irq_error = 1;
    ata_irq_invoked = 1;
}

static void ata_delay(uint16_t io_base) {
    for(int i = 0; i < 4; i++)
        inb(io_base + ATA_REG_STATUS);
}

static int ata_wait(uint16_t io_base, uint8_t mask, uint8_t value, uint32_t timeout) {
    uint8_t status;
    while(timeout--) {
        status = inb(io_base + ATA_REG_STATUS);
        if((status & ATA_SR_BSY) == 0 && (status & mask) == value)
            return 0;
    }
    return -1;
}

static int ata_wait_irq(uint32_t timeout_ms) {
    uint32_t timeout = timeout_ms * 1000;
    while(timeout--) {
        if(ata_irq_invoked) {
            uint8_t err = ata_irq_error;
            ata_irq_invoked = 0;
            ata_irq_error = 0;
            return err ? -1 : 0;
        }
        for(int i = 0; i < 10; i++)
            __asm__ volatile("pause");
    }
    return -1;
}

static void ata_reset_bus(uint16_t ctrl_base) {
    outb(ctrl_base, 0x04);
    ata_delay(ctrl_base - 0x1F0);
    outb(ctrl_base, 0x00);
    for(int i = 0; i < 100; i++)
        inb(ctrl_base - 0x1F0 + ATA_REG_STATUS);
}

static int ata_select_device(uint16_t io_base, uint8_t master) {
    outb(io_base + ATA_REG_DRIVE, master ? ATA_MASTER : ATA_SLAVE);
    ata_delay(io_base);
    uint8_t status = inb(io_base + ATA_REG_STATUS);
    if(status == 0xFF) return 0;
    if(ata_wait(io_base, ATA_SR_BSY, 0, 100000) < 0) return 0;
    return 1;
}

static void ata_set_lba48(ata_device_t* dev, uint64_t lba, uint8_t count) {
    outb(dev->io_base + ATA_REG_DRIVE, (dev->master ? ATA_MASTER : ATA_SLAVE) | 0x40);
    outb(dev->io_base + ATA_REG_SECCOUNT, count);
    outb(dev->io_base + ATA_REG_LBA_LO, (lba >> 24) & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 32) & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 40) & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
}

static int ata_detect_device(ata_device_t* dev) {
    ata_reset_bus(dev->ctrl_base);
    if(!ata_select_device(dev->io_base, dev->master)) return 0;
    
    outb(dev->io_base + ATA_REG_SECCOUNT, 0);
    outb(dev->io_base + ATA_REG_LBA_LO, 0);
    outb(dev->io_base + ATA_REG_LBA_MID, 0);
    outb(dev->io_base + ATA_REG_LBA_HI, 0);
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(dev->io_base);
    
    uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
    if(status == 0) {
        ata_reset_bus(dev->ctrl_base);
        return 0;
    }
    
    if(ata_wait(dev->io_base, ATA_SR_BSY, 0, 100000) < 0) {
        ata_reset_bus(dev->ctrl_base);
        return 0;
    }
    
    status = inb(dev->io_base + ATA_REG_STATUS);
    if((status & ATA_SR_ERR)) {
        ata_reset_bus(dev->ctrl_base);
        return 0;
    }
    
    uint16_t data[256];
    for(int i = 0; i < 256; i++)
        data[i] = inw(dev->io_base + ATA_REG_DATA);
    
    uint16_t capabilities = data[49];
    uint32_t max_lba28 = data[60] | (data[61] << 16);
    uint64_t max_lba48 = 0;
    
    if(capabilities & (1 << 10)) {
        max_lba48 = ((uint64_t)data[100] | ((uint64_t)data[101] << 16) |
                     ((uint64_t)data[102] << 32) | ((uint64_t)data[103] << 48));
        dev->lba48_support = 1;
        dev->max_lba = max_lba48;
    } else {
        dev->lba48_support = 0;
        dev->max_lba = max_lba28;
    }
    
    return 1;
}

void ata_init(void) {
    idt_register_irq_handler(14, ata_irq_handler_primary);
    idt_register_irq_handler(15, ata_irq_handler_secondary);
    
    uint8_t mask = inb(0xA1);
    mask &= ~(1 << 6);
    mask &= ~(1 << 7);
    outb(0xA1, mask);
    
    outb(ATA_PRIMARY_CTRL, 0x02);
    outb(ATA_SECONDARY_CTRL, 0x02);
    
    for(int i = 0; i < 100; i++) {
        inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        inb(ATA_SECONDARY_IO + ATA_REG_STATUS);
    }
    
    ata_devices[0].io_base = ATA_PRIMARY_IO;
    ata_devices[0].ctrl_base = ATA_PRIMARY_CTRL;
    ata_devices[0].master = 1;
    ata_devices[0].exists = ata_detect_device(&ata_devices[0]);
    
    ata_devices[1].io_base = ATA_PRIMARY_IO;
    ata_devices[1].ctrl_base = ATA_PRIMARY_CTRL;
    ata_devices[1].master = 0;
    ata_devices[1].exists = ata_detect_device(&ata_devices[1]);
    
    ata_devices[2].io_base = ATA_SECONDARY_IO;
    ata_devices[2].ctrl_base = ATA_SECONDARY_CTRL;
    ata_devices[2].master = 1;
    ata_devices[2].exists = ata_detect_device(&ata_devices[2]);
    
    ata_devices[3].io_base = ATA_SECONDARY_IO;
    ata_devices[3].ctrl_base = ATA_SECONDARY_CTRL;
    ata_devices[3].master = 0;
    ata_devices[3].exists = ata_detect_device(&ata_devices[3]);
    
    outb(ATA_PRIMARY_CTRL, 0x00);
    outb(ATA_SECONDARY_CTRL, 0x00);
}

int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t* buffer) {
    if(drive > 3 || !ata_devices[drive].exists) return -1;
    if(!buffer) return -1;
    
    ata_device_t* dev = &ata_devices[drive];
    if(lba >= dev->max_lba) return -1;
    
    if(ata_wait(dev->io_base, ATA_SR_BSY, 0, 1000000) < 0) return -1;
    
    if(dev->lba48_support && lba > 0x0FFFFFFF) {
        ata_set_lba48(dev, lba, 1);
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
    } else {
        outb(dev->io_base + ATA_REG_DRIVE, (dev->master ? ATA_MASTER : ATA_SLAVE) | ((lba >> 24) & 0x0F));
        outb(dev->io_base + ATA_REG_SECCOUNT, 1);
        outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
        outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
        outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    }
    
    if(ata_wait_irq(10000) < 0) return -1;
    if(ata_wait(dev->io_base, ATA_SR_DRQ, ATA_SR_DRQ, 1000000) < 0) return -1;
    
    uint16_t* buf16 = (uint16_t*)buffer;
    for(int i = 0; i < 256; i++)
        buf16[i] = inw(dev->io_base + ATA_REG_DATA);
    
    ata_wait(dev->io_base, ATA_SR_BSY, 0, 100000);
    return 0;
}

int ata_write_sector(uint8_t drive, uint32_t lba, uint8_t* buffer) {
    if(drive > 3 || !ata_devices[drive].exists) return -1;
    if(!buffer) return -1;
    
    ata_device_t* dev = &ata_devices[drive];
    if(lba >= dev->max_lba) return -1;
    
    if(ata_wait(dev->io_base, ATA_SR_BSY, 0, 1000000) < 0) return -1;
    
    if(dev->lba48_support && lba > 0x0FFFFFFF) {
        ata_set_lba48(dev, lba, 1);
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);
    } else {
        outb(dev->io_base + ATA_REG_DRIVE, (dev->master ? ATA_MASTER : ATA_SLAVE) | ((lba >> 24) & 0x0F));
        outb(dev->io_base + ATA_REG_SECCOUNT, 1);
        outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
        outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
        outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    }
    
    if(ata_wait(dev->io_base, ATA_SR_DRQ, ATA_SR_DRQ, 1000000) < 0) return -1;
    
    uint16_t* buf16 = (uint16_t*)buffer;
    for(int i = 0; i < 256; i++)
        outw(dev->io_base + ATA_REG_DATA, buf16[i]);
    
    if(ata_wait_irq(10000) < 0) return -1;
    
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
    if(ata_wait_irq(10000) < 0) return -1;
    
    ata_wait(dev->io_base, ATA_SR_BSY, 0, 100000);
    return 0;
}

int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t* buffer, uint8_t count) {
    if(drive > 3 || !ata_devices[drive].exists) return -1;
    if(!buffer || count == 0 || count > 128) return -1;
    
    ata_device_t* dev = &ata_devices[drive];
    if(lba + count > dev->max_lba) return -1;
    
    if(ata_wait(dev->io_base, ATA_SR_BSY, 0, 1000000) < 0) return -1;
    
    if(dev->lba48_support && (lba > 0x0FFFFFFF || (lba + count) > 0x0FFFFFFF)) {
        ata_set_lba48(dev, lba, count);
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
    } else {
        outb(dev->io_base + ATA_REG_DRIVE, (dev->master ? ATA_MASTER : ATA_SLAVE) | ((lba >> 24) & 0x0F));
        outb(dev->io_base + ATA_REG_SECCOUNT, count);
        outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
        outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
        outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    }
    
    for(uint8_t sector = 0; sector < count; sector++) {
        if(ata_wait_irq(10000) < 0) return -1;
        if(ata_wait(dev->io_base, ATA_SR_DRQ, ATA_SR_DRQ, 1000000) < 0) return -1;
        
        uint16_t* buf16 = (uint16_t*)(buffer + sector * 512);
        for(int i = 0; i < 256; i++)
            buf16[i] = inw(dev->io_base + ATA_REG_DATA);
    }
    
    ata_wait(dev->io_base, ATA_SR_BSY, 0, 100000);
    return 0;
}

int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t* buffer, uint8_t count) {
    if(drive > 3 || !ata_devices[drive].exists) return -1;
    if(!buffer || count == 0 || count > 128) return -1;
    
    ata_device_t* dev = &ata_devices[drive];
    if(lba + count > dev->max_lba) return -1;
    
    if(ata_wait(dev->io_base, ATA_SR_BSY, 0, 1000000) < 0) return -1;
    
    if(dev->lba48_support && (lba > 0x0FFFFFFF || (lba + count) > 0x0FFFFFFF)) {
        ata_set_lba48(dev, lba, count);
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);
    } else {
        outb(dev->io_base + ATA_REG_DRIVE, (dev->master ? ATA_MASTER : ATA_SLAVE) | ((lba >> 24) & 0x0F));
        outb(dev->io_base + ATA_REG_SECCOUNT, count);
        outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
        outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
        outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    }
    
    for(uint8_t sector = 0; sector < count; sector++) {
        if(ata_wait(dev->io_base, ATA_SR_DRQ, ATA_SR_DRQ, 1000000) < 0) return -1;
        
        uint16_t* buf16 = (uint16_t*)(buffer + sector * 512);
        for(int i = 0; i < 256; i++)
            outw(dev->io_base + ATA_REG_DATA, buf16[i]);
        
        if(ata_wait_irq(10000) < 0) return -1;
    }
    
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
    if(ata_wait_irq(10000) < 0) return -1;
    
    ata_wait(dev->io_base, ATA_SR_BSY, 0, 100000);
    return 0;
}
