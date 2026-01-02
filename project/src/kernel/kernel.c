#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../include/fs/xcfs.h"
#include "../lib/io.h"
#include "../lib/string.h"

#include "demo.c"
#include "startup.c"

void parse_command(char* input, char* cmd, char* arg1, char* arg2, char* arg3) {
    int i = 0, j = 0;
    
    while (input[i] && input[i] != ' ' && j < 63) {
        cmd[j++] = input[i++];
    }
    cmd[j] = '\0';
    
    while (input[i] == ' ') i++;
    
    j = 0;
    while (input[i] && input[i] != ' ' && j < 127) {
        arg1[j++] = input[i++];
    }
    arg1[j] = '\0';
    
    while (input[i] == ' ') i++;
    
    j = 0;
    while (input[i] && input[i] != ' ' && j < 127) {
        arg2[j++] = input[i++];
    }
    arg2[j] = '\0';

    while (input[i] == ' ') i++;

    j = 0;
    while (input[i] && input[i] != ' ' && j < 127) {
        arg3[j++] = input[i++];
    }
    arg3[j] = '\0';
}

void cmd_ls() {
    xcfs_list();
}

void cmd_cat(const char* filename) {
    if (!filename[0]) {
        printf("{FG(255,0,0)}Usage: cat <file>\n");
        return;
    }
    
    uint8_t buffer[4096];
    if (xcfs_read(filename, buffer, 4096) == 0) {
        printf("{FG(0,255,0)}");
        for (int i = 0; i < 4096 && buffer[i]; i++) {
            printf("%c", buffer[i]);
        }
        printf("{FG(255,255,255)}\n");
    } else {
        printf("{FG(255,0,0)}cat: %s: No such file\n", filename);
    }
}

void cmd_echo(const char* text, const char* redirect, const char* filename) {
    if (!text[0]) {
        printf("\n");
        return;
    }
    
    if (redirect[0] && strcmp(redirect, "=") == 0 && filename[0]) {
        xcfs_delete(filename);
        if (xcfs_create(filename, strlen(text)) == 0) {
            xcfs_write(filename, (uint8_t*)text, strlen(text));
            printf("{FG(0,255,0)}Written to %s\n", filename);
        } else {
            printf("{FG(255,0,0)}echo: cannot create %s\n", filename);
        }
    } else {
        printf("%s\n", text);
    }
}

void cmd_touch(const char* filename) {
    if (!filename[0]) {
        printf("{FG(255,0,0)}Usage: touch <file>\n");
        return;
    }
    
    if (xcfs_create(filename, 0) == 0) {
        printf("{FG(0,255,0)}Created: %s\n", filename);
    } else {
        printf("{FG(255,0,0)}touch: cannot create '%s'\n", filename);
    }
}

void cmd_rm(const char* filename) {
    if (!filename[0]) {
        printf("{FG(255,0,0)}Usage: rm <file>\n");
        return;
    }
    
    if (xcfs_delete(filename) == 0) {
        printf("{FG(0,255,0)}Removed: %s\n", filename);
    } else {
        printf("{FG(255,0,0)}rm: cannot remove '%s': No such file\n", filename);
    }
}

void cmd_cp(const char* src, const char* dst) {
    if (!src[0] || !dst[0]) {
        printf("{FG(255,0,0)}Usage: cp <source> <dest>\n");
        return;
    }
    
    uint8_t buffer[4096];
    if (xcfs_read(src, buffer, 4096) == 0) {
        xcfs_delete(dst);
        
        uint32_t size = 0;
        for (int i = 0; i < 4096 && buffer[i]; i++) size++;
        
        if (xcfs_create(dst, size) == 0) {
            xcfs_write(dst, buffer, size);
            printf("{FG(0,255,0)}Copied: %s -> %s\n", src, dst);
        } else {
            printf("{FG(255,0,0)}cp: cannot create '%s'\n", dst);
        }
    } else {
        printf("{FG(255,0,0)}cp: cannot read '%s': No such file\n", src);
    }
}

void cmd_mv(const char* src, const char* dst) {
    cmd_cp(src, dst);
    if (xcfs_delete(src) != 0) {
        printf("{FG(255,165,0)}Warning: could not remove source file\n");
    }
}

void cmd_uname() {
    printf("XC-OS v1.0 i386\n");
}

void cmd_free() {
    pmm_print_stats();
}

void cmd_uptime() {
    datetime_t now = time_get_datetime();
    printf("Current time: %d:%d:%d %d/%d/%d\n", 
           now.hour, now.minute, now.second,
           now.day, now.month, now.year);
}

