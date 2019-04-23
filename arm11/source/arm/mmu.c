/*
 *   This file is part of GodMode9
 *   Copyright (C) 2018-2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <types.h>
#include <common.h>

#include <arm.h>

#include "arm/mmu.h"

/* Virtual Memory Mapper */
#define L1_VA_IDX(v)	(((v) >> 20) & 0xFFF)
#define L2_VA_IDX(v)	(((v) >> 12) & 0xFF)

#define SECT_ADDR_SHIFT		(20)
#define COARSE_ADDR_SHIFT	(10)
#define PAGE_ADDR_SHIFT		(12)
#define LPAGE_ADDR_SHIFT	(16)

#define SECT_SIZE	(BIT(SECT_ADDR_SHIFT))
#define COARSE_SIZE	(BIT(COARSE_ADDR_SHIFT))
#define PAGE_SIZE	(BIT(PAGE_ADDR_SHIFT))
#define LPAGE_SIZE	(BIT(LPAGE_ADDR_SHIFT))

#define SECT_MASK	(~(SECT_SIZE - 1))
#define COARSE_MASK	(~(COARSE_SIZE - 1))
#define PAGE_MASK	(~(PAGE_SIZE - 1))
#define LPAGE_MASK	(~(LPAGE_SIZE - 1))

#define DESCRIPTOR_L1_UNMAPPED	(0)
#define DESCRIPTOR_L1_COARSE	(1)
#define DESCRIPTOR_L1_SECTION	(2)
#define DESCRIPTOR_L1_RESERVED	(3)

#define DESCRIPTOR_L2_UNMAPPED	(0)
#define DESCRIPTOR_L2_LARGEPAGE	(1)
#define DESCRIPTOR_L2_PAGE_EXEC	(2)
#define DESCRIPTOR_L2_PAGE_NX	(3)

#define DESCRIPTOR_TYPE_MASK	(3)

enum DescriptorType {
	L1_UNMAPPED,
	L1_COARSE,
	L1_SECTION,
	L1_RESERVED,

	L2_UNMAPPED,
	L2_LARGEPAGE,
	L2_PAGE,
};

typedef struct {
	u32 desc[4096];
} __attribute__((aligned(16384))) MMU_Lvl1_Table;

typedef struct {
	u32 desc[256];
} __attribute__((aligned(1024))) MMU_Lvl2_Table;

static MMU_Lvl1_Table MMU_Lvl1_TT;

/* function to allocate 2nd level page tables */
#define MAX_SECOND_LEVEL	(4)
static MMU_Lvl2_Table Lvl2_Tables[MAX_SECOND_LEVEL];
static u32 Lvl2_Allocated = 0;
static MMU_Lvl2_Table *Alloc_Lvl2(void)
{
	if (Lvl2_Allocated == MAX_SECOND_LEVEL)
		return NULL;
	return &Lvl2_Tables[Lvl2_Allocated++];
}


/* functions to convert from internal page flag format to ARM */

/* {TEX, CB} */
static const u8 MMU_TypeLUT[MEMORY_TYPES][2] = {
	[STRONGLY_ORDERED] = {0, 0},
	[NON_CACHEABLE] = {1, 0},
	[DEVICE_SHARED] = {0, 1},
	[DEVICE_NONSHARED] = {2, 0},
	[CACHED_WT] = {0, 2},
	[CACHED_WB] = {1, 3},
	[CACHED_WB_ALLOC] = {1, 3},
};

static u32 MMU_GetTEX(u32 f)
{
	return MMU_TypeLUT[MMU_FLAGS_TYPE(f)][0];
}

static u32 MMU_GetCB(u32 f)
{
	return MMU_TypeLUT[MMU_FLAGS_TYPE(f)][1];
}

static u32 MMU_GetAP(u32 f)
{
	switch(MMU_FLAGS_ACCESS(f)) {
		default:
		case NO_ACCESS:
			return 0;
		case READ_ONLY:
			return 0x21;
		case READ_WRITE:
			return 0x01;
	}
}

static u32 MMU_GetNX(u32 f)
{
	return MMU_FLAGS_NOEXEC(f) ? 1 : 0;
}

static u32 MMU_GetShared(u32 f)
{
	return MMU_FLAGS_SHARED(f) ? 1 : 0;
}

static enum DescriptorType MMU_WalkTT(u32 va)
{
	MMU_Lvl2_Table *coarsepd;
	u32 desc = MMU_Lvl1_TT.desc[L1_VA_IDX(va)];

	switch(desc & DESCRIPTOR_TYPE_MASK) {
		case DESCRIPTOR_L1_UNMAPPED:
			return L1_UNMAPPED;

		case DESCRIPTOR_L1_COARSE:
			break;

		case DESCRIPTOR_L1_SECTION:
			return L1_SECTION;

		case DESCRIPTOR_L1_RESERVED:
			return L1_RESERVED;
	}

	coarsepd = (MMU_Lvl2_Table*)(desc & COARSE_MASK);
	desc = coarsepd->desc[L2_VA_IDX(va)];

