OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(__boot)
SECTIONS
{
    . = 0x1FF80000;

    .text   : ALIGN(4) { *(.text.boot) *(.text*); . = ALIGN(4); }
    .rodata : ALIGN(4) { *(.rodata*); . = ALIGN(4); }
    .data   : ALIGN(4) { *(.data*); . = ALIGN(8); }
    .bss    : ALIGN(4) { __bss_start = .; *(.bss* COMMON); __bss_end = .; }

    . = ALIGN(4);
}
