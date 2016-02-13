OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
  {
  ram : ORIGIN = 0x23F00000, LENGTH = 128K
  }

SECTIONS
{
	.init	:
	{
		__text_start = . ;
		KEEP (*(.init))
		. = ALIGN(4);  /* REQUIRED. LD is flaky without it. */
		} >ram = 0xff
	.plt : { *(.plt) } >ram = 0xff

	.text :   /* ALIGN (4): */
	{
		*(.text .stub .text.* .gnu.linkonce.t.*)
		KEEP (*(.text.*personality*))
		/* .gnu.warning sections are handled specially by elf32.em.  */
		*(.gnu.warning)
		*(.glue_7t) *(.glue_7) *(.vfp11_veneer)
		. = ALIGN(4);  /* REQUIRED. LD is flaky without it. */
	} >ram = 0xff

	.fini           :
	{
		KEEP (*(.fini))
	} >ram =0xff

	__text_end = . ;

	.rodata :
	{
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
		SORT(CONSTRUCTORS)
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >ram = 0xff

	.ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >ram
	__exidx_start = .;
	.ARM.exidx   : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } >ram
	__exidx_end = .;

/* Ensure the __preinit_array_start label is properly aligned.  We
   could instead move the label definition inside the section, but
   the linker would then create the section even if it turns out to
   be empty, which isn't pretty.  */
	. = ALIGN(32 / 8);
	PROVIDE (__preinit_array_start = .);
	.preinit_array     : { KEEP (*(.preinit_array)) } >ram = 0xff
	PROVIDE (__preinit_array_end = .);
	PROVIDE (__init_array_start = .);
	.init_array     : { KEEP (*(.init_array)) } >ram = 0xff
	PROVIDE (__init_array_end = .);
	PROVIDE (__fini_array_start = .);
	.fini_array     : { KEEP (*(.fini_array)) } >ram = 0xff
	PROVIDE (__fini_array_end = .);

	.ctors :
	{
	/* gcc uses crtbegin.o to find the start of the constructors, so
		we make sure it is first.  Because this is a wildcard, it
		doesn't matter if the user does not actually link against
		crtbegin.o; the linker won't look for a file to match a
		wildcard.  The wildcard also means that it doesn't matter which
		directory crtbegin.o is in.  */
		KEEP (*crtbegin.o(.ctors))
		KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
		KEEP (*(SORT(.ctors.*)))
		KEEP (*(.ctors))
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >ram = 0xff

	.dtors :
	{
		KEEP (*crtbegin.o(.dtors))
		KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
		KEEP (*(SORT(.dtors.*)))
		KEEP (*(.dtors))
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >ram = 0xff

	.eh_frame :
	{
		KEEP (*(.eh_frame))
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >ram = 0xff

	.gcc_except_table :
	{
		*(.gcc_except_table)
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >ram = 0xff
	.jcr            : { KEEP (*(.jcr)) } >ram = 0
	.got            : { *(.got.plt) *(.got) } >ram = 0

	.data ALIGN(4) : 	{
		__data_start = ABSOLUTE(.);
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		CONSTRUCTORS
		. = ALIGN(4);
		__data_end = ABSOLUTE(.) ;
	} >ram = 0xff

	.bss ALIGN(4) :
	{
		__bss_start = ABSOLUTE(.);
		__bss_start__ = ABSOLUTE(.);
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);    /* REQUIRED. LD is flaky without it. */
		__bss_end__ = ABSOLUTE(.);
		__end__ = ABSOLUTE(.);
	} >ram

	.stack 0x80000 : { _stack = .; *(.stack) }
}
