#ifndef CPU_H
#define CPU_H
#include <stdint.h>

typedef struct {
    char vendor[13];
    char brand[49];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t cores;
    uint32_t logical_cpus;
    uint32_t cache_line;
    uint32_t l2_cache;
    uint32_t l3_cache;
    uint32_t features;
} cpu_info_t;

#define CPU_FEATURE_FPU     (1 << 0)
#define CPU_FEATURE_VME     (1 << 1)
#define CPU_FEATURE_DE      (1 << 2)
#define CPU_FEATURE_PSE     (1 << 3)
#define CPU_FEATURE_TSC     (1 << 4)
#define CPU_FEATURE_MSR     (1 << 5)
#define CPU_FEATURE_PAE     (1 << 6)
#define CPU_FEATURE_MCE     (1 << 7)
#define CPU_FEATURE_CX8     (1 << 8)
#define CPU_FEATURE_APIC    (1 << 9)
#define CPU_FEATURE_SEP     (1 << 11)
#define CPU_FEATURE_MTRR    (1 << 12)
#define CPU_FEATURE_PGE     (1 << 13)
#define CPU_FEATURE_MCA     (1 << 14)
#define CPU_FEATURE_CMOV    (1 << 15)
#define CPU_FEATURE_PAT     (1 << 16)
#define CPU_FEATURE_PSE36   (1 << 17)
#define CPU_FEATURE_MMX     (1 << 23)
#define CPU_FEATURE_SSE     (1 << 25)
#define CPU_FEATURE_SSE2    (1 << 26)
#define CPU_FEATURE_HTT     (1 << 28)

void cpu_init();
cpu_info_t get_cpu_info();
void cpu_print_info();
uint8_t cpu_has_feature(uint32_t feature);

#endif