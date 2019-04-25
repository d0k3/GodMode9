#include "touchcal.h"
#include "ui.h"
#include "hid.h"


bool ShowCalibrationDialog(void)
{
    static const u32 dot_positions[][2] = {
        {16, 16},
        {320 - 16, 240 - 16},
        {16, 240 - 16},
        {320 - 16, 16},
    };

    HID_CalibrationData calibrations[countof(dot_positions)];
    for (u32 i = 0; i < countof(dot_positions); i++) {
        calibrations[i].screen_x = dot_positions[i][0];
        calibrations[i].screen_y = dot_positions[i][1];
    }

    // clear screen, draw instructions
    ClearScreen(BOT_SCREEN, COLOR_STD_BG);
    DrawStringCenter(BOT_SCREEN, COLOR_STD_FONT, COLOR_STD_BG,
        "Touch the red crosshairs to\ncalibrate your touchscreen.\n \nUse the stylus for best\nresults!");

    // actual calibration
    for (u32 current_dot = 0; current_dot < countof(dot_positions); current_dot++) {
        // draw four crosshairs
        for (u32 i = 0; i < countof(dot_positions); i++) {
            int color_cross = (i < current_dot) ? COLOR_BRIGHTGREEN : (i == current_dot) ? COLOR_RED : COLOR_STD_FONT;
            for (u32 r = 2; r < 8; r++) {
                DrawPixel(BOT_SCREEN, dot_positions[i][0] + 0, dot_positions[i][1] + r, color_cross);
                DrawPixel(BOT_SCREEN, dot_positions[i][0] + r, dot_positions[i][1] + 0, color_cross);
                DrawPixel(BOT_SCREEN, dot_positions[i][0] + 0, dot_positions[i][1] - r, color_cross);
                DrawPixel(BOT_SCREEN, dot_positions[i][0] - r, dot_positions[i][1] + 0, color_cross);
            }
        }

        // wait until touchscreen released
        while (HID_ReadState() & (BUTTON_B | BUTTON_TOUCH));

        // wait for input, store calibration data
        while (1) {
            u32 pressed = HID_ReadState();
            if (pressed & BUTTON_B) {
                return false;
            } else if (pressed & BUTTON_TOUCH) {
                calibrations[current_dot].ts_raw = HID_ReadRawTouchState();
                break;
            }
        }
    }

    return HID_SetCalibrationData(calibrations, countof(dot_positions), 320, 240);
}

void ShowTouchPlayground(void)
{
    ClearScreen(BOT_SCREEN, COLOR_STD_BG);

    while(1) {
        u16 tx, ty;
        u32 pressed = HID_ReadState();

        if (pressed & BUTTON_TOUCH) {
            HID_ReadTouchState(&tx, &ty);
            if (tx < 320 && ty < 240)
                DrawPixel(BOT_SCREEN, tx, ty, COLOR_BRIGHTYELLOW);
        } else {
            tx = ty = 0;
        }

        DrawStringF(BOT_SCREEN, 16, 16, COLOR_STD_FONT, COLOR_STD_BG, "Current touchscreen coordinates: %3.3d, %3.3d", tx, ty);
        if (pressed & BUTTON_B)
            return;
    }

}
