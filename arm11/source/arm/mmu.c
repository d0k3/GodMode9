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

enum {
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
} __attribute__((aligned(16384))) mmuLevel1Table;

typedef struct {
	u32 desc[256];
} __attribute__((aligned(1024))) mmuLevel2Table;

static mmuLevel1Table mmuGlobalTT;

// simple watermark allocator for 2nd level page tables
#define MAX_SECOND_LEVEL	(8)
static mmuLevel2Table mmuCoarseTables[MAX_SECOND_LEVEL];
static u32 mmuCoarseAllocated = 0;
static mmuLevel2Table *mmuAllocateLevel2Table(void)
{
	return &mmuCoarseTables[mmuCoarseAllocated++];
}


// functions to convert from internal page flag format to ARM

// {TEX, CB} pairs
static const u8 mmuTypeLUT[MMU_MEMORY_TYPES][2] = {
	[MMU_STRONG_ORDER] = {0, 0},
	[MMU_UNCACHEABLE] = {1, 0},
	[MMU_DEV_SHARED] = {0, 1},
	[MMU_DEV_NONSHARED] = {2, 0},
	[MMU_CACHE_WT] = {0, 2},
	[MMU_CACHE_WB] = {1, 3},
	[MMU_CACHE_WBA] = {1, 3},
};

static u32 mmuGetTEX(u32 f)
{ return mmuTypeLUT[MMU_FLAGS_TYPE(f)][0]; }
static u32 mmuGetCB(u32 f)
{ return mmuTypeLUT[MMU_FLAGS_TYPE(f)][1]; }
static u32 mmuGetNX(u32 f)
{ return MMU_FLAGS_NOEXEC(f) ? 1 : 0; }
static u32 mmuGetShared(u32 f)
{ return MMU_FLAGS_SHARED(f) ? 1 : 0; }

// access permissions
static const u8 mmuAccessLUT[MMU_ACCESS_TYPES] = {
	[MMU_NO_ACCESS] = 0,
	[MMU_READ_ONLY] = 0x21,
	[MMU_READ_WRITE] = 0x01,
};

static u32 mmuGetAP(u32 f)
{ return mmuAccessLUT[MMU_FLAGS_ACCESS(f)]; }

// other misc helper functions
static unsigned mmuWalkTT(u32 va)
{
	mmuLevel2Table *coarsepd;
	u32 desc = mmuGlobalTT.desc[L1_VA_IDX(va)];

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

	coarsepd = (mmuLevel2Table*)(desc & COARSE_MASK);
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

static mmuLevel2Table *mmuCoarseFix(u32 va)
{
	u32 type;
	mmuLevel2Table *coarsepd;

	type = mmuWalkTT(va);
	switch(type) {
		case L1_UNMAPPED:
			coarsepd = mmuAllocateLevel2Table();
			mmuGlobalTT.desc[L1_VA_IDX(va)] = (u32)coarsepd | DESCRIPTOR_L1_COARSE;
			break;

		case L2_UNMAPPED:
			coarsepd = (mmuLevel2Table*)(mmuGlobalTT.desc[L1_VA_IDX(va)] & COARSE_MASK);
			break;

		default:
			coarsepd = NULL;
			break;
	}

	return coarsepd;
}


/* Sections */
static u32 mmuSectionFlags(u32 f)
{ // converts the internal format to the hardware L1 section format
	return (mmuGetShared(f) << 16) | (mmuGetTEX(f) << 12) |
		(mmuGetAP(f) << 10) | (mmuGetNX(f) << 4) |
		(mmuGetCB(f) << 2) | DESCRIPTOR_L1_SECTION;
}

static void mmuMapSection(u32 va, u32 pa, u32 flags)
{
	mmuGlobalTT.desc[L1_VA_IDX(va)] = pa | mmuSectionFlags(flags);
}


/* Pages */
static u32 mmuPageFlags(u32 f)
{
	return (mmuGetShared(f) << 10) | (mmuGetTEX(f) << 6) |
		(mmuGetAP(f) << 4) | (mmuGetCB(f) << 2) |
		(mmuGetNX(f) ? DESCRIPTOR_L2_PAGE_NX : DESCRIPTOR_L2_PAGE_EXEC);
}

static void mmuMapPage(u32 va, u32 pa, u32 flags)
{
	mmuLevel2Table *l2 = mmuCoarseFix(va);
	l2->desc[L2_VA_IDX(va)] = pa | mmuPageFlags(flags);
}


static bool mmuMappingFits(u32 va, u32 pa, u32 sz, u32 alignment)
{
	return !((va | pa | sz) & (alignment));
}

u32 mmuMapArea(u32 va, u32 pa, u32 size, u32 flags)
{
	static const struct {
		u32 size;
		void (*mapfn)(u32,u32,u32);
	} VMappers[] = {
		{
			.size = BIT(SECT_ADDR_SHIFT),
			.mapfn = mmuMapSection,
		},
		{
			.size = BIT(PAGE_ADDR_SHIFT),
			.mapfn = mmuMapPage,
		},
	};

	while(size > 0) {
		size_t i = 0;
		for (i = 0; i < countof(VMappers); i++) {
			u32 pgsize = VMappers[i].size;

			if (mmuMappingFits(va, pa, size, pgsize-1)) {
				(VMappers[i].mapfn)(va, pa, flags);

				va += pgsize;
				pa += pgsize;
				size -= pgsize;
				break;
			}
		}
		/* alternatively return the unmapped remaining size:
		if (i == countof(VMappers))
			return size;
		*/
	}

	return 0;
}

void mmuInvalidate(void)
{
	ARM_MCR(p15, 0, 0, c8, c7, 0);
}

void mmuInvalidateVA(u32 addr)
{
	ARM_MCR(p15, 0, addr, c8, c7, 2);
}

void mmuInitRegisters(void)
{
	u32 ttbr0 = (u32)(&mmuGlobalTT) | 0x12;

	// Set up TTBR0/1 and the TTCR
	ARM_MCR(p15, 0, ttbr0, c2, c0, 0);
	ARM_MCR(p15, 0, 0, c2, c0, 1);
	ARM_MCR(p15, 0, 0, c2, c0, 2);

	// Set up the DACR
	ARM_MCR(p15, 0, 0x55555555, c3, c0, 0);

	// Invalidate the unified TLB
	ARM_MCR(p15, 0, 0, c8, c7, 0);
}
