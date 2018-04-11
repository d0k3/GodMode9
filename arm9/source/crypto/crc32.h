// C port of byuu's \nall\crc32.hpp, which was released under GPLv3
// https://github.com/eai04191/beat/blob/master/nall/crc32.hpp
// Ported by Hyarion for use with VirtualFatFS

#pragma once
#include "vff.h"

uint32_t crc32_adjust(uint32_t crc32, uint8_t input);
uint32_t crc32_calculate(const uint8_t* data, unsigned int length);
uint32_t crc32_calculate_from_file(FIL inputFile, unsigned int length);
