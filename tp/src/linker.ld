OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0x100000;
    
    .multiboot : {
        *(.multiboot)
    }
    
    .text : {
        *(.text)
        *(.text.*)
    }
    
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data : {
        *(.data)
        *(.data.*)
    }
    
    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
}
