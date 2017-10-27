#pragma once

#include "common.h"

#define GODMODE_EXIT_REBOOT     0
#define GODMODE_EXIT_POWEROFF   1

u32 GodMode(int entrypoint);
u32 ScriptRunner(int entrypoint);
