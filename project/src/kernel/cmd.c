/* 1 February 2026 */
/* /ᐠ - ˕ -マ forker-25 presents */
/* XC-OS CMD's */

#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../include/fs/xcfs.h"
#include "../include/memory/pmm.h"
#include "../include/memory/vmm.h"
#include "../include/cpu/cpu.h"
#include "../include/graphics/vbe.h"
#include "../include/scheduler/scheduler.h"
#include "../include/timer/pit.h"
#include "../lib/string.h"
#include "../lib/time.h"
#include "../lib/io.h"

extern volatile uint8_t ctrl_pressed;

void cmd_ls(int argc, char** argv) {
    const char* path = (argc > 0) ? argv[0] : NULL;
    xcfs_list(path);
}

void cmd_cd(int argc, char** argv) {
    if (argc < 1) {
        xcfs_chdir("/");
        return;
    }
    if (xcfs_chdir(argv[0]) < 0) {
        printf("{FG(255,0,0)}cd: %s: no such directory\n", argv[0]);
    }
}

void cmd_pwd(void) {
    printf("%s\n", xcfs_getcwd());
}

void cmd_mkdir(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: mkdir <directory>\n");
        return;
    }
    if (xcfs_mkdir(argv[0]) < 0) {
        printf("{FG(255,0,0)}mkdir: cannot create directory '%s'\n", argv[0]);
    } else {
        printf("{FG(0,255,0)}Created directory: %s\n", argv[0]);
    }
}

void cmd_cat(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: cat <file>\n");
        return;
    }
    uint8_t buffer[8192];
    if (xcfs_read(argv[0], buffer, 8192) == 0) {
        printf("{FG(0,255,0)}");
        for (int i = 0; i < 8192 && buffer[i]; i++) {
            printf("%c", buffer[i]);
        }
        printf("{FG(255,255,255)}\n");
    } else {
        printf("{FG(255,0,0)}cat: %s: no such file\n", argv[0]);
    }
}

void cmd_echo(int argc, char** argv) {
    if (argc == 0) {
        printf("\n");
        return;
    }

    int write_argc = argc;
    const char* filename = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            filename   = argv[i + 1];
            write_argc = i;
            break;
        }
    }

    if (filename) {
        char text[1024] = {0};
        for (int i = 0; i < write_argc; i++) {
            strcat(text, argv[i]);
            if (i < write_argc - 1) strcat(text, " ");
        }
        xcfs_delete(filename);
        if (xcfs_create(filename, strlen(text)) == 0) {
            xcfs_write(filename, (uint8_t*)text, strlen(text));
            printf("{FG(0,255,0)}Written to %s\n", filename);
        } else {
            printf("{FG(255,0,0)}echo: cannot create %s\n", filename);
        }
    } else {
        for (int i = 0; i < argc; i++) {
            printf("%s", argv[i]);
            if (i < argc - 1) printf(" ");
        }
        printf("\n");
    }
}

void cmd_touch(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: touch <file>\n");
        return;
    }
    if (xcfs_create(argv[0], 0) == 0) {
        printf("{FG(0,255,0)}Created: %s\n", argv[0]);
    } else {
        printf("{FG(255,0,0)}touch: cannot create '%s'\n", argv[0]);
    }
}

void cmd_rm(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: rm <file>\n");
        return;
    }
    if (xcfs_delete(argv[0]) == 0) {
        printf("{FG(0,255,0)}Removed: %s\n", argv[0]);
    } else {
        printf("{FG(255,0,0)}rm: cannot remove '%s'\n", argv[0]);
    }
}

void cmd_cp(int argc, char** argv) {
    if (argc < 2) {
        printf("{FG(255,0,0)}Usage: cp <source> <dest>\n");
        return;
    }
    uint8_t buffer[8192];
    if (xcfs_read(argv[0], buffer, 8192) != 0) {
        printf("{FG(255,0,0)}cp: cannot read '%s'\n", argv[0]);
        return;
    }
    uint32_t size = 0;
    for (int i = 0; i < 8192 && buffer[i]; i++) size++;

    xcfs_delete(argv[1]);
    if (xcfs_create(argv[1], size) == 0) {
        xcfs_write(argv[1], buffer, size);
        printf("{FG(0,255,0)}Copied: %s -> %s\n", argv[0], argv[1]);
    } else {
        printf("{FG(255,0,0)}cp: cannot create '%s'\n", argv[1]);
    }
}

