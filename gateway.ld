ENTRY(_start)
SECTIONS
{
    . = 0x08000000;
    .text.start : { *(.text.start) }
    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { __bss_start = .; *(.bss COMMON) }
    __bss_end = .;
    .rodata : { *(.rodata) }
    . = ALIGN(4);
    __end__ = ABSOLUTE(.);
}
