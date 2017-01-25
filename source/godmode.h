#pragma once

#include "common.h"

#define GODMODE_EXIT_REBOOT     0
#define GODMODE_EXIT_POWEROFF   1

u32 GodMode();
void Chainload(u8 *source, size_t size);
