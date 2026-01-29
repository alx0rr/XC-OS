#include "../include/text.h"
#include "../include/fs/xcfs.h"
#include "../lib/string.h"

void print_tree(const char* path, int level) {
    xcfs_dirent_t entries[128];
    int count = xcfs_readdir(path, entries, 128);
    
    if (count < 0) return;
    
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < level; j++) {
            printf("  ");
        }
        
        if (entries[i].type == XCFS_TYPE_DIR) {
            printf("{FG(100,200,255)}├─ [%s]/\n", entries[i].name);
            
            char subpath[256];
            strcpy(subpath, path);
            if (subpath[strlen(subpath) - 1] != '/') {
                strcat(subpath, "/");
            }
            strcat(subpath, entries[i].name);
            
            print_tree(subpath, level + 1);
        } else {
            printf("{FG(255,255,255)}├─ %s\n", entries[i].name);
        }
    }
}

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";
    
    printf("{FG(0,255,255)}%s\n", path);
    print_tree(path, 0);
    
    return 0;
}
