#pragma once

#include "common.h"


// general scheme access
#define LOTTERY_N               ((lo_n > 4) ? 0 : lo_n)
#define LOTTERY_COLOR_FONT      (lo_theme[LOTTERY_N].color_font)
#define LOTTERY_COLOR_BG        (lo_theme[LOTTERY_N].color_bg)
#define LOTTERY_SPLASH          (lo_theme[LOTTERY_N].splash)
#define LOTTERY_FONT            (lo_theme[LOTTERY_N].font)
#define LOTTERY_PROMPTHACK      (lo_theme[LOTTERY_N].prompthack)


typedef struct {
    const u32 color_font;
    const u32 color_bg;
    const char* splash;
    const char* font;
    const bool prompthack;
} __attribute__((packed)) LotteryTheme;

extern const LotteryTheme lo_theme[];
extern u32 lo_n;

u32 InitLottery(void);