void cmd_mv(int argc, char** argv) {
    if (argc < 2) {
        printf("{FG(255,0,0)}Usage: mv <source> <dest>\n");
        return;
    }
    cmd_cp(argc, argv);
    if (xcfs_delete(argv[0]) != 0) {
        printf("{FG(255,165,0)}Warning: could not remove source file\n");
    }
}

void cmd_stat(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: stat <file>\n");
        return;
    }
    printf("{FG(255,255,0)}File: %s\n", argv[0]);
    printf("{FG(0,255,0)}Status: Not implemented yet\n");
}

void cmd_tree(int argc, char** argv) {
    printf("{FG(255,255,0)}Directory Tree\n");
    printf("{FG(0,255,255)}==============\n");
    printf("/ (root)\n");
    printf("  Not implemented yet\n\n");
}

void cmd_uname(void) {
    printf("XC-OS v0.3 i386 (XCFS v2)\n");
}

void cmd_free(void) {
    pmm_print_stats();
}

void cmd_uptime(void) {
    uint32_t uptime_ms = get_uptime();
    uint32_t seconds   = uptime_ms / 1000;
    uint32_t minutes   = seconds / 60;
    uint32_t hours     = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    printf("Uptime: %u hours, %u minutes, %u seconds\n",
           hours, minutes, seconds);
}

static const char* months[] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

void cmd_clock(void) {
    datetime_t dt = time_get_datetime();
    printf("{FG(0,255,255)}Current Date & Time:\n");
    printf("{FG(255,255,0)}  Date: %s %02u, %u\n",
           months[dt.month], dt.day, dt.year);
    printf("{FG(0,255,0)}  Time: %02u:%02u:%02u\n\n",
           dt.hour, dt.minute, dt.second);
}

void cmd_meminfo(void) {
    printf("{FG(255,255,0)}Memory Information\n");
    printf("{FG(0,255,255)}==================\n\n");

    uint32_t total = pmm_get_total_memory();
    uint32_t used  = pmm_get_used_memory();
    uint32_t free  = pmm_get_free_memory();

    printf("{FG(0,255,0)}Physical Memory (PMM):\n");
    printf("  Total: %u MB\n",  total / (1024*1024));
    printf("  Used:  %u MB\n",  used  / (1024*1024));
    printf("  Free:  %u MB\n",  free  / (1024*1024));
    printf("  Usage: %u%%\n\n", total > 0 ? (used * 100) / total : 0);

    vmm_print_stats();
}

void cmd_sysinfo(void) {
    datetime_t dt = time_get_datetime();

    printf("{FG(255,255,0)}System Information\n");
    printf("{FG(0,255,255)}==================\n\n");
    printf("{FG(0,255,0)}OS:\n");
    printf("  Name:    XC-OS\n");
    printf("  Version: 0.3 with VMM\n");
    printf("  Arch:    i386 (x86)\n");
    printf("  FS:      XCFS v2\n\n");
    printf("{FG(0,255,0)}Time:\n");
    printf("  Date: %02u/%02u/%u\n",    dt.day, dt.month, dt.year);
    printf("  Time: %02u:%02u:%02u\n\n", dt.hour, dt.minute, dt.second);
    printf("{FG(0,255,0)}Display:\n");
    printf("  Resolution: %ux%u\n", vbe_get_width(), vbe_get_height());
    printf("  BPP: %u\n\n", vbe_get_bpp());

    cmd_uptime();
    printf("\n");
}

void cmd_memtest(void) {
    printf("{FG(255,255,0)}Memory Stress Test\n");
    printf("{FG(0,255,255)}==================\n\n");

    const uint32_t test_sizes[] = {4096, 8192, 16384, 32768, 65536};
    const uint32_t num_tests    = 5;

    void*    allocated[100] = {0};
    uint32_t alloc_count    = 0;

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
        for (uint32_t j = 0; j < 1024; j++)
            ptr[j] = (uint8_t)(j & 0xFF);
        printf("  Block %u: {FG(0,255,0)}Write OK\n", i);
    }

    printf("\n{FG(0,255,0)}Phase 3: Read verify test\n");
    uint32_t errors = 0;
    for (uint32_t i = 0; i < alloc_count; i++) {
        uint8_t* ptr = (uint8_t*)allocated[i];
        uint32_t block_errors = 0;
        for (uint32_t j = 0; j < 1024; j++) {
            if (ptr[j] != (uint8_t)(j & 0xFF))
                block_errors++;
        }
        errors += block_errors;
        if (block_errors == 0)
            printf("  Block %u: {FG(0,255,0)}Verify OK\n", i);
        else
            printf("  Block %u: {FG(255,0,0)}Verify FAILED (%u errors)\n", i, block_errors);
    }

    printf("\n{FG(0,255,0)}Phase 4: Deallocation test\n");
    for (uint32_t i = 0; i < alloc_count; i++) {
        pmm_free(allocated[i]);
        printf("  Block %u: {FG(0,255,0)}Freed\n", i);
    }

    printf("\n{FG(0,255,255)}Test complete!\n");
    printf("{FG(255,255,255)}Total errors: %u\n\n", errors);
}

