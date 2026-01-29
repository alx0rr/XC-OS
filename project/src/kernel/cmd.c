#include "../include/text.h"
#include "../include/fs/xcfs.h"
#include "../lib/string.h"
#include "../lib/time.h"

void cmd_banner(int argc, char** argv) {
    if (argc < 1) {
        printf("Usage: banner <text>\n");
        return;
    }
    
    printf("\n");
    printf("  +======================================+\n");
    printf("  |                                      |\n");
    printf("  |  %s", argv[0]);
    
    int len = 0;
    for (int i = 0; argv[0][i]; i++) len++;
    
    for (int i = len; i < 34; i++) {
        printf(" ");
    }
    
    printf("|\n");
    printf("  |                                      |\n");
    printf("  +--------------------------------------+\n");
    printf("\n");
}

void cmd_stat(int argc, char** argv) {
    if (argc < 1) {
        printf("Usage: stat <file>\n");
        return;
    }
    
    xcfs_dirent_t info;
    if (xcfs_stat(argv[0], &info) < 0) {
        printf("stat: cannot stat '%s'\n", argv[0]);
        return;
    }
    
    printf("File: %s\n", info.name);
    printf("Type: %s\n", (info.type == XCFS_TYPE_DIR) ? "Directory" : "Regular File");
    printf("Size: %u bytes\n", info.size);
    printf("Flags: ");
    
    if (info.flags & XCFS_FLAG_PROTECTED) printf("PROTECTED ");
    if (info.flags & XCFS_FLAG_EXECUTABLE) printf("EXECUTABLE ");
    if (info.flags & XCFS_FLAG_READONLY) printf("READONLY ");
    
    printf("\n");
}

void print_tree_recursive(const char* path, int level) {
    xcfs_dirent_t entries[128];
    int count = xcfs_readdir(path, entries, 128);
    
    if (count < 0) return;
    
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < level; j++) {
            printf("  ");
        }
        
        if (entries[i].type == XCFS_TYPE_DIR) {
            printf("{FG(100,200,255)} +- [%s]/\n", entries[i].name);
            
            char subpath[256];
            strcpy(subpath, path);
            if (subpath[strlen(subpath) - 1] != '/') {
                strcat(subpath, "/");
            }
            strcat(subpath, entries[i].name);
            
            print_tree_recursive(subpath, level + 1);
        } else {
            printf("{FG(255,255,255)}+- %s\n", entries[i].name);
        }
    }
}

void cmd_tree(int argc, char** argv) {
    const char* path = (argc > 0) ? argv[0] : "/";
    
    printf("{FG(0,255,255)}%s\n", path);
    print_tree_recursive(path, 0);
}

void cmd_uptime() {
    uint32_t uptime_ms = get_uptime();
    uint32_t seconds = uptime_ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    printf("Uptime: %u:%02u:%02u\n", hours, minutes, seconds);
}
