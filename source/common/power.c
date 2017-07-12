#include "power.h"
#include "i2c.h"
#include "cache.h"

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
