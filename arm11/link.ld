OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(__boot)

MEMORY
{
    AXIWRAM (RWX) : ORIGIN = 0x1FF80000, LENGTH = 96K
    HIGHRAM (RWX) : ORIGIN = 0xFFFF0000, LENGTH = 4K
}

SECTIONS
{
    .text : ALIGN(4K)
    {
        __text_pa = LOADADDR(.text);
        __text_va = ABSOLUTE(.);
        *(.text*)
        . = ALIGN(4K);
        __text_va_end = .;
    } >AXIWRAM

    .data : ALIGN(4K)
    {
        __data_pa = LOADADDR(.data);
        __data_va = ABSOLUTE(.);
        *(.data*)
        . = ALIGN(4K);
        __data_va_end = .;
    } >AXIWRAM

    .rodata : ALIGN(4K)
    {
        __rodata_pa = LOADADDR(.rodata);
        __rodata_va = ABSOLUTE(.);
        *(.rodata*)
        . = ALIGN(4K);
        __rodata_va_end = .;
    } >AXIWRAM

    .shared (NOLOAD) : ALIGN(4K)
    {
        __shared_pa = LOADADDR(.shared);
        __shared_va = ABSOLUTE(.);
        *(.shared*)
        . = ALIGN(4K);
        __shared_va_end = .;
    } >AXIWRAM

    .bss (NOLOAD) : ALIGN(4K)
    {
        __bss_pa = LOADADDR(.bss);
        __bss_va = ABSOLUTE(.);
        *(.bss*)
        . = ALIGN(4K);
        __bss_va_end = .;
    } >AXIWRAM
}
