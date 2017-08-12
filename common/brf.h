#pragma once

#ifdef ARM9

/* None of these functions require a working stack */
/* Assume R0-R3, R12 are always clobbered */
#define BRF_INVALIDATE_DCACHE       (0xFFFF07F0)
#define BRF_INVALIDATE_DCACHE_RANGE (0xFFFF0868)
#define BRF_WRITEBACK_DCACHE        (0xFFFF07FC)
#define BRF_WRITEBACK_DCACHE_RANGE  (0xFFFF0884)
#define BRF_WB_INV_DCACHE           (0xFFFF0830)
#define BRF_WB_INV_DCACHE_RANGE     (0xFFFF08A8)
#define BRF_INVALIDATE_ICACHE       (0xFFFF0AB4)
#define BRF_INVALIDATE_ICACHE_RANGE (0xFFFF0AC0)
#define BRF_RESETCP15               (0xFFFF0C58)

#else

#endif
