#include "common.h"
#include "godmode.h"
#include "draw.h"
#include "fs.h"
#include "i2c.h"

void Reboot()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}


void PowerOff()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
    while (true);
}


int main()
{
    (GodMode() == GODMODE_EXIT_REBOOT) ? Reboot() : PowerOff();
    return 0;
}
