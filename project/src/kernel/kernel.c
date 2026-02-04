/* 1 February 2026 */
/* /ᐠ - ˕ -マ forker-25 presents */
/* XC-OS Kernel */

#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../include/fs/xcfs.h"
#include "../include/memory/vmm.h"
#include "../include/cpu/cpu.h"
#include "../include/scheduler/scheduler.h"
#include "../lib/io.h"
#include "../lib/string.h"
#include "startup.c"
#include "cmd.c"

static void parse_command(char* input, char* cmd, char* args[], int* argc) {
    int i = 0, j = 0;
    *argc = 0;

    while (input[i] && input[i] == ' ') i++;

    while (input[i] && input[i] != ' ' && j < 63)
        cmd[j++] = input[i++];
    cmd[j] = '\0';

    while (input[i] && *argc < 16) {
        while (input[i] == ' ') i++;
        if (!input[i]) break;

        args[*argc] = (char*)pmm_malloc(128);
        if (!args[*argc]) break;

        j = 0;
        while (input[i] && input[i] != ' ' && j < 127)
            args[*argc][j++] = input[i++];
        args[*argc][j] = '\0';
        (*argc)++;
    }
}

static void free_args(char* args[], int argc) {
    for (int i = 0; i < argc; i++) {
        if (args[i]) pmm_free(args[i]);
    }
}


void poweroff() {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    asm volatile("cli; hlt");
}




void kernel_main(void) {
    startup();

    printf("{FG(0,255,0)}Welcome to {FG(194,122,255)}XC-OS v0.3{FG(0,255,0)}\n");
    printf("\nType '{FG(255,255,0)}help{FG(255,255,255)}' for available commands\n\n");

    while (1) {
        printf("{FG(194,122,255)}root@xcos{FG(255,255,255)}:{FG(100,200,255)}%s{FG(255,255,255)}$ ",
               xcfs_getcwd());

        char* input = keyboard_input();
        printf("\n");

        if (strlen(input) == 0) continue;

        char  cmd[64]    = {0};
        char* args[16]   = {0};
        int   argc       = 0;
        parse_command(input, cmd, args, &argc);

        if      (strcmp(cmd, "help")     == 0)  cmd_help();
        else if (strcmp(cmd, "clear")    == 0)  clear();
        else if (strcmp(cmd, "ls")       == 0)  cmd_ls(argc, args);
        else if (strcmp(cmd, "cd")       == 0)  cmd_cd(argc, args);
        else if (strcmp(cmd, "pwd")      == 0)  cmd_pwd();
        else if (strcmp(cmd, "mkdir")    == 0)  cmd_mkdir(argc, args);
        else if (strcmp(cmd, "cat")      == 0)  cmd_cat(argc, args);
        else if (strcmp(cmd, "echo")     == 0)  cmd_echo(argc, args);
        else if (strcmp(cmd, "touch")    == 0)  cmd_touch(argc, args);
        else if (strcmp(cmd, "rm")       == 0)  cmd_rm(argc, args);
        else if (strcmp(cmd, "cp")       == 0)  cmd_cp(argc, args);
        else if (strcmp(cmd, "mv")       == 0)  cmd_mv(argc, args);
        else if (strcmp(cmd, "stat")     == 0)  cmd_stat(argc, args);
        else if (strcmp(cmd, "tree")     == 0)  cmd_tree(argc, args);
        else if (strcmp(cmd, "uname")    == 0)  cmd_uname();
        else if (strcmp(cmd, "free")     == 0)  cmd_free();
        else if (strcmp(cmd, "uptime")   == 0)  cmd_uptime();
        else if (strcmp(cmd, "clock")    == 0)  cmd_clock();
        else if (strcmp(cmd, "timer")    == 0)  cmd_timer();
        else if (strcmp(cmd, "sleep")    == 0)  cmd_sleep(argc, args);
        else if (strcmp(cmd, "countdown") == 0) cmd_countdown(argc, args);
        else if (strcmp(cmd, "meminfo")  == 0)  cmd_meminfo();
        else if (strcmp(cmd, "sysinfo")  == 0)  cmd_sysinfo();
        else if (strcmp(cmd, "vmmstat")  == 0)  vmm_print_stats();
        else if (strcmp(cmd, "cpu")      == 0)  cpu_print_info();
        else if (strcmp(cmd, "memtest")  == 0)  cmd_memtest();
        else if (strcmp(cmd, "vmmtest")  == 0)  cmd_vmmtest();
        else if (strcmp(cmd, "bench")    == 0)  cmd_bench();
        else if (strcmp(cmd, "pitbench") == 0)  cmd_pitbench();
        else if (strcmp(cmd, "ps")       == 0)  scheduler_print_tasks();
        else if (strcmp(cmd, "taskdemo") == 0)  cmd_taskdemo();
        else if (strcmp(cmd, "banner")   == 0)  cmd_banner(argc, args);
        else if (strcmp(cmd, "poweroff") == 0)  poweroff(); // bruh
        else if (strcmp(cmd, "reboot")   == 0) {
            printf("{FG(255,255,0)}Rebooting...\n");
            outb(0x64, 0xFE);
        }
        else if (strlen(cmd) > 0) {
            printf("{FG(255,0,0)}%s: command not found\n", cmd);
        }

        free_args(args, argc);
    }
}
