#include "power.h"
#include "i2c.h"
#include "cache.h"
#include "timer.h"

void ScreenOn() {
    wait_msec(3); // wait 3ms (cause profi200 said so)
    flushDCacheRange((u8*)0x23FFFE00, sizeof(u8*)*3); // this assumes CakeHax framebuffers, see ui.h
    I2C_writeReg(I2C_DEV_MCU, 0x22, 0x2A); // poweron LCD
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
