#include "godmode.h"
#include "power.h"
#include "pxi.h"
#include "i2c.h"

void main(int argc, char** argv, int entrypoint)
{
    (void) argc;
    (void) argv;

    // Wait for ARM11
    PXI_WaitRemote(PXI_READY);

    PXI_DoCMD(PXI_SCREENINIT, NULL, 0);
    I2C_writeReg(I2C_DEV_MCU, 0x22, 0x2A);

    #ifdef AUTORUN_SCRIPT
    // Run the script runner
    if (ScriptRunner(entrypoint) == GODMODE_EXIT_REBOOT)
    #else
    // Run the main program
    if (GodMode(entrypoint) == GODMODE_EXIT_REBOOT)
    #endif
        Reboot();

    PowerOff();
}
