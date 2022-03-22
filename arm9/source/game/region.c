#include "common.h"
#include "language.h"
#include "region.h"

// Names of system regions, short form.
const char* const g_regionNamesShort[SMDH_NUM_REGIONS] = {
    "JPN",
    "USA",
    "EUR",
    "AUS", // mostly unused
    "CHN",
    "KOR",
    "TWN",
};

// Names of system regions, long form and translatable.
const char* regionNameLong(int region) {
    switch(region) {
        case REGION_JPN: return STR_REGION_JAPAN;
        case REGION_USA: return STR_REGION_AMERICAS;
        case REGION_EUR: return STR_REGION_EUROPE;
        case REGION_AUS: return STR_REGION_AUSTRALIA;
        case REGION_CHN: return STR_REGION_CHINA;
        case REGION_KOR: return STR_REGION_KOREA;
        case REGION_TWN: return STR_REGION_TAIWAN;
        default:         return STR_REGION_UNKNOWN;
    }
};
