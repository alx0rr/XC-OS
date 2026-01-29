#include "../../include/memory/pmm.h"
#include "../../include/text.h"

static inline uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                       ((uint32_t)func << 8) | (offset & 0xFC);
    
    __asm__ volatile(
        "mov %0, %%eax\n"
        "mov $0xCF8, %%dx\n"
        "outl %%eax, %%dx"
        : : "r"(address) : "eax", "edx"
    );
    
    uint32_t value;
    __asm__ volatile(
        "mov $0xCFC, %%dx\n"
        "inl %%dx, %%eax"
        : "=a"(value) : : "edx"
    );
    
    return value;
}

static uint32_t detect_memory_via_intel_northbridge() {
    uint32_t vendor_device = pci_read(0, 0, 0, 0x00);
    uint16_t vendor = vendor_device & 0xFFFF;
    
    if (vendor != 0x8086) {
        return 0;
    }
    
    uint32_t tom = pci_read(0, 0, 0, 0xA0);
    if (tom != 0xFFFFFFFF && tom > 0) {
        uint32_t memory = (tom & 0xFFFFF000) + 0x100000;
        printf("Intel Northbridge detected: %u MB\n", memory / (1024*1024));
        return memory;
    }
    
    return 0;
}

static uint32_t detect_memory_via_amd_northbridge() {
    uint32_t vendor_device = pci_read(0, 0, 0, 0x00);
    uint16_t vendor = vendor_device & 0xFFFF;
    
    if (vendor != 0x1022) {
        return 0;
    }
    
    uint32_t drambase = pci_read(0, 24, 2, 0x40);
    uint32_t dramlimit = pci_read(0, 24, 2, 0x44);
    
    if (dramlimit != 0 && dramlimit != 0xFFFFFFFF) {
        uint32_t memory = ((dramlimit & 0xFFFF0000) | 0xFFFF) + 1;
        printf("AMD Northbridge detected: %u MB\n", memory / (1024*1024));
        return memory;
    }
    
    return 0;
}

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

static uint8_t acpi_checksum(void* ptr, uint32_t length) {
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*)ptr;
    for (uint32_t i = 0; i < length; i++) {
        sum += bytes[i];
    }
    return sum;
}

static acpi_rsdp_t* find_acpi_rsdp() {
    uint8_t* ptr = (uint8_t*)0xE0000;
    
    while (ptr < (uint8_t*)0x100000) {
        if (ptr[0] == 'R' && ptr[1] == 'S' && ptr[2] == 'D' && ptr[3] == ' ' &&
            ptr[4] == 'P' && ptr[5] == 'T' && ptr[6] == 'R' && ptr[7] == ' ') {
            
            acpi_rsdp_t* rsdp = (acpi_rsdp_t*)ptr;
            
            if (acpi_checksum(rsdp, 20) == 0) {
                printf("ACPI RSDP found at 0x%x\n", (uint32_t)ptr);
                return rsdp;
            }
        }
        ptr += 16;
    }
    
    return 0;
}

static uint32_t detect_memory_via_acpi() {
    acpi_rsdp_t* rsdp = find_acpi_rsdp();
    if (!rsdp) {
        return 0;
    }
    
    acpi_sdt_header_t* rsdt = (acpi_sdt_header_t*)(uintptr_t)rsdp->rsdt_address;
    
    if (rsdt->signature[0] != 'R' || rsdt->signature[1] != 'S' || 
        rsdt->signature[2] != 'D' || rsdt->signature[3] != 'T') {
        return 0;
    }
    
    if (acpi_checksum(rsdt, rsdt->length) != 0) {
        return 0;
    }
    
    uint32_t entries = (rsdt->length - sizeof(acpi_sdt_header_t)) / 4;
    uint32_t* entry_ptr = (uint32_t*)(rsdt + 1);
    
    for (uint32_t i = 0; i < entries; i++) {
        acpi_sdt_header_t* sdt = (acpi_sdt_header_t*)(uintptr_t)entry_ptr[i];
        
        if (sdt->signature[0] == 'S' && sdt->signature[1] == 'R' && 
            sdt->signature[2] == 'A' && sdt->signature[3] == 'T') {
            
            printf("ACPI SRAT table found\n");
            return 0;
        }
    }
    
    return 0;
}

static uint32_t read_cmos_extended_memory() {
    uint8_t low, high;
    
    __asm__ volatile(
        "movb $0x17, %%al\n"
        "movw $0x70, %%dx\n"
        "outb %%al, %%dx\n"
        "movw $0x71, %%dx\n"
        "inb %%dx, %%al\n"
        "movb %%al, %0\n"
        
        "movb $0x18, %%al\n"
        "movw $0x70, %%dx\n"
        "outb %%al, %%dx\n"
        "movw $0x71, %%dx\n"
        "inb %%dx, %%al"
        : "=m"(low), "=m"(high)
        :
        : "eax", "edx"
    );
    
    uint32_t kb = ((uint32_t)high << 8) | low;
    
    if (kb > 1024 && kb < 262144) {
        uint32_t bytes = (kb + 1024) * 1024;
        printf("CMOS extended memory: %u MB\n", bytes / (1024*1024));
        return bytes;
    }
    
    return 0;
}

uint32_t detect_memory_advanced() {
    uint32_t memory = 0;
    
    printf("\n=== Advanced Memory Detection ===\n");
    
    memory = detect_memory_via_intel_northbridge();
    if (memory > 0) return memory;
    
    memory = detect_memory_via_amd_northbridge();
    if (memory > 0) return memory;
    
    memory = detect_memory_via_acpi();
    if (memory > 0) return memory;
    
    memory = read_cmos_extended_memory();
    if (memory > 0) return memory;
    
    printf("All advanced methods failed\n");
    return 0;
}
