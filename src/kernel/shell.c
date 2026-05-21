#include "../include/shell.h"
#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../include/fs/xcfs.h"
#include "../include/memory/pmm.h"
#include "../lib/string.h"
#include "../lib/io.h"
#include "../include/net/ne2000.h"
#include "../include/net/arp.h"
#include "../include/net/icmp.h"
#include "../include/net/dns.h"
#include "../include/net/http.h"
#include "../include/editor.h"
#include "../include/cpu/cpu.h"
#include "../include/memory/vmm.h"
#include "../include/graphics/vbe.h"
#include "../include/graphics/framebuffer.h"
#include "../include/timer/pit.h"
#include "../include/sound/pcspk.h"
#include "../lib/time.h"
#include "../lib/io.h"

extern void cmd_help(void);
extern void cmd_ls(int, char**);
extern void cmd_cd(int, char**);
extern void cmd_pwd(void);
extern void cmd_mkdir(int, char**);
extern void cmd_rsof(void);
extern void cmd_cat(int, char**);
extern void cmd_echo(int, char**);
extern void cmd_touch(int, char**);
extern void cmd_rm(int, char**);
extern void cmd_cp(int, char**);
extern void cmd_mv(int, char**);
extern void cmd_stat(int, char**);
extern void cmd_tree(int, char**);
extern void cmd_uname(void);
extern void cmd_free(void);
extern void cmd_uptime(void);
extern void cmd_clock(void);
extern void cmd_timer(void);
extern void cmd_sleep(int, char**);
extern void cmd_countdown(int, char**);
extern void cmd_meminfo(void);
extern void cmd_sysinfo(void);
extern void vmm_print_stats(void);
extern void cpu_print_info(void);
extern void cmd_memtest(void);
extern void cmd_vmmtest(void);
extern void cmd_bench(void);
extern void cmd_pitbench(void);
extern void cmd_ifconfig(void);
extern void cmd_ping(int, char**);
extern void cmd_setip(int, char**);
extern void cmd_banner(int, char**);
extern void cmd_beep(int, char**);
extern void cmd_hb(void);
extern void poweroff();
extern volatile uint8_t ctrl_pressed;

static void fmt_ip(u32 ip, char *buf) {
    snprintf(buf, 16, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
             (ip >>  8) & 0xFF,  ip         & 0xFF);
}

static int parse_ip(const char *s, u32 *ip) {
    u32 r = 0;
    u8  i;
    for (i = 0; i < 4; i++) {
        u8 b = 0;
        while (*s >= '0' && *s <= '9') b = b * 10 + (*s++ - '0');
        r = (r << 8) | b;
        if (i < 3) { if (*s != '.') return -1; s++; }
    }
    *ip = r;
    return 0;
}


static void cmd_setdns(int argc, char **argv) {
    u32 ip;
    char buf[16];
    if (argc < 1) { printf("{FG(255,0,0)}Usage: setdns <ip>\n"); return; }
    if (parse_ip(argv[0], &ip) < 0) { printf("{FG(255,0,0)}Bad IP\n"); return; }
    dns_set_server(ip);
    fmt_ip(ip, buf);
    printf("{FG(0,255,0)}DNS set to %s\n", buf);
}

static void cmd_nslookup(int argc, char **argv) {
    u32 ip;
    char buf[16];
    if (argc < 1) { printf("{FG(255,0,0)}Usage: nslookup <host>\n"); return; }
    printf("Resolving %s...\n", argv[0]);
    if (dns_resolve(argv[0], &ip) < 0) {
        printf("{FG(255,0,0)}Failed to resolve\n");
        return;
    }
    fmt_ip(ip, buf);
    printf("{FG(0,255,0)}%s -> %s\n", argv[0], buf);
}

static void cmd_wget(int argc, char **argv) {
    if (argc < 2) {
        printf("{FG(255,0,0)}Usage: wget <url> <filename>\n");
        return;
    }

    const char *url      = argv[0];
    const char *filename = argv[1];

    char host[128] = {0};
    char path[256] = "/";
    u16  port      = 80;
    const char *p  = url;

    if (strncmp(p, "http://", 7) == 0) p += 7;

    u32 i = 0;
    while (*p && *p != '/' && *p != ':' && i < 127) host[i++] = *p++;
    host[i] = '\0';
    if (*p == ':') {
        p++;
        port = 0;
        while (*p >= '0' && *p <= '9') port = (u16)(port * 10 + (*p++ - '0'));
    }
    if (*p == '/') strncpy(path, p, 255);
    else { path[0] = '/'; path[1] = '\0'; }

    printf("Resolving %s...\n", host);

    static u8 resp[65536];
    u32 out_len = 0;
    int status  = 0;

    if (http_get(host, (u16)port, path, resp, sizeof(resp), &out_len, &status) < 0) {
        printf("{FG(255,0,0)}Failed: could not connect to %s:%d\n", host, (int)port);
        return;
    }

    if (out_len == 0) {
        printf("{FG(255,0,0)}Failed: empty response\n");
        return;
    }

    printf("HTTP %d, %u bytes total\n", status, out_len);

    u8  *body    = resp;
    u32  body_sz = out_len;

    for (u32 j = 0; j + 3 < out_len; j++) {
        if (resp[j]=='\r' && resp[j+1]=='\n' && resp[j+2]=='\r' && resp[j+3]=='\n') {
            body    = resp + j + 4;
            body_sz = out_len - (j + 4);
            break;
        }
    }

    if (body_sz == 0) {
        printf("{FG(255,255,0)}Warning: empty body\n");
    }

    xcfs_dirent_t info;
    if (xcfs_stat(filename, &info) == 0) xcfs_delete(filename);
    if (xcfs_create(filename, body_sz) < 0) {
        printf("{FG(255,0,0)}Failed to create file %s\n", filename);
        return;
    }
    xcfs_write(filename, body, body_sz);
    printf("{FG(0,255,0)}Saved %u bytes -> %s\n", body_sz, filename);
}

