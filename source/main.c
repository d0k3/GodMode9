#include "common.h"
#include "godmode.h"
#include "ui.h"
#include "i2c.h"

u32 origin;

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


void main(int org, int entry_method)
{
    origin = org;
    switch(entry_method)
    {
        case ENTRY_BRAHMA:
            TOP_SCREEN = (u8*)(*(u32*)0x23FFFE00);
            BOT_SCREEN = (u8*)(*(u32*)0x23FFFE08);
            break;

        case ENTRY_GATEWAY:
            TOP_SCREEN = (u8*)(*(u32*)(0x080FFFC0 + ((*(u32*)0x080FFFD8 & 1) << 2)));
            BOT_SCREEN = (u8*)(*(u32*)(0x080FFFD0 + ((*(u32*)0x080FFFDC & 1) << 2)));
            break;

        default:
            while(1);
    }

    u32 godmode_exit = GodMode();
    ClearScreenF(true, true, COLOR_STD_BG);
    (godmode_exit == GODMODE_EXIT_REBOOT) ? Reboot() : PowerOff();
    return;
}
