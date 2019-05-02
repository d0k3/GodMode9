// C port of byuu's \nall\crc32.hpp, which was released under GPLv3
// https://github.com/eai04191/beat/blob/master/nall/crc32.hpp
// Ported by Hyarion for use with VirtualFatFS

#pragma once

#include "common.h"

u32 crc32_adjust(u32 crc32, u8 input);
u32 crc32_calculate(u32 crc32, const u8* data, u32 length);
u32 crc32_calculate_from_file(const char* fileName, u32 offset, u32 length);
