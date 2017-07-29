#pragma once

#include "common.h"

#define PAYLOAD_MAX_SIZE 0xFFFE0

void __attribute__((noreturn)) Chainload(u8 *source, size_t size);
void __attribute__((noreturn)) BootFirm(FirmHeader *firm, char *path);
