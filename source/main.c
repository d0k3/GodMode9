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

u8 *top_screen, *bottom_screen;

void main(int argc, char** argv)
{
    // Fetch the framebuffer addresses
    if(argc >= 2) {
        // newer entrypoints
        u8 **fb = (u8 **)(void *)argv[2];
        top_screen = fb[0];
        bottom_screen = fb[2];
    } else {
        // outdated entrypoints
        top_screen = (u8*)(*(u32*)0x23FFFE00);
        bottom_screen = (u8*)(*(u32*)0x23FFFE08);
    }
    u32 godmode_exit = GodMode();
    ClearScreenF(true, true, COLOR_STD_BG);
    (godmode_exit == GODMODE_EXIT_REBOOT) ? Reboot() : PowerOff();
}
