#include "lottery.h"
#include "rtc.h"
#include "colors.h"
#include "vram0.h"

#define LOTTERY_MAX 8

const LotteryTheme lo_theme[] =
{
    { // standard scheme
        COLOR_WHITE,
        COLOR_BLACK,
        VRAM0_SPLASH_PCX,
        VRAM0_FONT_PBM,
        false
    },
    { // bricked scheme
        RGB(0xFF, 0xFF, 0x00),
        RGB(0x00, 0x00, 0xFF),
        "lottery/bricked/bootrom_splash.pcx",
        "lottery/bricked/font_nbraille_4x6.pbm",
        true
    },
    { // C64 scheme
        RGB(0x7B, 0x71, 0xD5),
        RGB(0x41, 0x30, 0xA4),
        "lottery/c64/c64_splash.pcx",
        "lottery/c64/font_c64_8x8.pbm",
        false
    },
    { // mirror scheme
        COLOR_WHITE,
        COLOR_BLACK,
        "lottery/mirror/mirror_splash.pcx",
        "lottery/mirror/font_6x10_mr.pbm",
        false
    },
    { // zuish scheme
        COLOR_WHITE,
        COLOR_BLACK,
        "lottery/zuish/zuish_splash.pcx",
        "lottery/zuish/font_zuish_8x8.pbm",
        false
    }
};

u32 lo_n = 0;

u32 InitLottery(void) {
    DsTime dstime;
    get_dstime(&dstime);
    lo_n = (DSTIMEGET(&dstime, bcd_s)>>1) % LOTTERY_MAX;
    return lo_n;
}
