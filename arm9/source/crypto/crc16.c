#include "crc16.h"

#define CRC16_TABVAL  \
	0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401, \
	0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400


// see: https://github.com/TASVideos/desmume/blob/master/desmume/src/bios.cpp#L1070tions
u16 crc16_quick(const void* src, u32 len) {
    const u16 tabval[] = { CRC16_TABVAL };
    u16* data = (u16*) src;
    u16 crc = 0xFFFF;
    
    for (len >>= 1; len; len--) {
        u16 curr = *(data++);
        for (u32 i = 0; i < 4; i++) {
            u16 tval = tabval[crc&0xF];
            crc >>= 4;
            crc ^= tval;
            tval = tabval[(curr >> (4*i))&0xF];
            crc ^= tval;
        }
    }
    
    return crc;
}
