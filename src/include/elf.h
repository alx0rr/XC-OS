#ifndef ELF_H
#define ELF_H
#include <stdint.h>
#include "proc/proc.h"

#define ELF_MAGIC    0x464C457F
#define ET_EXEC      2
#define EM_386       3
#define PT_LOAD      1
#define PF_X         0x1
#define PF_W         0x2
#define PF_R         0x4

typedef struct {
    uint32_t e_magic;
    uint8_t  e_class;
    uint8_t  e_data;
    uint8_t  e_version;
    uint8_t  e_osabi;
    uint8_t  e_pad[8];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_ver;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsz;
    uint16_t e_phentsz;
    uint16_t e_phnum;
    uint16_t e_shentsz;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

#define ELF_USER_MAX 0xC0000000U

int elf_validate(const uint8_t *img, uint32_t sz);
proc_t *elf_load(const char *nm, const uint8_t *img, uint32_t sz);

#endif