void help() {
    printf("{FG(0,255,255)}=== System Commands ===\n");
    printf("  {FG(255,255,0)}help{FG(255,255,255)}      - Show this help\n");
    printf("  {FG(255,255,0)}clear{FG(255,255,255)}     - Clear screen\n");
    printf("  {FG(255,255,0)}reboot{FG(255,255,255)}    - Reboot system\n");
    printf("  {FG(255,255,0)}uname{FG(255,255,255)}     - System information\n");
    printf("  {FG(255,255,0)}uptime{FG(255,255,255)}    - Show current time\n");
    printf("  {FG(255,255,0)}free{FG(255,255,255)}      - Memory usage\n");
    printf("  {FG(255,255,0)}cpu{FG(255,255,255)}       - CPU information\n");
    printf("\n");
    printf("{FG(0,255,255)}=== File Commands ===\n");
    printf("  {FG(255,255,0)}ls{FG(255,255,255)}        - List files\n");
    printf("  {FG(255,255,0)}cat{FG(255,255,255)} <file> - Show file content\n");
    printf("  {FG(255,255,0)}touch{FG(255,255,255)} <f>  - Create empty file\n");
    printf("  {FG(255,255,0)}rm{FG(255,255,255)} <file>  - Remove file\n");
    printf("  {FG(255,255,0)}cp{FG(255,255,255)} <s> <d> - Copy file\n");
    printf("  {FG(255,255,0)}mv{FG(255,255,255)} <s> <d> - Move file\n");
    printf("  {FG(255,255,0)}echo{FG(255,255,255)} <txt> - Print text\n");
    printf("  {FG(255,255,0)}echo{FG(255,255,255)} <t> > <f> - Write to file\n");
    printf("\n");
    printf("{FG(0,255,255)}=== Debug Commands ===\n");
    printf("  {FG(255,255,0)}memtest{FG(255,255,255)}   - Memory stress test\n");
    printf("  {FG(255,255,0)}random{FG(255,255,255)}    - Random number demo\n");
}

void kernel_main() {
    startup();
    printf("{FG(0,255,0)}Welcome to {FG(194,122,255)}XC-OS.{FG(0,255,0)}\n");
    printf("\nType '{FG(255,255,0)}help{FG(255,255,255)}' for available commands\n\n");
    
    while (1) { 
        printf("{FG(194,122,255)}root@xcos{FG(255,255,255)}:{FG(100,200,255)}~{FG(255,255,255)}$ ");
        char* input = keyboard_input();
        printf("\n");
        
        if (strlen(input) == 0) continue;
        
        char cmd[64] = {0};
        char arg1[128] = {0};
        char arg2[128] = {0};
        char arg3[128] = {0};
        
        parse_command(input, cmd, arg1, arg2, arg3);
        
        if (strcmp(cmd, "clear") == 0) {
            clear();
        }
        else if (strcmp(cmd, "help") == 0) {
            help();
        }
        else if (strcmp(cmd, "ls") == 0) {
            cmd_ls();
        }
        else if (strcmp(cmd, "cat") == 0) {
            cmd_cat(arg1);
        }
        else if (strcmp(cmd, "echo") == 0) {
            cmd_echo(arg1, arg2, arg3);
        }
        else if (strcmp(cmd, "touch") == 0) {
            cmd_touch(arg1);
        }
        else if (strcmp(cmd, "rm") == 0) {
            cmd_rm(arg1);
        }
        else if (strcmp(cmd, "cp") == 0) {
            cmd_cp(arg1, arg2);
        }
        else if (strcmp(cmd, "mv") == 0) {
            cmd_mv(arg1, arg2);
        }
        else if (strcmp(cmd, "uname") == 0) {
            cmd_uname();
        }
        else if (strcmp(cmd, "free") == 0) {
            cmd_free();
        }
        else if (strcmp(cmd, "uptime") == 0) {
            cmd_uptime();
        }
        else if (strcmp(cmd, "cpu") == 0) {
            cpu_print_info();
        }
        else if (strcmp(cmd, "memtest") == 0) {
            memory_stress_test();
        }
        else if (strcmp(cmd, "random") == 0) {
            random_demo();
        }
        else if (strcmp(cmd, "reboot") == 0) {
            printf("{FG(255,255,0)}Rebooting...\n");
            outb(0x64, 0xFE);
        }
        else if (strlen(cmd) > 0) {
            printf("{FG(255,0,0)}%s: command not found\n", cmd);
        }
    }
}