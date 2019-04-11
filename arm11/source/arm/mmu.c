#include <types.h>
#include <arm.h>

#include "arm/mmu.h"

static u32 __attribute__((aligned(16384))) MMU_TranslationTable[4096];

// Currently just does a super simple identity mapping
// with all sections set to uncacheable/unbufferable
// and no access limitations (RW and no NX bit set)
void MMU_PopulateTranslationTable(void)
{
	for (int i = 0; i < 4096; i++) {
		MMU_TranslationTable[i] = (i << 20) | (3 << 10) | 2;
	}
}

void MMU_Init(void)
{
	u32 ttbr0 = (u32)(&MMU_TranslationTable) | 0x12;
	ARM_MCR(p15, 0, ttbr0, c2, c0, 0);
	ARM_MCR(p15, 0, 0, c2, c0, 1);
	ARM_MCR(p15, 0, 0, c2, c0, 2);

	ARM_MCR(p15, 0, 0x55555555, c3, c0, 0);

	ARM_MCR(p15, 0, 0, c8, c7, 0);
}
