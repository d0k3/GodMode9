#include "godmode.h"
#include "power.h"
#include "pxi.h"

void main(int argc, char** argv)
{
    (void) argv; // unused for now

    // Wait for ARM11
    PXI_WaitRemote(PXI_READY);
    
    #ifdef AUTORUN_SCRIPT
    // Run the script runner
    if (ScriptRunner(argc) == GODMODE_EXIT_REBOOT) Reboot();
    else PowerOff();
    #else
    // Run the main program
    if (GodMode(argc) == GODMODE_EXIT_REBOOT) Reboot();
    else PowerOff();
    #endif
}
