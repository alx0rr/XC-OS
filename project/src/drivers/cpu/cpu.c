#include "../../include/cpu/cpu.h"
#include "../../include/text.h"
#include <stddef.h>

static cpu_info_t cpu_info;

static inline void cpuid(uint32_t eax, uint32_t *reax, uint32_t *rebx, uint32_t *recx, uint32_t *redx) {
    asm volatile(
        "cpuid"
        : "=a" (*reax), "=b" (*rebx), "=c" (*recx), "=d" (*redx)
        : "a" (eax)
    );
}

static inline void cpuid_ecx(uint32_t eax, uint32_t ecx, uint32_t *reax, uint32_t *rebx, uint32_t *recx, uint32_t *redx) {
    asm volatile(
        "cpuid"
        : "=a" (*reax), "=b" (*rebx), "=c" (*recx), "=d" (*redx)
        : "a" (eax), "c" (ecx)
    );
}

void cpu_init() {
    uint32_t eax, ebx, ecx, edx;
    
    cpuid(0, &eax, &ebx, &ecx, &edx);
    
    cpu_info.vendor[0] = (ebx >> 0) & 0xFF;
    cpu_info.vendor[1] = (ebx >> 8) & 0xFF;
    cpu_info.vendor[2] = (ebx >> 16) & 0xFF;
    cpu_info.vendor[3] = (ebx >> 24) & 0xFF;
    cpu_info.vendor[4] = (edx >> 0) & 0xFF;
    cpu_info.vendor[5] = (edx >> 8) & 0xFF;
    cpu_info.vendor[6] = (edx >> 16) & 0xFF;
    cpu_info.vendor[7] = (edx >> 24) & 0xFF;
    cpu_info.vendor[8] = (ecx >> 0) & 0xFF;
    cpu_info.vendor[9] = (ecx >> 8) & 0xFF;
    cpu_info.vendor[10] = (ecx >> 16) & 0xFF;
    cpu_info.vendor[11] = (ecx >> 24) & 0xFF;
    cpu_info.vendor[12] = 0;
    
    uint32_t max_cpuid = eax;
    
    cpuid(1, &eax, &ebx, &ecx, &edx);
    
    cpu_info.stepping = eax & 0xF;
    cpu_info.model = (eax >> 4) & 0xF;
    cpu_info.family = (eax >> 8) & 0xF;
    
    uint32_t ext_model = (eax >> 16) & 0xF;
    uint32_t ext_family = (eax >> 20) & 0xFF;
    
    if (cpu_info.family == 0xF) {
        cpu_info.family += ext_family;
    }
    if (cpu_info.model == 0xF) {
        cpu_info.model += (ext_model << 4);
    }
    
    cpu_info.logical_cpus = (ebx >> 16) & 0xFF;
    cpu_info.features = edx;
    
    if (max_cpuid >= 4) {
        uint32_t cores_eax;
        cpuid_ecx(4, 0, &cores_eax, &ebx, &ecx, &edx);
        cpu_info.cores = ((cores_eax >> 26) & 0x3F) + 1;
    } else {
        cpu_info.cores = cpu_info.logical_cpus;
    }
    
    if (max_cpuid >= 0x80000004) {
        uint32_t brand[12];
        
        cpuid(0x80000002, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, &brand[8], &brand[9], &brand[10], &brand[11]);
        
        uint8_t* brand_str = (uint8_t*)brand;
        int idx = 0;
        
        for (int i = 0; i < 48 && idx < 48; i++) {
            if (brand_str[i] != ' ' || (idx > 0 && cpu_info.brand[idx-1] != ' ')) {
                cpu_info.brand[idx++] = brand_str[i];
            }
        }
        cpu_info.brand[idx] = 0;
    } else {
        cpu_info.brand[0] = 0;
    }
    
    if (max_cpuid >= 0x80000006) {
        cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
        cpu_info.l2_cache = (ecx >> 16) & 0xFFFF;
        cpu_info.cache_line = ecx & 0xFF;
    }
}

cpu_info_t get_cpu_info() {
    return cpu_info;
}

void cpu_print_info() {
    printf("{FG(100,200,255)}=== CPU Information ===\n");
    printf("{FG(255,255,255)}\n");
    
    printf("Vendor: {FG(0,255,0)}%s\n", cpu_info.vendor);
    printf("{FG(255,255,255)}Brand: {FG(0,255,0)}%s\n", cpu_info.brand);
    printf("{FG(255,255,255)}\n");
    
    printf("Family: {FG(100,255,255)}0x%x\n", cpu_info.family);
    printf("{FG(255,255,255)}Model: {FG(100,255,255)}0x%x\n", cpu_info.model);
    printf("{FG(255,255,255)}Stepping: {FG(100,255,255)}0x%x\n", cpu_info.stepping);
    printf("{FG(255,255,255)}\n");
    
    printf("Cores: {FG(0,255,0)}%u\n", cpu_info.cores);
    printf("{FG(255,255,255)}Logical CPUs: {FG(0,255,0)}%u\n", cpu_info.logical_cpus);
    printf("{FG(255,255,255)}\n");
    
    if (cpu_info.l2_cache) {
        printf("L2 Cache: {FG(255,255,0)}%u KB\n", cpu_info.l2_cache);
    }
    if (cpu_info.cache_line) {
        printf("{FG(255,255,255)}Cache Line: {FG(255,255,0)}%u bytes\n", cpu_info.cache_line);
    }
    printf("{FG(255,255,255)}\n");
    
    printf("Features: ");
    if (cpu_has_feature(CPU_FEATURE_FPU)) printf("{FG(0,255,0)}FPU ");
    if (cpu_has_feature(CPU_FEATURE_TSC)) printf("TSC ");
    if (cpu_has_feature(CPU_FEATURE_MSR)) printf("MSR ");
    if (cpu_has_feature(CPU_FEATURE_PAE)) printf("PAE ");
    if (cpu_has_feature(CPU_FEATURE_APIC)) printf("APIC ");
    if (cpu_has_feature(CPU_FEATURE_MMX)) printf("MMX ");
    if (cpu_has_feature(CPU_FEATURE_SSE)) printf("SSE ");
    if (cpu_has_feature(CPU_FEATURE_SSE2)) printf("SSE2 ");
    if (cpu_has_feature(CPU_FEATURE_HTT)) printf("HTT ");
    printf("{FG(255,255,255)}\n\n");
}

uint8_t cpu_has_feature(uint32_t feature) {
    return (cpu_info.features & feature) != 0;
}