void cmd_vmmtest(void) {
    printf("{FG(255,255,0)}VMM Test Suite\n");
    printf("{FG(0,255,255)}==================\n\n");

    printf("{FG(0,255,0)}Test 1: Single page allocation\n");
    void* page1 = vmm_alloc_pages(1, PAGE_WRITE);
    if (!page1) {
        printf("  {FG(255,0,0)}FAIL: Cannot allocate\n\n");
        return;
    }
    printf("  Allocated 1 page at 0x%x\n", (uint32_t)page1);
    printf("  Physical address: 0x%x\n", vmm_get_physical((uint32_t)page1));
    printf("  {FG(0,255,0)}PASS\n\n");

    printf("{FG(0,255,0)}Test 2: Multiple pages allocation\n");
    void* pages = vmm_alloc_pages(4, PAGE_WRITE);
    if (!pages) {
        printf("  {FG(255,0,0)}FAIL: Cannot allocate\n\n");
        vmm_free_pages(page1, 1);
        return;
    }
    printf("  Allocated 4 pages at 0x%x\n", (uint32_t)pages);
    printf("  {FG(0,255,0)}PASS\n\n");

    printf("{FG(0,255,0)}Test 3: Write/Read test\n");
    uint32_t* test_ptr = (uint32_t*)page1;
    test_ptr[0] = 0xDEADBEEF;
    test_ptr[1] = 0xCAFEBABE;
    if (test_ptr[0] == 0xDEADBEEF && test_ptr[1] == 0xCAFEBABE)
        printf("  Write/Read: {FG(0,255,0)}PASS\n\n");
    else
        printf("  Write/Read: {FG(255,0,0)}FAIL\n\n");

    printf("{FG(0,255,0)}Test 4: Page mapping check\n");
    if (vmm_is_mapped((uint32_t)page1))
        printf("  Page is mapped: {FG(0,255,0)}PASS\n\n");
    else
        printf("  Page is mapped: {FG(255,0,0)}FAIL\n\n");

    printf("{FG(0,255,0)}Test 5: Deallocation\n");
    vmm_free_pages(page1, 1);
    vmm_free_pages(pages, 4);
    printf("  Pages freed: {FG(0,255,0)}PASS\n\n");

    printf("{FG(0,255,255)}All tests completed!\n\n");
}

void cmd_bench(void) {
    printf("{FG(255,255,0)}System Benchmark\n");
    printf("{FG(0,255,255)}================\n\n");

    printf("{FG(0,255,0)}CPU Test: Integer operations\n");
    uint32_t start = get_uptime();
    volatile uint32_t sum = 0;
    for (uint32_t i = 0; i < 10000000; i++)
        sum += i;
    uint32_t cpu_time = get_uptime() - start;
    printf("  Time: %u ms\n", cpu_time);
    printf("  Result: %u\n\n", sum);

    printf("{FG(0,255,0)}Memory Test: Sequential write\n");
    void* mem = pmm_malloc(1024 * 1024);
    if (mem) {
        start = get_uptime();
        uint32_t* ptr = (uint32_t*)mem;
        for (uint32_t i = 0; i < 256 * 1024; i++)
            ptr[i] = i;
        uint32_t mem_time = get_uptime() - start;
        printf("  Time: %u ms\n", mem_time);
        printf("  Bandwidth: ~%u MB/s\n\n",
               mem_time > 0 ? 1000 / mem_time : 0);
        pmm_free(mem);
    } else {
        printf("  {FG(255,0,0)}Cannot allocate memory\n\n");
    }

    printf("{FG(0,255,255)}Benchmark complete!\n\n");
}

static void demo_task_1(void) {
    uint32_t counter = 0;
    while (1) {
        printf("{FG(255,100,100)}[Task 1] Counter: %u\n", counter++);
        task_sleep(1000);
        if (counter >= 5) {
            printf("{FG(255,100,100)}[Task 1] Exiting after 5 iterations\n");
            task_exit();
        }
    }
}

