#include "../include/text.h"
#include "../include/fs/xcfs.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: stat <file>\n");
        return 1;
    }
    
    xcfs_dirent_t info;
    if (xcfs_stat(argv[1], &info) < 0) {
        printf("stat: cannot stat '%s'\n", argv[1]);
        return 1;
    }
    
    printf("File: %s\n", info.name);
    printf("Type: %s\n", (info.type == XCFS_TYPE_DIR) ? "Directory" : "Regular File");
    printf("Size: %u bytes\n", info.size);
    printf("Flags: ");
    
    if (info.flags & XCFS_FLAG_PROTECTED) printf("PROTECTED ");
    if (info.flags & XCFS_FLAG_EXECUTABLE) printf("EXECUTABLE ");
    if (info.flags & XCFS_FLAG_READONLY) printf("READONLY ");
    
    printf("\n");
    
    return 0;
}
