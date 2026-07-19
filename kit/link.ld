OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS {
    . = 0x08048000;

    .text : {
        *(.text*)
    }

    . = ALIGN(4096);
    .rodata : {
        *(.rodata*)
    }

    . = ALIGN(4096);
    .data : {
        *(.data*)
    }

    . = ALIGN(4096);
    .bss : {
        *(.bss*)
        *(COMMON)
    }
}
