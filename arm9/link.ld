OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    AHBWRAM (RWX) : ORIGIN = 0x08006000, LENGTH = 512K
    VECTORS (RX) : ORIGIN = 0x08000000, LENGTH = 64
}

SECTIONS
{
    .vectors : ALIGN(4) {
        __vectors_lma = LOADADDR(.vectors);
        __vectors_vma = ABSOLUTE(.);
        KEEP(*(.vectors));
        . = ALIGN(4);
        __vectors_len = ABSOLUTE(.) - __vectors_vma;
    } >VECTORS AT>AHBWRAM

    .text : ALIGN(4) {
        __text_s = ABSOLUTE(.);
        *(.text.start);
        *(.text*);
        . = ALIGN(4);
        __text_e = ABSOLUTE(.);
    } >AHBWRAM

    .rodata : ALIGN(4) {
        *(.rodata*);
        . = ALIGN(4);
    } >AHBWRAM

    .data : ALIGN(4) {
        *(.data*);
        . = ALIGN(4);
    } >AHBWRAM

    .bss : ALIGN(4) {
        __bss_start = .;
        *(.bss*);
        . = ALIGN(4);
        __bss_end = .;
    } >AHBWRAM

    __end__ = ABSOLUTE(.);
}
