#include "godmode.h"
#include "power.h"
#include "timer.h"
#include "pxi.h"
#include "i2c.h"

#include "vram.h"

void main(int argc, char** argv, int entrypoint)
{
    (void) argc;
    (void) argv;

    PXI_Reset();

    // Don't even try to send any messages until the
    // ARM11 says it's ready
    PXI_Barrier(ARM11_READY_BARRIER);

    PXI_DoCMD(PXI_SCREENINIT, NULL, 0);
    I2C_writeReg(I2C_DEV_MCU, 0x22, 0x2A);

    #ifdef SCRIPT_RUNNER
    // Run the script runner
    if (ScriptRunner(entrypoint) == GODMODE_EXIT_REBOOT)
    #else
    // Run the main program
    if (GodMode(entrypoint) == GODMODE_EXIT_REBOOT)
    #endif
        Reboot();

    PowerOff();
}
