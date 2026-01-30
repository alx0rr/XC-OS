#include "../include/text.h"
#include "../include/memory/pmm.h"
#include "../include/memory/vmm.h"
#include "../lib/time.h"
#include "../lib/string.h"
#include "../lib/io.h"
#include "../include/graphics/vbe.h"
extern volatile uint8_t ctrl_pressed;
void cmd_banner(int argc, char** argv) {
    if (argc == 0) {
        printf("{FG(255,0,0)}Usage: banner <text>\n");
        return;
    }
    char text[256] = {0};
    for (int i = 0; i < argc; i++) {
        strcat(text, argv[i]);
        if (i < argc - 1) strcat(text, " ");
    }
    printf("\n{FG(0,255,255)}");
    for (int i = 0; i < 60; i++) printf("=");
    printf("\n");
    int padding = (60 - strlen(text)) / 2;
    for (int i = 0; i < padding; i++) printf(" ");
    printf("{FG(255,255,0)}%s\n", text);
    printf("{FG(0,255,255)}");
    for (int i = 0; i < 60; i++) printf("=");
    printf("{FG(255,255,255)}\n\n");
}
void cmd_stat(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: stat <file>\n");
        return;
    }
    printf("{FG(255,255,0)}File: %s\n", argv[0]);
    printf("{FG(0,255,0)}Status: Not implemented yet\n");
}
void cmd_uptime() {
    uint32_t uptime_ms = get_uptime();
    uint32_t seconds = uptime_ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    seconds = seconds % 60;
    minutes = minutes % 60;
    printf("Uptime: %u hours, %u minutes, %u seconds\n", hours, minutes, seconds);
}
void memory_stress_test() {
    printf("{FG(255,255,0)}Memory Stress Test\n");
    printf("{FG(0,255,255)}==================\n\n");
    const uint32_t test_sizes[] = {4096, 8192, 16384, 32768, 65536};
    const uint32_t num_tests = 5;
    void* allocated[100] = {0};
    uint32_t alloc_count = 0;
    printf("{FG(0,255,0)}Phase 1: Allocation test\n");
    for (uint32_t t = 0; t < num_tests; t++) {
        printf("  Allocating %u bytes... ", test_sizes[t]);
        void* ptr = pmm_malloc(test_sizes[t]);
        if (ptr) {
            allocated[alloc_count++] = ptr;
            printf("{FG(0,255,0)}OK (0x%x)\n", (uint32_t)ptr);
        } else {
            printf("{FG(255,0,0)}FAILED\n");
        }
    }
    printf("\n{FG(0,255,0)}Phase 2: Write test\n");
    for (uint32_t i = 0; i < alloc_count; i++) {
        uint8_t* ptr = (uint8_t*)allocated[i];
        for (uint32_t j = 0; j < 1024; j++) {
            ptr[j] = (uint8_t)(j & 0xFF);
        }
        printf("  Block %u: {FG(0,255,0)}Write OK\n", i);
    }
    printf("\n{FG(0,255,0)}Phase 3: Read verify test\n");
    uint32_t errors = 0;
    for (uint32_t i = 0; i < alloc_count; i++) {
        uint8_t* ptr = (uint8_t*)allocated[i];
        for (uint32_t j = 0; j < 1024; j++) {
            if (ptr[j] != (uint8_t)(j & 0xFF)) {
                errors++;
            }
        }
        if (errors == 0) {
            printf("  Block %u: {FG(0,255,0)}Verify OK\n", i);
        } else {
            printf("  Block %u: {FG(255,0,0)}Verify FAILED (%u errors)\n", i, errors);
        }
    }
    printf("\n{FG(0,255,0)}Phase 4: Deallocation test\n");
    for (uint32_t i = 0; i < alloc_count; i++) {
        pmm_free(allocated[i]);
        printf("  Block %u: {FG(0,255,0)}Freed\n", i);
    }
    printf("\n{FG(0,255,255)}Test complete!\n");
    printf("{FG(255,255,255)}Total errors: %u\n\n", errors);
}
void cmd_vmm_test() {
    printf("{FG(255,255,0)}VMM Test Suite\n");
    printf("{FG(0,255,255)}==================\n\n");
    printf("{FG(0,255,0)}Test 1: Single page allocation\n");
    void* page1 = vmm_alloc_pages(1, PAGE_WRITE);
    if (page1) {
        printf("  Allocated 1 page at 0x%x\n", (uint32_t)page1);
        uint32_t phys = vmm_get_physical((uint32_t)page1);
        printf("  Physical address: 0x%x\n", phys);
        printf("  {FG(0,255,0)}PASS\n\n");
    } else {
        printf("  {FG(255,0,0)}FAIL: Cannot allocate\n\n");
        return;
    }
    printf("{FG(0,255,0)}Test 2: Multiple pages allocation\n");
    void* pages = vmm_alloc_pages(4, PAGE_WRITE);
    if (pages) {
        printf("  Allocated 4 pages at 0x%x\n", (uint32_t)pages);
        printf("  {FG(0,255,0)}PASS\n\n");
    } else {
        printf("  {FG(255,0,0)}FAIL: Cannot allocate\n\n");
        return;
    }
    printf("{FG(0,255,0)}Test 3: Write/Read test\n");
    uint32_t* test_ptr = (uint32_t*)page1;
    test_ptr[0] = 0xDEADBEEF;
    test_ptr[1] = 0xCAFEBABE;
    if (test_ptr[0] == 0xDEADBEEF && test_ptr[1] == 0xCAFEBABE) {
        printf("  Write/Read: {FG(0,255,0)}PASS\n\n");
    } else {
        printf("  Write/Read: {FG(255,0,0)}FAIL\n\n");
    }
    printf("{FG(0,255,0)}Test 4: Page mapping check\n");
    if (vmm_is_mapped((uint32_t)page1)) {
        printf("  Page is mapped: {FG(0,255,0)}PASS\n\n");
    } else {
        printf("  Page is mapped: {FG(255,0,0)}FAIL\n\n");
    }
    printf("{FG(0,255,0)}Test 5: Deallocation\n");
    vmm_free_pages(page1, 1);
    vmm_free_pages(pages, 4);
    printf("  Pages freed: {FG(0,255,0)}PASS\n\n");
    printf("{FG(0,255,255)}All tests completed!\n\n");
}
void cmd_meminfo() {
    printf("{FG(255,255,0)}Memory Information\n");
    printf("{FG(0,255,255)}==================\n\n");
    uint32_t total = pmm_get_total_memory();
    uint32_t used = pmm_get_used_memory();
    uint32_t free = pmm_get_free_memory();
    printf("{FG(0,255,0)}Physical Memory (PMM):\n");
    printf("  Total: %u MB\n", total / (1024*1024));
    printf("  Used:  %u MB\n", used / (1024*1024));
    printf("  Free:  %u MB\n", free / (1024*1024));
    printf("  Usage: %u%%\n\n", total > 0 ? (used * 100) / total : 0);
    vmm_print_stats();
}
void cmd_sysinfo() {
    datetime_t dt = time_get_datetime();
    printf("{FG(255,255,0)}System Information\n");
    printf("{FG(0,255,255)}==================\n\n");
    printf("{FG(0,255,0)}OS:\n");
    printf("  Name:    XC-OS\n");
    printf("  Version: 0.3 with VMM\n");
    printf("  Arch:    i386 (x86)\n");
    printf("  FS:      XCFS v2\n\n");
    printf("{FG(0,255,0)}Time:\n");
    printf("  Date: %02u/%02u/%u\n", dt.day, dt.month, dt.year);
    printf("  Time: %02u:%02u:%02u\n\n", dt.hour, dt.minute, dt.second);
    printf("{FG(0,255,0)}Display:\n");
    printf("  Resolution: %ux%u\n", vbe_get_width(), vbe_get_height());
    printf("  BPP: %u\n\n", vbe_get_bpp());
    cmd_uptime();
    printf("\n");
}
static const char* months[] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char* days[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
void cmd_clock() {
    uint16_t width = vbe_get_width();
    uint16_t height = vbe_get_height();
    uint32_t old_bg = bg_color;
    uint32_t old_fg = fg_color;
    uint8_t old_scale = text_get_scale();
    clear();
    printf("{FG(255,255,0)}Clock Mode - Press CTRL+C to exit\n\n");
    text_set_scale(3);
    while (1) {
        datetime_t dt = time_get_datetime();
        char time_str[32];
        char date_str[64];
        time_str[0] = '0' + (dt.hour / 10);
        time_str[1] = '0' + (dt.hour % 10);
        time_str[2] = ':';
        time_str[3] = '0' + (dt.minute / 10);
        time_str[4] = '0' + (dt.minute % 10);
        time_str[5] = ':';
        time_str[6] = '0' + (dt.second / 10);
        time_str[7] = '0' + (dt.second % 10);
        time_str[8] = '\0';
        strcpy(date_str, months[dt.month]);
        strcat(date_str, " ");
        char day_buf[3];
        day_buf[0] = '0' + (dt.day / 10);
        day_buf[1] = '0' + (dt.day % 10);
        day_buf[2] = '\0';
        strcat(date_str, day_buf);
        strcat(date_str, ", ");
        char year_buf[5];
        year_buf[0] = '0' + (dt.year / 1000);
        year_buf[1] = '0' + ((dt.year / 100) % 10);
        year_buf[2] = '0' + ((dt.year / 10) % 10);
        year_buf[3] = '0' + (dt.year % 10);
        year_buf[4] = '\0';
        strcat(date_str, year_buf);
        uint16_t time_x = (width / 2) - (strlen(time_str) * 24 * 3 / 2);
        uint16_t time_y = (height / 2) - 60;
        uint16_t date_x = (width / 2) - (strlen(date_str) * 24 * 2 / 2);
        uint16_t date_y = (height / 2) + 30;
        for (uint16_t y = time_y - 10; y < time_y + 100; y++) {
            for (uint16_t x = 0; x < width; x++) {
                uint32_t* fb = (uint32_t*)vbe_get_framebuffer();
                fb[y * width + x] = 0x000000;
            }
        }
        print_at_pos(time_str, time_x, time_y, 0x00FF00, 0x000000);
        text_set_scale(2);
        print_at_pos(date_str, date_x, date_y, 0xFFFF00, 0x000000);
        text_set_scale(3);
        for (uint32_t delay = 0; delay < 10000000; delay++) {
            if (ctrl_pressed) {
                text_set_scale(old_scale);
                bg_color = old_bg;
                fg_color = old_fg;
                clear();
                return;
            }
            asm volatile("pause");
        }
    }
}
void cmd_tree(int argc, char** argv) {
    printf("{FG(255,255,0)}Directory Tree\n");
    printf("{FG(0,255,255)}==============\n");
    printf("/ (root)\n");
    printf("  Not implemented yet\n\n");
}
void cmd_bench() {
    printf("{FG(255,255,0)}System Benchmark\n");
    printf("{FG(0,255,255)}================\n\n");
    printf("{FG(0,255,0)}CPU Test: Integer operations\n");
    uint32_t start = get_uptime();
    volatile uint32_t sum = 0;
    for (uint32_t i = 0; i < 10000000; i++) {
        sum += i;
    }
    uint32_t cpu_time = get_uptime() - start;
    printf("  Time: %u ms\n", cpu_time);
    printf("  Result: %u\n\n", sum);
    printf("{FG(0,255,0)}Memory Test: Sequential write\n");
    void* mem = pmm_malloc(1024 * 1024);
    if (mem) {
        start = get_uptime();
        uint32_t* ptr = (uint32_t*)mem;
        for (uint32_t i = 0; i < 256 * 1024; i++) {
            ptr[i] = i;
        }
        uint32_t mem_time = get_uptime() - start;
        printf("  Time: %u ms\n", mem_time);
        printf("  Bandwidth: ~%u MB/s\n\n", mem_time > 0 ? 1000 / mem_time : 0);
        pmm_free(mem);
    } else {
        printf("  {FG(255,0,0)}Cannot allocate memory\n\n");
    }
    printf("{FG(0,255,255)}Benchmark complete!\n\n");
}