static void demo_task_2(void) {
    uint32_t counter = 0;
    while (1) {
        printf("{FG(100,255,100)}[Task 2] Counter: %u\n", counter++);
        task_sleep(1500);
        if (counter >= 5) {
            printf("{FG(100,255,100)}[Task 2] Exiting after 5 iterations\n");
            task_exit();
        }
    }
}

static void demo_task_3(void) {
    uint32_t counter = 0;
    while (1) {
        printf("{FG(100,100,255)}[Task 3] Counter: %u\n", counter++);
        task_sleep(2000);
        if (counter >= 5) {
            printf("{FG(100,100,255)}[Task 3] Exiting after 5 iterations\n");
            task_exit();
        }
    }
}

void cmd_taskdemo(void) {
    printf("{FG(255,255,0)}=== Multitasking Demo ===\n\n");
    printf("{FG(0,255,255)}Creating tasks...\n");

    task_create("Demo Task 1", demo_task_1, 0);
    task_create("Demo Task 2", demo_task_2, 0);
    task_create("Demo Task 3", demo_task_3, 0);

    printf("{FG(0,255,0)}Tasks created! Starting scheduler...\n\n");
    scheduler_start();

    printf("{FG(255,255,0)}Scheduler is now running!\n");
    printf("{FG(0,255,255)}Watch the tasks execute in parallel...\n\n");
}

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

    int padding = (60 - (int)strlen(text)) / 2;
    for (int i = 0; i < padding; i++) printf(" ");
    printf("{FG(255,255,0)}%s\n", text);

    printf("{FG(0,255,255)}");
    for (int i = 0; i < 60; i++) printf("=");
    printf("{FG(255,255,255)}\n\n");
}

void cmd_timer(void) {
    printf("{FG(0,255,255)}System Timer Information\n");
    printf("========================\n\n");
    u64 ticks = pit_get_ticks();
    u32 ticks_low = (u32)ticks;
    printf("Ticks since boot: {FG(255,255,0)}%u\n", ticks_low);
    printf("{FG(255,255,255)}Seconds since boot: {FG(255,255,0)}%u\n", ticks_low / 1000);
}

void cmd_sleep(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: sleep <milliseconds>\n");
        return;
    }
    
    u32 ms = 0;
    for (int i = 0; argv[0][i]; i++) {
        if (argv[0][i] >= '0' && argv[0][i] <= '9') {
            ms = ms * 10 + (argv[0][i] - '0');
        } else {
            printf("{FG(255,0,0)}Invalid number\n");
            return;
        }
    }
    
    printf("Sleeping for %u ms...\n", ms);
    pit_sleep(ms);
    printf("{FG(0,255,0)}Done!\n");
}

void cmd_countdown(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: countdown <seconds>\n");
        return;
    }
    
    u32 seconds = 0;
    for (int i = 0; argv[0][i]; i++) {
        if (argv[0][i] >= '0' && argv[0][i] <= '9') {
            seconds = seconds * 10 + (argv[0][i] - '0');
        } else {
            printf("{FG(255,0,0)}Invalid number\n");
            return;
        }
    }
    
    if (seconds > 60) {
        printf("{FG(255,0,0)}Maximum 60 seconds\n");
        return;
    }
    
    printf("{FG(255,255,0)}Countdown started...\n");
    for (u32 i = seconds; i > 0; i--) {
        printf("{FG(0,255,255)}%u... ", i);
        pit_sleep(1000);
    }
    printf("{FG(0,255,0)}\nDone!\n");
}

void cmd_pitbench(void) {
    printf("{FG(255,255,0)}PIT Accuracy Test\n");
    printf("{FG(0,255,255)}=================\n\n");
    
    printf("Testing 1 second delay...\n");
    u64 start = pit_get_ticks();
    pit_sleep(1000);
    u64 end = pit_get_ticks();
    u32 elapsed = (u32)(end - start);
    
    printf("Expected ticks: 1000\n");
    printf("Actual ticks:   {FG(255,255,0)}%u\n", elapsed);
    printf("{FG(255,255,255)}Accuracy:       {FG(0,255,0)}%u.%02u%%\n\n", 
           (elapsed * 100) / 1000,
           ((elapsed * 10000) / 1000) % 100);
    
    printf("Testing 5 second delay...\n");
    start = pit_get_ticks();
    pit_sleep(5000);
    end = pit_get_ticks();
    elapsed = (u32)(end - start);
    
    printf("Expected ticks: 5000\n");
    printf("Actual ticks:   {FG(255,255,0)}%u\n", elapsed);
    printf("{FG(255,255,255)}Accuracy:       {FG(0,255,0)}%u.%02u%%\n\n",
           (elapsed * 100) / 5000,
           ((elapsed * 10000) / 5000) % 100);
}

