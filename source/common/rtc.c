#include "rtc.h"
#include "i2c.h"

bool is_valid_dstime(DsTime* dstime) {
    // check the time...
    if ((DSTIMEGET(dstime, bcd_h) >= 24) ||
        (DSTIMEGET(dstime, bcd_m) >= 60) ||
        (DSTIMEGET(dstime, bcd_s) >= 60))
        return false;
        
    // check the date...
    u32 year = 2000 + DSTIMEGET(dstime, bcd_Y);
    u32 month = DSTIMEGET(dstime, bcd_M);
    u32 day = DSTIMEGET(dstime, bcd_D);
    
    // date: year & month
    if ((year >= 2100) || (month == 0) || (month > 12))
        return false;
    
    // date: day
    // see: https://github.com/devkitPro/libnds/blob/9678bf09389cb1fcdc99dfa0357ec0cbe51dd0b7/source/arm7/clock.c#L224-L262
    u32 months_lastday[1+12] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    u32 leap = (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)) ? 1 : 0;
    u32 days_in_month = months_lastday[month] + ((month == 2) ? leap : 0);
    if (day > days_in_month) return false;
    
    return true;
}

bool get_dstime(DsTime* dstime) {
    return (I2C_readRegBuf(I2C_DEV_MCU, 0x30, (void*) dstime, sizeof(DsTime)));
}

bool set_dstime(DsTime* dstime) {
    if (!is_valid_dstime(dstime)) return false;
    for (u32 i = 0; i < sizeof(DsTime); i++) {
        if ((i == 3) || (i == 7)) continue; // skip the unused bytes
        if (!I2C_writeReg(I2C_DEV_MCU, 0x30+i, ((u8*)dstime)[i]))
            return false;
    }
    return true;
}
