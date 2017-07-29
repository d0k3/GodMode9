#include "godmode.h"
#include "power.h"

void main(int argc, char** argv)
{
    (void) argc; // unused for now
    (void) argv; // unused for now
    
    // Screen on
    ScreenOn();
    
    // Run the main program
    if (GodMode() == GODMODE_EXIT_REBOOT) Reboot();
    else PowerOff();
}
