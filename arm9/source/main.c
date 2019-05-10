#include "godmode.h"
#include "power.h"
#include "pxi.h"

#include "arm.h"
#include "shmem.h"

#include "hid.h"


void main(int argc, char** argv, int entrypoint)
{
    (void) argc;
    (void) argv;

    PXI_Reset();

    // Don't even try to send any messages until the
    // ARM11 says it's ready
    PXI_Barrier(ARM11_READY_BARRIER);

    // Set screens to RGB16 mode
    PXI_DoCMD(PXI_SET_VMODE, (u32[]){0}, 1);

    // A pointer to the shared memory region is
    // stored in the thread ID register in the ARM9
    ARM_InitSHMEM();

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
