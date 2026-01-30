#include "../../include/pci/pci.h"
#include "../../include/text.h"
#include "../../lib/io.h"
#include "../../lib/string.h"

#define MAX_PCI_DEVICES 256

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read_config(bus, device, function, offset & 0xFC);
    return (uint16_t)((value >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read_config(bus, device, function, offset & 0xFC);
    return (uint8_t)((value >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t old_value = pci_read_config(bus, device, function, offset & 0xFC);
    uint32_t shift = (offset & 2) * 8;
    uint32_t new_value = (old_value & ~(0xFFFF << shift)) | ((uint32_t)value << shift);
    pci_write_config(bus, device, function, offset & 0xFC, new_value);
}

static void pci_scan_device(uint8_t bus, uint8_t device) {
    for (uint8_t function = 0; function < 8; function++) {
        uint16_t vendor_id = pci_read_config_word(bus, device, function, PCI_VENDOR_ID);
        
        if (vendor_id == 0xFFFF) {
            if (function == 0) break;
            continue;
        }
        
        if (pci_device_count >= MAX_PCI_DEVICES) {
            printf("{FG(255,165,0)}Warning: PCI device limit reached\n");
            return;
        }
        
        pci_device_t* dev = &pci_devices[pci_device_count++];
        dev->bus = bus;
        dev->device = device;
        dev->function = function;
        dev->vendor_id = vendor_id;
        dev->device_id = pci_read_config_word(bus, device, function, PCI_DEVICE_ID);
        dev->class_code = pci_read_config_byte(bus, device, function, PCI_CLASS);
        dev->subclass = pci_read_config_byte(bus, device, function, PCI_SUBCLASS);
        dev->prog_if = pci_read_config_byte(bus, device, function, PCI_PROG_IF);
        dev->revision_id = pci_read_config_byte(bus, device, function, PCI_REVISION_ID);
        dev->interrupt_line = pci_read_config_byte(bus, device, function, PCI_INTERRUPT_LINE);
        
        // Read BARs
        for (int i = 0; i < 6; i++) {
            dev->bar[i] = pci_read_config(bus, device, function, PCI_BAR0 + (i * 4));
        }
    }
}

void pci_init(void) {
    pci_device_count = 0;
    
    printf("{FG(0,255,255)}Scanning PCI bus...\n");
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            pci_scan_device(bus, device);
        }
    }
    
    printf("{FG(0,255,0)}PCI: Found %u devices\n", pci_device_count);
}

pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

void pci_enable_bus_mastering(pci_device_t* dev) {
    uint16_t command = pci_read_config_word(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_CMD_BUS_MASTER | PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE;
    pci_write_config_word(dev->bus, dev->device, dev->function, PCI_COMMAND, command);
}

void pci_list_devices(void) {
    printf("{FG(255,255,0)}PCI Devices:\n");
    printf("{FG(0,255,255)}Bus Dev Fn Vendor Device Class  Sub    IRQ\n");
    printf("==================================================\n");
    
    for (uint32_t i = 0; i < pci_device_count; i++) {
        pci_device_t* dev = &pci_devices[i];
        printf("{FG(255,255,255)}%3u %3u %2u 0x%04x 0x%04x 0x%02x   0x%02x   %3u\n",
               dev->bus, dev->device, dev->function,
               dev->vendor_id, dev->device_id,
               dev->class_code, dev->subclass,
               dev->interrupt_line);
    }
    printf("\n");
}
