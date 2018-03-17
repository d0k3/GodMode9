#include "smdh.h"
#include "utf.h"

#define SMDH_STRING(str, src, len) utf16_to_utf8((u8*) str, src, len, len)
// shamelessly stolen from bch2obj.py / 3ds_hb_menu :)
#define SMDH_LUT      0,  1,  8,  9,  2,  3, 10, 11, 16, 17, 24, 25, 18, 19, 26, 27, \
                      4,  5, 12, 13,  6,  7, 14, 15, 20, 21, 28, 29, 22, 23, 30, 31, \
                     32, 33, 40, 41, 34, 35, 42, 43, 48, 49, 56, 57, 50, 51, 58, 59, \
                     36, 37, 44, 45, 38, 39, 46, 47, 52, 53, 60, 61, 54, 55, 62, 63

u32 ConvertSmdhIcon(u8* icon, const u16* smdh_icon, u32 w, u32 h) {
    const u32 lut[8*8] = { SMDH_LUT };
    u16* pix565 = (u16*) smdh_icon;
    for (u32 y = 0; y < h; y += 8) {
        for (u32 x = 0; x < w; x += 8) {
            for (u32 i = 0; i < 8*8; i++) {
                u32 ix = x + (lut[i] & 0x7);
                u32 iy = y + (lut[i] >> 3);
                u8* pix888 = icon + ((iy * w) + ix) * 3;
                *(pix888++) = ((*pix565 >>  0) & 0x1F) << 3; // B
                *(pix888++) = ((*pix565 >>  5) & 0x3F) << 2; // G
                *(pix888++) = ((*pix565 >> 11) & 0x1F) << 3; // R
                pix565++;
            }
        }
    }
    return 0;
}

// short desc is max 64(+1) chars long
u32 GetSmdhDescShort(char* desc, const Smdh* smdh) {
    const SmdhAppTitle* title = &(smdh->apptitles[1]); // english title
    memset(desc, 0, SMDH_SIZE_DESC_SHORT + 1);
    SMDH_STRING(desc, title->short_desc, SMDH_SIZE_DESC_SHORT);
    return 0;
}

// long desc is max 128(+1) chars long
u32 GetSmdhDescLong(char* desc, const Smdh* smdh) {
    const SmdhAppTitle* title = &(smdh->apptitles[1]); // english title
    memset(desc, 0, SMDH_SIZE_DESC_LONG + 1);
    SMDH_STRING(desc, title->long_desc, SMDH_SIZE_DESC_LONG);
    return 0;
}

// publisher is max 64(+1) chars long
u32 GetSmdhPublisher(char* pub, const Smdh* smdh) {
    const SmdhAppTitle* title = &(smdh->apptitles[1]); // english title
    memset(pub, 0, SMDH_SIZE_PUBLISHER + 1);
    SMDH_STRING(pub, title->publisher, SMDH_SIZE_PUBLISHER);
    return 0;
}

// small icons are 24x24 => 0x6C0 byte in RGB888
u32 GetSmdhIconSmall(u8* icon, const Smdh* smdh) {
    return ConvertSmdhIcon(icon, smdh->icon_small, SMDH_DIM_ICON_SMALL, SMDH_DIM_ICON_SMALL);
}

// big icons are 48x48 => 0x1B00 byte in RGB888
u32 GetSmdhIconBig(u8* icon, const Smdh* smdh) {
    return ConvertSmdhIcon(icon, smdh->icon_big, SMDH_DIM_ICON_BIG, SMDH_DIM_ICON_BIG);
}
