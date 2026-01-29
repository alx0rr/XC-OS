#include "../../include/exec.h"
#include "../../include/fs/xcfs.h"
#include "../../include/memory/pmm.h"
#include "../../include/text.h"
#include "../../lib/string.h"

#define EXEC_BASE_ADDR 0x400000

int exec_find_in_path(const char* name, char* fullpath) {
    if (!name || !fullpath) return -1;
    
    if (name[0] == '/' || name[0] == '.') {
        strcpy(fullpath, name);
        return 0;
    }
    
    strcpy(fullpath, "/bin/");
    strcat(fullpath, name);
    
    xcfs_dirent_t info;
    if (xcfs_stat(fullpath, &info) == 0) {
        return 0;
    }
    
    return -1;
}

int exec_load(const char* path, int argc, char** argv) {
    if (!path) return -1;
    
    char fullpath[256];
    if (exec_find_in_path(path, fullpath) < 0) {
        printf("{FG(255,0,0)}exec: %s: not found\n", path);
        return -1;
    }
    
    xcfs_dirent_t info;
    if (xcfs_stat(fullpath, &info) < 0) {
        printf("{FG(255,0,0)}exec: cannot stat %s\n", fullpath);
        return -1;
    }
    
    if (!(info.flags & XCFS_FLAG_EXECUTABLE)) {
        printf("{FG(255,0,0)}exec: %s: permission denied\n", fullpath);
        return -1;
    }
    
    uint8_t* binary = (uint8_t*)pmm_malloc(info.size);
    if (!binary) {
        printf("{FG(255,0,0)}exec: out of memory\n");
        return -1;
    }
    
    if (xcfs_read(fullpath, binary, info.size) < 0) {
        pmm_free(binary);
        printf("{FG(255,0,0)}exec: cannot read %s\n", fullpath);
        return -1;
    }
    
    exec_header_t* header = (exec_header_t*)binary;
    if (header->magic != EXEC_MAGIC) {
        pmm_free(binary);
        printf("{FG(255,0,0)}exec: %s: bad format\n", fullpath);
        return -1;
    }
    
    uint32_t total_size = header->code_size + header->data_size + header->bss_size;
    uint8_t* prog_mem = (uint8_t*)pmm_malloc(total_size);
    if (!prog_mem) {
        pmm_free(binary);
        printf("{FG(255,0,0)}exec: out of memory\n");
        return -1;
    }
    
    uint8_t* code_start = binary + sizeof(exec_header_t);
    memcpy(prog_mem, code_start, header->code_size + header->data_size);
    memset(prog_mem + header->code_size + header->data_size, 0, header->bss_size);
    
    exec_func_t entry = (exec_func_t)(prog_mem + header->entry_point);
    
    int ret = entry(argc, argv);
    
    pmm_free(prog_mem);
    pmm_free(binary);
    
    return ret;
}
