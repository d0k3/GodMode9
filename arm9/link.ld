OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    AHBWRAM (RWX) : ORIGIN = 0x08000040, LENGTH = 512K - 64
    AHBWRAM2 (RWX) : ORIGIN = 0x080a0000, LENGTH = 384K
}

SECTIONS
{
    /* this must come *first* so it picks up lua */
    AHBWRAM2 : ALIGN(4) {
        build/lua*(.text*);

        . = ALIGN(4);
        *(.rodata*);

        /* .data */
        . = ALIGN(4);
        *(.data*);

        . = ALIGN(4);
        __bss_start = .;
        *(.bss*);
        __bss_end = .;

        __end__ = .;
    } >AHBWRAM2



    __vectors_lma = 0x08000000;
    AHBWRAM : ALIGN(4) {
        /* this needs to be absolute otherwise GM9 won't boot */
        __vectors_vma = ABSOLUTE(.);
        KEEP(*(.vectors));
        . = ALIGN(4);
        __vectors_len = . - __vectors_vma;
        . = . + 0x10;



        . = ALIGN(4);
        __text_s = .;
        *(.text.start);
        *(.text*);
        __text_e = .;

        /* .ARM.exidx (needed for lua) */
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } >AHBWRAM
}
