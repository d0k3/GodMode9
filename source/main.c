#include "common.h"
#include "godmode.h"
#include "ui.h"
#include "i2c.h"
#include "power.h"

u8 *top_screen, *bottom_screen;

void main(int argc, char** argv)
{
    // Turn on backlight
    I2C_writeReg(I2C_DEV_MCU, 0x22, 0x2A);
    
    // Fetch the framebuffer addresses
    if(argc >= 2) {
        // newer entrypoints
        u8 **fb = (u8 **)(void *)argv[1];
        top_screen = fb[0];
        bottom_screen = fb[2];
    } else {
        // outdated entrypoints
        top_screen = (u8*)(*(u32*)0x23FFFE00);
        bottom_screen = (u8*)(*(u32*)0x23FFFE08);
    }
    
    // Run the main program
    (GodMode() == GODMODE_EXIT_REBOOT) ? Reboot() : PowerOff();
}
