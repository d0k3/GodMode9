#include "godmode.h"
#include "power.h"
#include "timer.h"
#include "pxi.h"
#include "i2c.h"

#include "arm.h"

void main(int argc, char** argv, int entrypoint)
{
    (void) argc;
    (void) argv;

    PXI_Reset();

    // Don't even try to send any messages until the
    // ARM11 says it's ready
    PXI_Barrier(ARM11_READY_BARRIER);

    // A pointer to the shared memory region is
    // stored in the thread ID register in the ARM9
    ARM_SetTID(PXI_DoCMD(PXI_GET_SHMEM, NULL, 0));

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
