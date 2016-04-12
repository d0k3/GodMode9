ENTRY(_start)
SECTIONS
{
    . = 0x08000000;
    .text.start : { *(.text.start) }
    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss COMMON) }
    .rodata : { *(.rodata) }
    . = ALIGN(4);
    __end__ = ABSOLUTE(.);
}
