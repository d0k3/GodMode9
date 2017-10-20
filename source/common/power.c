#include "power.h"
#include "i2c.h"
#include "cache.h"
#include "pxi.h"

static const u8 br_settings[] = {0x10, 0x17, 0x1E, 0x25, 0x2C, 0x34, 0x3C, 0x44, 0x4D, 0x56, 0x60, 0x6B, 0x79, 0x8C, 0xA7, 0xD2};
static int prev_brightness = -1;
void CheckBrightness() {
    u8 curSlider;
    #ifndef FIXED_BRIGHTNESS
    I2C_readRegBuf(I2C_DEV_MCU, 0x09, &curSlider, 1);
    // Volume Slider value is always between 0x00 and 0x3F
    curSlider >>= 2;
    #else
    curSlider = FIXED_BRIGHTNESS;
    #endif
    if (curSlider != prev_brightness) {
        PXI_DoCMD(PXI_BRIGHTNESS, (u32[]){br_settings[curSlider]}, 1);
        prev_brightness = curSlider;
    }
    return;
}

u32 GetBatteryPercent() {
    u8 battery = 0;
    I2C_readRegBuf(I2C_DEV_MCU, 0x0B, &battery, 1);
    return battery;
}

bool IsCharging() {
    u8 flags = 0;
    I2C_readRegBuf(I2C_DEV_MCU, 0x0F, &flags, 1);
    return flags & (1<<4);
}

void Reboot() {
    I2C_writeReg(I2C_DEV_MCU, 0x22, 1 << 0); // poweroff LCD to prevent MCU hangs
    flushEntireDCache();
    if (I2C_writeReg(I2C_DEV_MCU, 0x20, 1 << 2))
        while(true);
}

void PowerOff()
{
    I2C_writeReg(I2C_DEV_MCU, 0x22, 1 << 0); // poweroff LCD to prevent MCU hangs
    flushEntireDCache();
    if (I2C_writeReg(I2C_DEV_MCU, 0x20, 1 << 0))
        while (true);
}
