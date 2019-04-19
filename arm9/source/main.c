#include "godmode.h"
#include "power.h"
#include "timer.h"
#include "pxi.h"
#include "i2c.h"

#include "arm.h"
#include "shmem.h"

#include "hid.h"

static const HID_CalibrationData default_calib = {
    .screen_x = 0,
    .screen_y = 0,
    .ts_raw = 0
    // ^ wrong: in my console it's 0x780086
    // but this is very much console dependent
    // so it's better to go with a sane default
};

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
    ARM_InitSHMEM();

    // Hardcoding this isn't ideal but it's better than
    // leaving the system without any state to work with
    HID_SetCalibrationData(&default_calib, 1, 320, 240);

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
