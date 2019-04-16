#pragma once

#define DEF_SECT_(n)	extern u32 __##n##_pa, __##n##_va, __##n##_len; static const u32 n##_pa = (u32)&__##n##_pa, n##_va = (u32)&__##n##_va;
DEF_SECT_(vector)
DEF_SECT_(text)
DEF_SECT_(data)
DEF_SECT_(rodata)
DEF_SECT_(bss)
#undef DEF_SECT_

#define SECTION_VA(n)	n##_va
#define SECTION_PA(n)	n##_pa
#define SECTION_LEN(n)	((u32)&__##n##_len)

#define SECTION_TRI(n)	SECTION_VA(n), SECTION_PA(n), SECTION_LEN(n)
