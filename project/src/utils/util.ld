OUTPUT_FORMAT(binary)
ENTRY(main)

SECTIONS
{
    . = 0x400000;
    
    .header : {
        LONG(0x584F5845);
        LONG(_start - 0x400000);
        LONG(_etext - _start);
        LONG(_edata - _etext);
        LONG(_end - _edata);
        LONG(0); LONG(0); LONG(0); LONG(0); LONG(0);
        LONG(0); LONG(0); LONG(0); LONG(0); LONG(0); LONG(0);
    }
    
    .text : {
        _start = .;
        *(.text)
        *(.text.*)
        _etext = .;
    }
    
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data : {
        *(.data)
        *(.data.*)
        _edata = .;
    }
    
    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
        _end = .;
    }
}
