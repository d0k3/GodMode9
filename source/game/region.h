// List of region IDs.

#pragma once

// TWL and CTR share region values, except that TWL doesn't have Taiwan.
#define REGION_JPN 0
#define REGION_USA 1
#define REGION_EUR 2
#define REGION_AUS 3
#define REGION_CHN 4
#define REGION_KOR 5
#define REGION_TWN 6

#define REGION_MASK_JPN (1u << REGION_JPN)
#define REGION_MASK_USA (1u << REGION_USA)
#define REGION_MASK_EUR (1u << REGION_EUR)
#define REGION_MASK_AUS (1u << REGION_AUS)
#define REGION_MASK_CHN (1u << REGION_CHN)
#define REGION_MASK_KOR (1u << REGION_KOR)
#define REGION_MASK_TWN (1u << REGION_TWN)

#define TWL_REGION_FREE     0xFFFFFFFF
#define TWL_NUM_REGIONS (REGION_KOR + 1)

#define SMDH_REGION_FREE    0x7FFFFFFF
#define SMDH_NUM_REGIONS (REGION_TWN + 1)

// Names of system regions, short form.
extern const char* const g_regionNamesShort[SMDH_NUM_REGIONS];
// Names of system regions, long form.
extern const char* const g_regionNamesLong[SMDH_NUM_REGIONS];
