#include "touchcal.h"
#include "ui.h"
#include "hid.h"
#include "crc16.h"
#include "language.h"
#include "spiflash.h"
#include "support.h"


#define TOUCH_CALIB_FILENAME    "gm9calib.bin"


static const HID_CalibrationData default_calib = {
    .screen_x = 0,
    .screen_y = 0,
    .ts_raw = 0
    // ^ wrong: in my console it's 0x780086
    // but this is very much console dependent
    // so it's better to go with a sane default
};

static bool is_calibrated = false;


static bool SetCalibrationDefaults(void)
{
    // Hardcoding this isn't ideal but it's better than
    // leaving the system without any state to work with
    is_calibrated = false; // no, this is not proper calibration
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
    DrawStringCenter(BOT_SCREEN, COLOR_STD_FONT, COLOR_STD_BG, "%s",
        STR_TOUCH_CROSSHAIRS_TO_CALIBRATE_TOUCHSCREEN_USE_STYLUS);

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

    is_calibrated = HID_SetCalibrationData(calibrations, countof(dot_positions), SCREEN_WIDTH_BOT, SCREEN_HEIGHT);
    if (is_calibrated) { // store calibration data in a file
        SaveSupportFile(TOUCH_CALIB_FILENAME, calibrations, sizeof(calibrations));
    }
    return is_calibrated;
}

bool CalibrateTouchFromSupportFile(void) {
    HID_CalibrationData calibrations[10];
    size_t size = LoadSupportFile(TOUCH_CALIB_FILENAME, calibrations, sizeof(calibrations));
    u32 n_dots = size / sizeof(HID_CalibrationData);

    is_calibrated = (n_dots == 0) ? false :
        HID_SetCalibrationData(calibrations, n_dots, SCREEN_WIDTH_BOT, SCREEN_HEIGHT);
    return is_calibrated;
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
    if (!spiflash_read(0x001C, 4, (u8*)&console_id))
        return false;

    if (((console_id >> 8) & 0xFF) != 0x57)
        return false; // not a 3DS

    // read and check DS fw user settings
    // see: https://problemkaputt.de/gbatek.htm#dsfirmwareusersettings
    u32 fw_usercfg_buf[0x100 / 0x4];
    u8* fw_usercfg = (u8*) fw_usercfg_buf;
    if (!spiflash_read(0x1FE00, 0x100, fw_usercfg))
        return false;

    if (getle16(fw_usercfg + 0x72) != crc16_quick(fw_usercfg, 0x70))
        return false;

    // get touchscreen calibration data from user settings
    u8 *ts_data = fw_usercfg + 0x58;
    for (int i = 0; i < 2; i++) {
        int base = i * 6;
        data[i].ts_raw = ts_data[base + 1] << 24 | ts_data[base + 0] << 16 | ts_data[base + 3] << 8 | ts_data[base + 2];
        data[i].screen_x = (((int)ts_data[base + 4]) * SCREEN_WIDTH_BOT) / 256;
        data[i].screen_y = (((int)ts_data[base + 5]) * SCREEN_HEIGHT) / 192;
    }

    is_calibrated = HID_SetCalibrationData(data, 2, SCREEN_WIDTH_BOT, SCREEN_HEIGHT);
    return is_calibrated;
}

bool TouchIsCalibrated(void) {
    return is_calibrated;
}
