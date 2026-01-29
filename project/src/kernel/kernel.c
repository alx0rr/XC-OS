#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../include/fs/xcfs.h"
#include "../include/memory/vmm.h"
#include "../lib/io.h"
#include "../lib/string.h"

#include "demo.c"
#include "startup.c"
#include "cmd.c"

void parse_command(char* input, char* cmd, char* args[], int* argc) {
    int i = 0, j = 0;
    *argc = 0;
    
    while (input[i] && input[i] == ' ') i++;
    
    while (input[i] && input[i] != ' ' && j < 63) {
        cmd[j++] = input[i++];
    }
    cmd[j] = '\0';
    
    while (input[i] && *argc < 16) {
        while (input[i] == ' ') i++;
        if (!input[i]) break;
        
        args[*argc] = (char*)pmm_malloc(128);
        if (!args[*argc]) break;
        
        j = 0;
        while (input[i] && input[i] != ' ' && j < 127) {
            args[*argc][j++] = input[i++];
        }
        args[*argc][j] = '\0';
        (*argc)++;
    }
}

void free_args(char* args[], int argc) {
    for (int i = 0; i < argc; i++) {
        if (args[i]) pmm_free(args[i]);
    }
}

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

void cmd_pwd() {
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
    
    int write_mode = 0;
    const char* filename = NULL;
    
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            write_mode = 1;
            filename = argv[i + 1];
            argc = i;
            break;
        }
    }
    
    if (write_mode && filename) {
        char text[1024] = {0};
        for (int i = 0; i < argc; i++) {
            strcat(text, argv[i]);
            if (i < argc - 1) strcat(text, " ");
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
    if (xcfs_read(argv[0], buffer, 8192) == 0) {
        xcfs_delete(argv[1]);
        
        uint32_t size = 0;
        for (int i = 0; i < 8192 && buffer[i]; i++) size++;
        
        if (xcfs_create(argv[1], size) == 0) {
            xcfs_write(argv[1], buffer, size);
            printf("{FG(0,255,0)}Copied: %s -> %s\n", argv[0], argv[1]);
        } else {
            printf("{FG(255,0,0)}cp: cannot create '%s'\n", argv[1]);
        }
    } else {
        printf("{FG(255,0,0)}cp: cannot read '%s'\n", argv[0]);
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

void cmd_uname() {
    printf("XC-OS v0.3 i386 (XCFS v2)\n");
}

void cmd_free() {
    pmm_print_stats();
}

void help() {
    printf("{FG(0,255,255)}=== System Commands ===\n");
    printf("  {FG(255,255,0)}help{FG(255,255,255)}      - Show this help\n");
    printf("  {FG(255,255,0)}clear{FG(255,255,255)}     - Clear screen\n");
    printf("  {FG(255,255,0)}reboot{FG(255,255,255)}    - Reboot system\n");
    printf("  {FG(255,255,0)}uname{FG(255,255,255)}     - System information\n");
    printf("  {FG(255,255,0)}sysinfo{FG(255,255,255)}   - Detailed system info\n");
    printf("  {FG(255,255,0)}free{FG(255,255,255)}      - Memory usage\n");
    printf("  {FG(255,255,0)}meminfo{FG(255,255,255)}   - Detailed memory info\n");
    printf("  {FG(255,255,0)}vmmstat{FG(255,255,255)}   - Virtual memory stats\n");
    printf("  {FG(255,255,0)}enablepaging{FG(255,255,255)} - Enable paging\n");
    printf("  {FG(255,255,0)}cpu{FG(255,255,255)}       - CPU information\n");
    printf("  {FG(255,255,0)}uptime{FG(255,255,255)}    - System uptime\n");
    printf("  {FG(255,255,0)}clock{FG(255,255,255)}     - Full-screen clock\n");
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
    printf("\n");
    printf("{FG(0,255,255)}=== Other Commands ===\n");
    printf("  {FG(255,255,0)}banner{FG(255,255,255)} <t> - Display banner\n");
    printf("\n");
}

void kernel_main() {
    startup();
    printf("{FG(0,255,0)}Welcome to {FG(194,122,255)}XC-OS v0.3{FG(0,255,0)}\n");
    printf("\nType '{FG(255,255,0)}help{FG(255,255,255)}' for available commands\n\n");
    
    while (1) {
        const char* cwd = xcfs_getcwd();
        printf("{FG(194,122,255)}root@xcos{FG(255,255,255)}:{FG(100,200,255)}%s{FG(255,255,255)}$ ", cwd);
        
        char* input = keyboard_input();
        printf("\n");
        
        if (strlen(input) == 0) continue;
        
        char cmd[64] = {0};
        char* args[16] = {0};
        int argc = 0;
        
        parse_command(input, cmd, args, &argc);
        
        if (strcmp(cmd, "clear") == 0) {
            clear();
        }
        else if (strcmp(cmd, "help") == 0) {
            help();
        }
        else if (strcmp(cmd, "ls") == 0) {
            cmd_ls(argc, args);
        }
        else if (strcmp(cmd, "cd") == 0) {
            cmd_cd(argc, args);
        }
        else if (strcmp(cmd, "pwd") == 0) {
            cmd_pwd();
        }
        else if (strcmp(cmd, "mkdir") == 0) {
            cmd_mkdir(argc, args);
        }
        else if (strcmp(cmd, "cat") == 0) {
            cmd_cat(argc, args);
        }
        else if (strcmp(cmd, "echo") == 0) {
            cmd_echo(argc, args);
        }
        else if (strcmp(cmd, "touch") == 0) {
            cmd_touch(argc, args);
        }
        else if (strcmp(cmd, "rm") == 0) {
            cmd_rm(argc, args);
        }
        else if (strcmp(cmd, "cp") == 0) {
            cmd_cp(argc, args);
        }
        else if (strcmp(cmd, "mv") == 0) {
            cmd_mv(argc, args);
        }
        else if (strcmp(cmd, "uname") == 0) {
            cmd_uname();
        }
        else if (strcmp(cmd, "free") == 0) {
            cmd_free();
        }
        else if (strcmp(cmd, "vmmstat") == 0) {
            vmm_print_stats();
        }
        else if (strcmp(cmd, "enablepaging") == 0) {
            printf("{FG(255,255,0)}Enabling paging...\n");
            vmm_enable_paging();
            printf("{FG(0,255,0)}Paging enabled successfully\n");
        }
        else if (strcmp(cmd, "cpu") == 0) {
            cpu_print_info();
        }
        else if (strcmp(cmd, "memtest") == 0) {
            memory_stress_test();
        }
        else if (strcmp(cmd, "vmmtest") == 0) {
            cmd_vmm_test();
        }
        else if (strcmp(cmd, "meminfo") == 0) {
            cmd_meminfo();
        }
        else if (strcmp(cmd, "sysinfo") == 0) {
            cmd_sysinfo();
        }
        else if (strcmp(cmd, "bench") == 0) {
            cmd_bench();
        }
        else if (strcmp(cmd, "clock") == 0) {
            cmd_clock();
        }
        else if (strcmp(cmd, "tree") == 0) {
            cmd_tree(argc, args);
        }
        else if (strcmp(cmd, "banner") == 0) {
            cmd_banner(argc, args);
        }
        else if (strcmp(cmd, "stat") == 0) {
            cmd_stat(argc, args);
        }
        else if (strcmp(cmd, "uptime") == 0) {
            cmd_uptime();
        }
        else if (strcmp(cmd, "reboot") == 0) {
            printf("{FG(255,255,0)}Rebooting...\n");
            outb(0x64, 0xFE);
        }
        else if (strlen(cmd) > 0) {
            printf("{FG(255,0,0)}%s: command not found\n", cmd);
        }
        
        free_args(args, argc);
    }
}