static void cmd_post(int argc, char **argv) {
    if (argc < 2) {
        printf("{FG(255,0,0)}Usage: post <url> <file>\n");
        return;
    }

    const char *url      = argv[0];
    const char *src_file = argv[1];

    xcfs_dirent_t info;
    if (xcfs_stat(src_file, &info) < 0) {
        printf("{FG(255,0,0)}File not found: %s\n", src_file);
        return;
    }

    u8 *fbuf = (u8*)pmm_malloc(info.size + 1);
    if (!fbuf) { printf("{FG(255,0,0)}Out of memory\n"); return; }
    xcfs_read(src_file, fbuf, info.size);

    char host[128] = {0};
    char path[256] = "/";
    u16  port = 80;
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    u8 i = 0;
    while (*p && *p != '/' && *p != ':' && i < 127) host[i++] = *p++;
    if (*p == ':') {
        p++;
        port = 0;
        while (*p >= '0' && *p <= '9') port = port * 10 + (*p++ - '0');
    }
    if (*p == '/') strncpy(path, p, 255);

    printf("POST %s:%u%s (%u bytes)\n", host, port, path, info.size);

    static u8 resp[4096];
    u32 out_len = 0;
    int status  = 0;

    http_post(host, port, path, "application/octet-stream",
              fbuf, info.size, resp, sizeof(resp), &out_len, &status);

    pmm_free(fbuf);
    printf("HTTP %d\n", status);
}

static void cmd_nano(int argc, char **argv) {
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

void shell_run(void) {
    while (1) {
        printf("{FG(194,122,255)}root@xcos{FG(255,255,255)}:{FG(100,200,255)}%s{FG(255,255,255)}$ ",
               xcfs_getcwd());

        char *input = keyboard_input();
        printf("\n");

        if (strlen(input) == 0) continue;

        char  cmd[64]  = {0};
        char *args[16] = {0};
        int   argc     = 0;
        parse_command(input, cmd, args, &argc);

        if      (strcmp(cmd, "help")      == 0) cmd_help();
        else if (strcmp(cmd, "clear")     == 0) clear();
        else if (strcmp(cmd, "ls")        == 0) cmd_ls(argc, args);
        else if (strcmp(cmd, "cd")        == 0) cmd_cd(argc, args);
        else if (strcmp(cmd, "pwd")       == 0) cmd_pwd();
        else if (strcmp(cmd, "mkdir")     == 0) cmd_mkdir(argc, args);
        else if (strcmp(cmd, "rsof")      == 0) cmd_rsof();
        else if (strcmp(cmd, "cat")       == 0) cmd_cat(argc, args);
        else if (strcmp(cmd, "echo")      == 0) cmd_echo(argc, args);
        else if (strcmp(cmd, "touch")     == 0) cmd_touch(argc, args);
        else if (strcmp(cmd, "rm")        == 0) cmd_rm(argc, args);
        else if (strcmp(cmd, "cp")        == 0) cmd_cp(argc, args);
        else if (strcmp(cmd, "mv")        == 0) cmd_mv(argc, args);
        else if (strcmp(cmd, "stat")      == 0) cmd_stat(argc, args);
        else if (strcmp(cmd, "tree")      == 0) cmd_tree(argc, args);
        else if (strcmp(cmd, "uname")     == 0) cmd_uname();
        else if (strcmp(cmd, "free")      == 0) cmd_free();
        else if (strcmp(cmd, "uptime")    == 0) cmd_uptime();
        else if (strcmp(cmd, "clock")     == 0) cmd_clock();
        else if (strcmp(cmd, "timer")     == 0) cmd_timer();
        else if (strcmp(cmd, "sleep")     == 0) cmd_sleep(argc, args);
        else if (strcmp(cmd, "countdown") == 0) cmd_countdown(argc, args);
        else if (strcmp(cmd, "meminfo")   == 0) cmd_meminfo();
        else if (strcmp(cmd, "sysinfo")   == 0) cmd_sysinfo();
        else if (strcmp(cmd, "vmmstat")   == 0) vmm_print_stats();
        else if (strcmp(cmd, "cpu")       == 0) cpu_print_info();
        else if (strcmp(cmd, "memtest")   == 0) cmd_memtest();
        else if (strcmp(cmd, "vmmtest")   == 0) cmd_vmmtest();
        else if (strcmp(cmd, "bench")     == 0) cmd_bench();
        else if (strcmp(cmd, "pitbench")  == 0) cmd_pitbench();
        else if (strcmp(cmd, "ifconfig")  == 0) cmd_ifconfig();
        else if (strcmp(cmd, "ping")      == 0) cmd_ping(argc, args);
        else if (strcmp(cmd, "setip")     == 0) cmd_setip(argc, args);
        else if (strcmp(cmd, "setdns")    == 0) cmd_setdns(argc, args);
        else if (strcmp(cmd, "nslookup")  == 0) cmd_nslookup(argc, args);
        else if (strcmp(cmd, "wget")      == 0) cmd_wget(argc, args);
        else if (strcmp(cmd, "post")      == 0) cmd_post(argc, args);
        else if (strcmp(cmd, "nano")      == 0) cmd_nano(argc, args);
        else if (strcmp(cmd, "banner")    == 0) cmd_banner(argc, args);
        else if (strcmp(cmd, "beep")      == 0) cmd_beep(argc, args);
        else if (strcmp(cmd, "hb")        == 0) cmd_hb();
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
