#ifndef ATA_H
#define ATA_H
#include <stdint.h>
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376
#define ATA_REG_DATA        0
#define ATA_REG_ERROR       1
#define ATA_REG_FEATURES    1
#define ATA_REG_SECCOUNT    2
#define ATA_REG_LBA_LO      3
#define ATA_REG_LBA_MID     4
#define ATA_REG_LBA_HI      5
#define ATA_REG_DRIVE       6
#define ATA_REG_STATUS      7
#define ATA_REG_COMMAND     7
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DF           0x20
#define ATA_SR_DSC          0x10
#define ATA_SR_DRQ          0x08
#define ATA_SR_CORR         0x04
#define ATA_SR_IDX          0x02
#define ATA_SR_ERR          0x01
#define ATA_MASTER          0xE0
#define ATA_SLAVE           0xF0
typedef struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t master;
    uint8_t exists;
} ata_device_t;
void ata_init(void);
int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t* buffer);
int ata_write_sector(uint8_t drive, uint32_t lba, uint8_t* buffer);
#endif
