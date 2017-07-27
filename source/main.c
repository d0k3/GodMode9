#include "common.h"
#include "godmode.h"
#include "i2c.h"
#include "power.h"

void main(int argc, char** argv)
{
    (void) argc; // unused for now
    (void) argv; // unused for now
    
    // Turn on backlight
    I2C_writeReg(I2C_DEV_MCU, 0x22, 0x2A);
    
    // Run the main program
    (GodMode() == GODMODE_EXIT_REBOOT) ? Reboot() : PowerOff();
}
