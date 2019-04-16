#pragma once

#include <types.h>

enum MMU_MemoryType {
	STRONGLY_ORDERED = 0,
	NON_CACHEABLE,
	DEVICE_SHARED,
	DEVICE_NONSHARED,
	CACHED_WT,
	CACHED_WB,
	CACHED_WB_ALLOC,
	MEMORY_TYPES,
};

enum MMU_MemoryAccess {
	NO_ACCESS = 0,
	READ_ONLY,
	READ_WRITE,
};

#define MMU_FLAGS(t, ap, nx, s)	((s) << 25 | (nx) << 24 | (ap) << 8 | (t))

#define MMU_FLAGS_TYPE(f)	((f) & 0xFF)
#define MMU_FLAGS_ACCESS(f)	(((f) >> 8) & 0xFF)

#define MMU_FLAGS_NOEXEC(f)	((f) & BIT(24))
#define MMU_FLAGS_SHARED(f)	((f) & BIT(25))

u32 MMU_Map(u32 va, u32 pa, u32 size, u32 flags);
void MMU_Init(void);
