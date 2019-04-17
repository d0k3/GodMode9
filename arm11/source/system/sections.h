#pragma once

#include <types.h>

#define DEF_SECT_(n)	extern u32 __##n##_pa, __##n##_va, __##n##_len;
DEF_SECT_(vector)
DEF_SECT_(text)
DEF_SECT_(data)
DEF_SECT_(rodata)
DEF_SECT_(bss)
#undef DEF_SECT_

#define SECTION_VA(n)	((u32)&__##n##_va)
#define SECTION_PA(n)	((u32)&__##n##_pa)
#define SECTION_LEN(n)	((u32)&__##n##_len)

#define SECTION_TRI(n)	SECTION_VA(n), SECTION_PA(n), SECTION_LEN(n)
