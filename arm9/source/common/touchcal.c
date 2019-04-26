#include "touchcal.h"
#include "ui.h"
#include "hid.h"
#include "crc16.h"
#include "spiflash.h"


static const HID_CalibrationData default_calib = {
    .screen_x = 0,
    .screen_y = 0,
    .ts_raw = 0
    // ^ wrong: in my console it's 0x780086
    // but this is very much console dependent
    // so it's better to go with a sane default
};


static bool SetCalibrationDefaults(void)
{
	// Hardcoding this isn't ideal but it's better than
    // leaving the system without any state to work with
    return HID_SetCalibrationData(&default_calib, 1, SCREEN_WIDTH_BOT, SCREEN_HEIGHT);
}

bool ShowTouchCalibrationDialog(void)
{
    static const u32 dot_positions[][2] = {
        {16, 16},
        {SCREEN_WIDTH_BOT - 16, SCREEN_HEIGHT - 16},
        {16, SCREEN_HEIGHT - 16},
        {SCREEN_WIDTH_BOT - 16, 16},
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

    // set calibration defaults
    SetCalibrationDefaults();

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

        // wait for input, store calibration data
        while (1) {
            u32 pressed = InputWait(0);
            if (pressed & BUTTON_B) {
                return false;
            } else if (pressed & BUTTON_TOUCH) {
                calibrations[current_dot].ts_raw = HID_ReadRawTouchState();
                break;
            }
        }
    }

    return HID_SetCalibrationData(calibrations, countof(dot_positions), SCREEN_WIDTH_BOT, SCREEN_HEIGHT);
}

void ShowTouchPlayground(void)
{
    ClearScreen(BOT_SCREEN, COLOR_STD_BG);

    while (1) {
        DrawStringF(BOT_SCREEN, 16, 16, COLOR_STD_FONT, COLOR_STD_BG,
        	"Current touchscreen coordinates: 000, 000");
        
        u32 pressed = InputWait(0);
        if (pressed & BUTTON_B) return;

        while (pressed & BUTTON_TOUCH) {
        	u16 tx, ty;
            HID_ReadTouchState(&tx, &ty);
            if (tx < SCREEN_WIDTH_BOT && ty < SCREEN_HEIGHT)
                DrawPixel(BOT_SCREEN, tx, ty, COLOR_BRIGHTYELLOW);
            DrawStringF(BOT_SCREEN, 16, 16, COLOR_STD_FONT, COLOR_STD_BG,
            	"Current touchscreen coordinates: %3.3d, %3.3d", tx, ty);
        	pressed = HID_ReadState();
        }
    }
}

bool CalibrateTouchFromFlash(void) {
	HID_CalibrationData data[2];

    // set calibration defaults
    SetCalibrationDefaults();
 
 	// check SPIflash status
    if (!spiflash_get_status())
        return false;
 
 	// check NVRAM console ID
    u32 console_id = 0;
    spiflash_read(0x001C, 4, (u8*)&console_id);
    if (((console_id >> 8) & 0xFF) != 0x57)
        return false; // not a 3DS

    // read and check DS fw user settings
    // see: https://problemkaputt.de/gbatek.htm#dsfirmwareusersettings
    u32 fw_usercfg_buf[0x100 / 0x4];
    u8* fw_usercfg = (u8*) fw_usercfg_buf;
    spiflash_read(0x1FE00, 0x100, fw_usercfg);
    if (getle16(fw_usercfg + 0x72) != crc16_quick(fw_usercfg, 0x70)) {
    	ShowPrompt(false, "ugh");
    	return false; 
    }

    // get touchscreen calibration data from user settings
    u8 *ts_data = fw_usercfg + 0x58;
    for (int i = 0; i < 2; i++) {
        int base = i * 6;
        data[i].ts_raw = ts_data[base + 1] << 24 | ts_data[base + 0] << 16 | ts_data[base + 3] << 8 | ts_data[base + 2];
        data[i].screen_x = (((int)ts_data[base + 4]) * SCREEN_WIDTH_BOT) / 256;
        data[i].screen_y = (((int)ts_data[base + 5]) * SCREEN_HEIGHT) / 192;
    }
 
    return HID_SetCalibrationData(data, 2, SCREEN_WIDTH_BOT, SCREEN_HEIGHT);
}