void cmd_help(void) {
    printf("{FG(0,255,255)}=== System Commands ===\n");
    printf("  {FG(255,255,0)}help{FG(255,255,255)}      - Show this help\n");
    printf("  {FG(255,255,0)}clear{FG(255,255,255)}     - Clear screen\n");
    printf("  {FG(255,255,0)}reboot{FG(255,255,255)}    - Reboot system\n");
    printf("  {FG(255,255,0)}poweroff{FG(255,255,255)}  - Turn off your computer (works only on the emulator)\n");
    printf("  {FG(255,255,0)}uname{FG(255,255,255)}     - System information\n");
    printf("  {FG(255,255,0)}sysinfo{FG(255,255,255)}   - Detailed system info\n");
    printf("  {FG(255,255,0)}free{FG(255,255,255)}      - Memory usage\n");
    printf("  {FG(255,255,0)}meminfo{FG(255,255,255)}   - Detailed memory info\n");
    printf("  {FG(255,255,0)}vmmstat{FG(255,255,255)}   - Virtual memory stats\n");
    printf("  {FG(255,255,0)}cpu{FG(255,255,255)}       - CPU information\n");
    printf("  {FG(255,255,0)}uptime{FG(255,255,255)}    - System uptime\n");
    printf("  {FG(255,255,0)}clock{FG(255,255,255)}     - Show date and time\n");
    printf("  {FG(255,255,0)}timer{FG(255,255,255)}     - Show timer ticks\n");
    printf("  {FG(255,255,0)}sleep{FG(255,255,255)} <ms>- Sleep for milliseconds\n");
    printf("  {FG(255,255,0)}countdown{FG(255,255,255)} <s> - Countdown timer\n");
    printf("\n");

    printf("{FG(0,255,255)}=== Directory Commands ===\n");
    printf("  {FG(255,255,0)}ls{FG(255,255,255)} [dir]  - List files\n");
    printf("  {FG(255,255,0)}cd{FG(255,255,255)} <dir>  - Change directory\n");
    printf("  {FG(255,255,0)}pwd{FG(255,255,255)}       - Print working directory\n");
    printf("  {FG(255,255,0)}mkdir{FG(255,255,255)} <d> - Create directory\n");
    printf("  {FG(255,255,0)}tree{FG(255,255,255)} [d]  - Show directory tree\n");
    printf("\n");

    printf("{FG(0,255,255)}=== File Commands ===\n");
    printf("  {FG(255,255,0)}cat{FG(255,255,255)} <file> - Show file content\n");
    printf("  {FG(255,255,0)}touch{FG(255,255,255)} <f>  - Create empty file\n");
    printf("  {FG(255,255,0)}rm{FG(255,255,255)} <file>  - Remove file/dir\n");
    printf("  {FG(255,255,0)}cp{FG(255,255,255)} <s> <d> - Copy file\n");
    printf("  {FG(255,255,0)}mv{FG(255,255,255)} <s> <d> - Move file\n");
    printf("  {FG(255,255,0)}echo{FG(255,255,255)} <txt> - Print text\n");
    printf("  {FG(255,255,0)}echo{FG(255,255,255)} <t> > <f> - Write to file\n");
    printf("  {FG(255,255,0)}stat{FG(255,255,255)} <f>  - File information\n");
    printf("\n");

    printf("{FG(0,255,255)}=== Test & Debug Commands ===\n");
    printf("  {FG(255,255,0)}memtest{FG(255,255,255)}   - Memory stress test\n");
    printf("  {FG(255,255,0)}vmmtest{FG(255,255,255)}   - VMM test suite\n");
    printf("  {FG(255,255,0)}bench{FG(255,255,255)}     - System benchmark\n");
    printf("  {FG(255,255,0)}pitbench{FG(255,255,255)}  - PIT accuracy test\n");
    printf("\n");

    printf("{FG(0,255,255)}=== Multitasking Commands ===\n");
    printf("  {FG(255,255,0)}ps{FG(255,255,255)}        - List all tasks\n");
    printf("  {FG(255,255,0)}taskdemo{FG(255,255,255)}  - Run multitasking demo\n");
    printf("\n");

    printf("{FG(0,255,255)}=== Other Commands ===\n");
    printf("  {FG(255,255,0)}banner{FG(255,255,255)} <t> - Display banner\n");
    printf("\n");
}
