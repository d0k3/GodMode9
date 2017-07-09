#include "power.h"
#include "i2c.h"
#include "cache.h"
#include "ui.h"

void Reboot() {
    ClearScreenF(true, true, COLOR_STD_BG);
    flushEntireDCache();
    if (I2C_writeReg(I2C_DEV_MCU, 0x20, 1 << 2))
        while(true);
}

void PowerOff()
{
    ClearScreenF(true, true, COLOR_STD_BG);
    flushEntireDCache();
    if (I2C_writeReg(I2C_DEV_MCU, 0x20, 1 << 0))
        while (true);
}
