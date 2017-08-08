#include "power.h"
#include "i2c.h"
#include "cache.h"
#include "timer.h"
#include "pxi.h"

static const u8 br_settings[] = {0x10, 0x17, 0x1E, 0x25, 0x2C, 0x34, 0x3C, 0x44, 0x4D, 0x56, 0x60, 0x6B, 0x79, 0x8C, 0xA7, 0xD2};
static int prev_brightness = -1;
void CheckBrightness() {
    u8 curSlider;
    I2C_readRegBuf(I2C_DEV_MCU, 0x09, &curSlider, 1);
    curSlider >>= 2;
    if (curSlider != prev_brightness) {
        PXI_Wait();
        PXI_Send(br_settings[curSlider]);
        PXI_SetRemote(PXI_SETBRIGHTNESS);
        PXI_Sync();
        prev_brightness = curSlider;
    }
    return;
}

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
