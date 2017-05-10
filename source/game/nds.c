#include "nds.h"
#include "vff.h"

#define CRC16_TABVAL  0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401, 0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400

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

u32 ValidateTwlHeader(TwlHeader* twl) {
    if (twl->logo_crc != NDS_LOGO_CRC16) return 1;
    return (crc16_quick(twl->logo, sizeof(twl->logo)) == NDS_LOGO_CRC16) ? 0 : 1;
}

u32 LoadTwlMetaData(const char* path, TwlHeader* hdr, TwlIconData* icon) {
    u8 ntr_header[0x200]; // we only need the NTR header (ignore TWL stuff)
    TwlHeader* twl = hdr ? hdr : (TwlHeader*) ntr_header;
    u32 hdr_size = hdr ? sizeof(TwlHeader) : 0x200; // load full header if bufefr provided
    UINT br;
    if ((fvx_qread(path, ntr_header, 0, hdr_size, &br) != FR_OK) || (br != hdr_size) ||
        (ValidateTwlHeader(twl) != 0))
        return 1;
    if (!icon) return 0; // done if icon data is not required
    // we don't need anything beyond the v0x0001 icon, so ignore the remainder
    if ((fvx_qread(path, icon, twl->icon_offset, TWLICON_SIZE_DATA(0x0001), &br) != FR_OK) || (br != TWLICON_SIZE_DATA(0x0001)) ||
        (!TWLICON_SIZE_DATA(icon->version)) || (crc16_quick(((u8*) icon) + 0x20, TWLICON_SIZE_DATA(0x0001) - 0x20) != icon->crc_0x0020_0x0840))
        return 1;
    icon->version = 0x0001; // just to be safe
    return 0;
}

// TWL title is max 128(+1) chars long
u32 GetTwlTitle(char* desc, const TwlIconData* twl_icon) {
    const u16* title = twl_icon->title_eng; // english title
    memset(desc, 0, TWLICON_SIZE_DESC + 1);
    for (u32 i = 0; i < TWLICON_SIZE_DESC; i++) desc[i] = title[i];
    return 0;
}

// TWL icon: 32x32 pixel, 8x8 tiles
u32 GetTwlIcon(u8* icon, const TwlIconData* twl_icon) {
    const u32 h = TWLICON_DIM_ICON; // fixed size
    const u32 w = TWLICON_DIM_ICON; // fixed size
    const u16* palette = twl_icon->palette;
    u8* pix4 = (u8*) twl_icon->icon;
    for (u32 y = 0; y < h; y += 8) {
        for (u32 x = 0; x < w; x += 8) {
            for (u32 i = 0; i < 8*8; i++) {
                u32 ix = x + (i & 0x7);
                u32 iy = y + (i >> 3);
                u16 pix555 = palette[((i%2) ? (*pix4 >> 4) : *pix4) & 0xF];
                u8* pix888 = icon + ((iy * w) + ix) * 3;
                *(pix888++) = ((pix555 >> 10) & 0x1F) << 3; // B
                *(pix888++) = ((pix555 >>  5) & 0x1F) << 3; // G
                *(pix888++) = ((pix555 >>  0) & 0x1F) << 3; // R
                if (i % 2) pix4++;
            }
        }
    }
    return 0;
}
