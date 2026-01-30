#include "../../include/storage/ata.h"
#include "../../lib/io.h"
#include "../../include/text.h"
#include "../../include/interrupts/idt.h"

static ata_device_t ata_devices[4];
static volatile uint8_t ata_irq_invoked = 0;
static volatile uint8_t ata_irq_error = 0;

static void ata_irq_handler_primary(registers_t regs) {
    (void)regs;
    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if(status & ATA_SR_ERR) {
        ata_irq_error = 1;
    }
    ata_irq_invoked = 1;
}

static void ata_irq_handler_secondary(registers_t regs) {
    (void)regs;
    uint8_t status = inb(ATA_SECONDARY_IO + ATA_REG_STATUS);
    if(status & ATA_SR_ERR) {
        ata_irq_error = 1;
    }
    ata_irq_invoked = 1;
}

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

static void ata_wait_irq(void) {
    uint32_t timeout = 1000000;
    while(!ata_irq_invoked && timeout--) {
        asm volatile("hlt");
    }
    ata_irq_invoked = 0;
}

static int ata_detect(uint16_t io_base, uint16_t ctrl_base, uint8_t master) {
    outb(io_base + ATA_REG_DRIVE, master ? ATA_MASTER : ATA_SLAVE);
    ata_delay(io_base);
    
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(io_base);
    
    uint8_t status = inb(io_base + ATA_REG_STATUS);
    if(status == 0) {
        return 0;
    }
    
    if(ata_wait(io_base, ATA_SR_DRQ, ATA_SR_DRQ, 100000) < 0) {
        return 0;
    }
    
    uint16_t data[256];
    for(int i = 0; i < 256; i++)
        data[i] = inw(io_base + ATA_REG_DATA);
    
    return 1;
}

void ata_init(void) {
    idt_register_irq_handler(14, ata_irq_handler_primary);
    idt_register_irq_handler(15, ata_irq_handler_secondary);
    
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
    
    ata_irq_invoked = 0;
    ata_irq_error = 0;
    
    outb(dev->io_base + ATA_REG_DRIVE, 
         (dev->master ? ATA_MASTER : ATA_SLAVE) | ((lba >> 24) & 0x0F));
    outb(dev->io_base + ATA_REG_SECCOUNT, 1);
    outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    ata_wait_irq();
    
    if(ata_irq_error) return -1;
    
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
    
    ata_irq_invoked = 0;
    ata_irq_error = 0;
    
    outb(dev->io_base + ATA_REG_DRIVE, 
         (dev->master ? ATA_MASTER : ATA_SLAVE) | ((lba >> 24) & 0x0F));
    outb(dev->io_base + ATA_REG_SECCOUNT, 1);
    outb(dev->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(dev->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    ata_wait_irq();
    
    if(ata_irq_error) return -1;
    
    if(ata_wait(dev->io_base, ATA_SR_DRQ, ATA_SR_DRQ, 100000) < 0) return -1;
    
    uint16_t* buf16 = (uint16_t*)buffer;
    for(int i = 0; i < 256; i++)
        outw(dev->io_base + ATA_REG_DATA, buf16[i]);
    
    outb(dev->io_base + ATA_REG_COMMAND, 0xE7);
    
    ata_wait_irq();
    
    if(ata_irq_error) return -1;
    
    return 0;
}
