#include "godmode.h"
#include "power.h"

void main(int argc, char** argv)
{
    (void) argv; // unused for now

    // Screen on
    ScreenOn();

    // Run the main program
    if (GodMode(argc) == GODMODE_EXIT_REBOOT) Reboot();
    else PowerOff();
}
