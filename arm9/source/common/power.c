#include "arm.h"
#include "power.h"
#include "i2c.h"
#include "pxi.h"

u32 SetScreenBrightness(int level) {
    u32 arg;

    if (level != BRIGHTNESS_AUTOMATIC) {
        arg = clamp(level, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    } else {
        arg = 0;
    }

    return PXI_DoCMD(PXI_BRIGHTNESS, &arg, 1);
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
    ARM_WbDC();
    ARM_DSB();
    I2C_writeReg(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}

void PowerOff()
{
    I2C_writeReg(I2C_DEV_MCU, 0x22, 1 << 0); // poweroff LCD to prevent MCU hangs
    ARM_WbDC();
    ARM_DSB();
    I2C_writeReg(I2C_DEV_MCU, 0x20, 1 << 0);
    while(true);
}
