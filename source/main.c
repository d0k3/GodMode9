#include "common.h"
#include "godmode.h"
#include "ui.h"
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
    u32 godmode_exit = GodMode();
    ClearScreenF(true, true, COLOR_STD_BG);
    (godmode_exit == GODMODE_EXIT_REBOOT) ? Reboot() : PowerOff();
    return 0;
}
