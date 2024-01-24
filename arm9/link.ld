OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    AHBWRAM_LO (RWX) : ORIGIN = 0x08000040, LENGTH = 512K - 64
    AHBWRAM_HI (RWX) : ORIGIN = 0x080a0000, LENGTH = 386K
}

SECTIONS
{
    /* this must come *first* so it picks up lua */
    AHBWRAM_HI : ALIGN(4) {
        *build/lua*(.text*);
        libm*(.text*);
		*(.rodata.vram_data);

    } >AHBWRAM_HI


    __vectors_vma = 0x08000000;
    AHBWRAM_LO : ALIGN(4) {
        /* this needs to be absolute otherwise GM9 won't boot */
        __vectors_lma = ABSOLUTE(.);
        KEEP(*(.vectors));
        . = ALIGN(4);
        __vectors_len = . - __vectors_lma;

        . = ALIGN(4);
        *(.text.start);
        *(.text*);

        /* .ARM.exidx (needed for lua) */
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;

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
    } >AHBWRAM_LO

}
