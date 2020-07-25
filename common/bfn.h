#pragma once

/*
 Addresses and declarations of preexisting BootROM functions
 All of these functions should follow the standard AAPCS convention
*/

#ifdef ARM9

// void waitCycles(u32 cycles)
// delays execution time by cycles
#define BFN_WAITCYCLES	(0xFFFF0198)

// void cpuSet(u32 val, u32 *dest, u32 count)
#define BFN_CPUSET	(0xFFFF03A4)

// void cpuCpy(const u32 *src, u32 *dest, u32 count)
#define BFN_CPUCPY	(0xFFFF03F0)

// u32 enterCriticalSection()
// disables interrupts and returns the old irq state
#define BFN_ENTERCRITICALSECTION	(0xFFFF06EC)

// void leaveCriticalSection(u32 irqstate)
// restores the old irq state
#define BFN_LEAVECRITICALSECTION	(0xFFFF0700)

// bool enableDCache()
// enables the data cache and returns the old dcache bit
#define BFN_ENABLE_DCACHE	(0xFFFF0798)

// bool disableDCache()
// disables the data cache
#define BFN_DISABLE_DCACHE	(0xFFFF07B0)

// bool setDCache(bool enable)
// toggles the data cache
#define BFN_SET_DCACHE	(0xFFFF07C8)

// void invalidateDCache()
// invalidates all data cache entries
#define BFN_INVALIDATE_DCACHE	(0xFFFF07F0)

// void writebackDCache()
// writes back all data cache entries
#define BFN_WRITEBACK_DCACHE	(0xFFFF07FC)

// void writebackInvalidateDCache()
// writes back and invalidates all data cache entries
#define BFN_WRITEBACK_INVALIDATE_DCACHE	(0xFFFF0830)

// void invalidateDCacheRange(u32 start, u32 len)
// invalidates data cache entries
#define BFN_INVALIDATE_DCACHE_RANGE	(0xFFFF0868)

// void writebackDCacheRange(u32 start, u32 len)
// writes back data cache entries
#define BFN_WRITEBACK_DCACHE_RANGE	(0xFFFF0884)

// void writebackInvalidateDCacheRange(u32 start, u32 len)
#define BFN_WRITEBACK_INVALIDATE_DCACHE_RANGE	(0xFFFF08A8)

// void dataSynchronizationBarrier()
#define BFN_DATASYNCBARRIER	(0xFFFF096C)

// bool enableICache()
#define BFN_ENABLE_ICACHE	(0xFFFF0A5C)

// bool disableICache()
#define BFN_DISABLE_ICACHE	(0xFFFF0A74)

// bool setICache(bool enable)
#define BFN_SET_ICACHE	(0xFFFF0A8C)

// void invalidateICache()
#define BFN_INVALIDATE_ICACHE	(0xFFFF0AB4)

// void invalidateICacheRange(u32 start, u32 len)
#define BFN_INVALIDATE_ICACHE_RANGE	(0xFFFF0AC0)

// void enableMPU()
#define BFN_ENABLE_MPU	(0xFFFF0C38)

// void disableMPU()
#define BFN_DISABLE_MPU	(0xFFFF0C48)

// void resetControlRegisters()
// set CR0 to its reset state (MPU & caches disabled, high vectors, TCMs enabled)
// invalidates both instruction and data caches (without previously writing back!!)
#define BFN_RESET_CRS	(0xFFFF0C58)

#else

#define BFN_WAITCYCLES	(0x00011A38)

#define BFN_CPUSET	(0x000116E4)
#define BFN_CPUCPY	(0x00011730)

#define BFN_ENTERCRITICALSECTION	(0x00011AC4)
#define BFN_LEAVECRITICALSECTION	(0x00011AD8)

#define BFN_ENABLE_DCACHE	(0x00011288)
#define BFN_DISABLE_DCACHE	(0x000112A0)
#define BFN_SET_DCACHE	(0x000112B8)

#define BFN_INVALIDATE_DCACHE	(0x000112E0)
#define BFN_WRITEBACK_DCACHE	(0x000112EC)
#define BFN_WRITEBACK_INVALIDATE_DCACHE	(0x00011320)

#define BFN_INVALIDATE_DCACHE_RANGE	(0x00011358)
#define BFN_WRITEBACK_DCACHE_RANGE	(0x00011374)
#define BFN_WRITEBACK_INVALIDATE_DCACHE_RANGE	(0x00011398)

#define BFN_DATASYNCBARRIER	(0x000113C0)

#define BFN_DATAMEMBARRIER	(0x000113E8)

#define BFN_ENABLE_ICACHE	(0x000113F4)
#define BFN_DISABLE_ICACHE	(0x0001140C)
#define BFN_SET_ICACHE	(0x00011424)

// also invalidates the branch target cache in ARM11
#define BFN_INVALIDATE_ICACHE	(0x0001144C)

// WARNING: DOES NOT INVALIDATE THE BRANCH TARGET CACHE
// NEEDS TO INVALIDATE IT AND FLUSH THE PREFETCH BUFFER
#define BFN_INVALIDATE_ICACHE_RANGE	(0x00011458)

// void instructionSynchronizationBarrier()
#define BFN_INSTSYNCBARRIER	(0x00011490)

// void invalidateBranchTargetCache()
#define BFN_INVALIDATE_BT_CACHE_RANGE	(0x000114F4)

#endif