	switch(desc & DESCRIPTOR_TYPE_MASK) {
		default:
		case DESCRIPTOR_L2_UNMAPPED:
			return L2_UNMAPPED;

		case DESCRIPTOR_L2_LARGEPAGE:
			return L2_LARGEPAGE;

		case DESCRIPTOR_L2_PAGE_NX:
		case DESCRIPTOR_L2_PAGE_EXEC:
			return L2_PAGE;
	}
}

static MMU_Lvl2_Table *MMU_CoarseFix(u32 va)
{
	enum DescriptorType type;
	MMU_Lvl2_Table *coarsepd;

	type = MMU_WalkTT(va);
	switch(type) {
		case L1_UNMAPPED:
			coarsepd = Alloc_Lvl2();
			if (coarsepd != NULL)
				MMU_Lvl1_TT.desc[L1_VA_IDX(va)] = (u32)coarsepd | DESCRIPTOR_L1_COARSE;
			break;

		case L2_UNMAPPED:
			coarsepd = (MMU_Lvl2_Table*)(MMU_Lvl1_TT.desc[L1_VA_IDX(va)] & COARSE_MASK);
			break;

		default:
			coarsepd = NULL;
			break;
	}

	return coarsepd;
}


/* Sections */
static u32 MMU_SectionFlags(u32 f)
{
	return (MMU_GetShared(f) << 16) | (MMU_GetTEX(f) << 12) |
		(MMU_GetAP(f) << 10) | (MMU_GetNX(f) << 4) |
		(MMU_GetCB(f) << 2) | DESCRIPTOR_L1_SECTION;
}

static bool MMU_MapSection(u32 va, u32 pa, u32 flags)
{
	enum DescriptorType type = MMU_WalkTT(va);
	if (type == L1_UNMAPPED) {
		MMU_Lvl1_TT.desc[L1_VA_IDX(va)] = pa | MMU_SectionFlags(flags);
		return true;
	}

	return false;
}


/* Large Pages */
static u32 MMU_LargePageFlags(u32 f)
{
	return (MMU_GetNX(f) << 15) | (MMU_GetTEX(f) << 12) |
		(MMU_GetShared(f) << 10) | (MMU_GetAP(f) << 4) |
		(MMU_GetCB(f) << 2) | DESCRIPTOR_L2_LARGEPAGE;
}

static bool MMU_MapLargePage(u32 va, u32 pa, u32 flags)
{
	MMU_Lvl2_Table *l2 = MMU_CoarseFix(va);

	if (l2 == NULL)
		return false;

	for (u32 i = va; i < (va + 0x10000); i += 0x1000)
		l2->desc[L2_VA_IDX(i)] = pa | MMU_LargePageFlags(flags);

	return true;
}


/* Pages */
static u32 MMU_PageFlags(u32 f)
{
	return (MMU_GetShared(f) << 10) | (MMU_GetTEX(f) << 6) |
		(MMU_GetAP(f) << 4) | (MMU_GetCB(f) << 2) |
		(MMU_GetNX(f) ? DESCRIPTOR_L2_PAGE_NX : DESCRIPTOR_L2_PAGE_EXEC);
}

static bool MMU_MapPage(u32 va, u32 pa, u32 flags)
{
	MMU_Lvl2_Table *l2 = MMU_CoarseFix(va);

	if (l2 == NULL)
		return false;

	l2->desc[L2_VA_IDX(va)] = pa | MMU_PageFlags(flags);
	return true;
}


static bool MMU_MappingFits(u32 va, u32 pa, u32 len, u32 abits)
{
	return !((va | pa | len) & (BIT(abits) - 1));
}

u32 MMU_Map(u32 va, u32 pa, u32 size, u32 flags)
{
	static const struct {
		u32 bits;
		bool (*mapfn)(u32,u32,u32);
	} VMappers[] = {
		{
			.bits = SECT_ADDR_SHIFT,
			.mapfn = MMU_MapSection,
		},
		{
			.bits = LPAGE_ADDR_SHIFT,
			.mapfn = MMU_MapLargePage,
		},
		{
			.bits = PAGE_ADDR_SHIFT,
			.mapfn = MMU_MapPage,
		},
	};

	while(size > 0) {
		size_t i = 0;
		for (i = 0; i < countof(VMappers); i++) {
			u32 abits = VMappers[i].bits;

			if (MMU_MappingFits(va, pa, size, abits)) {
				bool mapped = (VMappers[i].mapfn)(va, pa, flags);
				u32 offset = BIT(abits);

				// no fun allowed
				if (!mapped)
					return size;

				va += offset;
				pa += offset;
				size -= offset;
				break;
			}
		}

		if (i == countof(VMappers))
			return size;
	}

	return 0;
}

void MMU_Init(void)
{
	u32 ttbr0 = (u32)(&MMU_Lvl1_TT) | 0x12;

	// Set up TTBR0/1 and the TTCR
	ARM_MCR(p15, 0, ttbr0, c2, c0, 0);
	ARM_MCR(p15, 0, 0, c2, c0, 1);
	ARM_MCR(p15, 0, 0, c2, c0, 2);

	// Set up the DACR
	ARM_MCR(p15, 0, 0x55555555, c3, c0, 0);

	// Invalidate the unified TLB
	ARM_MCR(p15, 0, 0, c8, c7, 0);
}
