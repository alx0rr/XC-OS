#include "../include/kshell/kshell.h"
#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../include/fs/xcfs.h"
#include "../include/memory/pmm.h"
#include "../lib/string.h"
#include "../lib/io.h"
#include "../include/editor.h"
#include "../include/cpu/cpu.h"
#include "../include/memory/vmm.h"
#include "../include/graphics/vbe.h"
#include "../include/graphics/framebuffer.h"
#include "../include/timer/pit.h"
#include "../include/sound/pcspk.h"
#include "../lib/time.h"

extern void kshell_cmd_help(void);
extern void kshell_cmd_ls(int, char**);
extern void kshell_cmd_cd(int, char**);
extern void kshell_cmd_pwd(void);
extern void kshell_cmd_mkdir(int, char**);
extern void kshell_cmd_rsof(void);
extern void kshell_cmd_cat(int, char**);
extern void kshell_cmd_echo(int, char**);
extern void kshell_cmd_touch(int, char**);
extern void kshell_cmd_rm(int, char**);
extern int  kshell_cmd_cp(int, char**);
extern void kshell_cmd_mv(int, char**);
extern void kshell_cmd_stat(int, char**);
extern void kshell_cmd_tree(int, char**);
extern void kshell_cmd_uname(void);
extern void kshell_cmd_free(void);
extern void kshell_cmd_uptime(void);
extern void kshell_cmd_clock(void);
extern void kshell_cmd_timer(void);
extern void kshell_cmd_sleep(int, char**);
extern void kshell_cmd_countdown(int, char**);
extern void kshell_cmd_meminfo(void);
extern void kshell_cmd_sysinfo(void);
extern void vmm_print_stats(void);
extern void cpu_print_info(void);
extern void kshell_cmd_memtest(void);
extern void kshell_cmd_vmmtest(void);
extern void kshell_cmd_bench(void);
extern void kshell_cmd_pitbench(void);
extern void kshell_cmd_banner(int, char**);
extern void kshell_cmd_beep(int, char**);
extern void kshell_cmd_hb(void);
extern void kshell_cmd_r3test(void);
extern void poweroff(void);
extern volatile uint8_t ctrl_pressed;

static void kshell_cmd_nano(int argc, char **argv) {
    if (argc < 1) { printf("{FG(255,0,0)}Usage: nano <file>\n"); return; }
    editor_open(argv[0]);
}

static void free_args(char *args[], int argc) {
    for (int i = 0; i < argc; i++)
        if (args[i]) pmm_free(args[i]);
}

static void parse_command(char *input, char *cmd, char *args[], int *argc) {
    int i = 0, j = 0;
    *argc = 0;

    while (input[i] && input[i] == ' ') i++;
    while (input[i] && input[i] != ' ' && j < 63) cmd[j++] = input[i++];
    cmd[j] = '\0';

    while (input[i] && *argc < 16) {
        while (input[i] == ' ') i++;
        if (!input[i]) break;

        args[*argc] = (char*)pmm_malloc(128);
        if (!args[*argc]) { free_args(args, *argc); *argc = 0; break; }

        j = 0;
        while (input[i] && input[i] != ' ' && j < 127)
            args[*argc][j++] = input[i++];
        args[*argc][j] = '\0';
        (*argc)++;
    }
}

void kshell_launch(void) {
    while (1) {
        printf("{FG(194,122,255)}root@xcos{FG(255,255,255)}:{FG(100,200,255)}%s{FG(255,255,255)}$ ",
               xcfs_getcwd());

        char input[256];
        keyboard_input(input, sizeof(input));
        printf("\n");

        if (strlen(input) == 0) continue;

        char  cmd[64]  = {0};
        char *args[16] = {0};
        int   argc     = 0;
        parse_command(input, cmd, args, &argc);

        if      (strcmp(cmd, "help")      == 0) kshell_cmd_help();
        else if (strcmp(cmd, "clear")     == 0) clear();
        else if (strcmp(cmd, "ls")        == 0) kshell_cmd_ls(argc, args);
        else if (strcmp(cmd, "cd")        == 0) kshell_cmd_cd(argc, args);
        else if (strcmp(cmd, "pwd")       == 0) kshell_cmd_pwd();
        else if (strcmp(cmd, "mkdir")     == 0) kshell_cmd_mkdir(argc, args);
        else if (strcmp(cmd, "rsof")      == 0) kshell_cmd_rsof();
        else if (strcmp(cmd, "cat")       == 0) kshell_cmd_cat(argc, args);
        else if (strcmp(cmd, "echo")      == 0) kshell_cmd_echo(argc, args);
        else if (strcmp(cmd, "touch")     == 0) kshell_cmd_touch(argc, args);
        else if (strcmp(cmd, "rm")        == 0) kshell_cmd_rm(argc, args);
        else if (strcmp(cmd, "cp")        == 0) kshell_cmd_cp(argc, args);
        else if (strcmp(cmd, "mv")        == 0) kshell_cmd_mv(argc, args);
        else if (strcmp(cmd, "stat")      == 0) kshell_cmd_stat(argc, args);
        else if (strcmp(cmd, "tree")      == 0) kshell_cmd_tree(argc, args);
        else if (strcmp(cmd, "uname")     == 0) kshell_cmd_uname();
        else if (strcmp(cmd, "free")      == 0) kshell_cmd_free();
        else if (strcmp(cmd, "uptime")    == 0) kshell_cmd_uptime();
        else if (strcmp(cmd, "clock")     == 0) kshell_cmd_clock();
        else if (strcmp(cmd, "timer")     == 0) kshell_cmd_timer();
        else if (strcmp(cmd, "sleep")     == 0) kshell_cmd_sleep(argc, args);
        else if (strcmp(cmd, "countdown") == 0) kshell_cmd_countdown(argc, args);
        else if (strcmp(cmd, "meminfo")   == 0) kshell_cmd_meminfo();
        else if (strcmp(cmd, "sysinfo")   == 0) kshell_cmd_sysinfo();
        else if (strcmp(cmd, "vmmstat")   == 0) vmm_print_stats();
        else if (strcmp(cmd, "cpu")       == 0) cpu_print_info();
        else if (strcmp(cmd, "memtest")   == 0) kshell_cmd_memtest();
        else if (strcmp(cmd, "vmmtest")   == 0) kshell_cmd_vmmtest();
        else if (strcmp(cmd, "bench")     == 0) kshell_cmd_bench();
        else if (strcmp(cmd, "pitbench")  == 0) kshell_cmd_pitbench();
        else if (strcmp(cmd, "nano")      == 0) kshell_cmd_nano(argc, args);
        else if (strcmp(cmd, "banner")    == 0) kshell_cmd_banner(argc, args);
        else if (strcmp(cmd, "beep")      == 0) kshell_cmd_beep(argc, args);
        else if (strcmp(cmd, "hb")        == 0) kshell_cmd_hb();
        else if (strcmp(cmd, "r3test")    == 0) kshell_cmd_r3test();
        else if (strcmp(cmd, "poweroff")  == 0) poweroff();
        else if (strcmp(cmd, "reboot")    == 0) {
            printf("{FG(255,255,0)}Rebooting...\n");
            outb(0x64, 0xFE);
        }
        else if (strlen(cmd) > 0) {
            printf("{FG(255,0,0)}%s: command not found\n", cmd);
        }

        free_args(args, argc);
    }
}
