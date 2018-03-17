#include "nds.h"
#include "vff.h"
#include "utf.h"

#define CRC16_TABVAL  0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401, 0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400

#define FNT_ENTRY_ISDIR(e) ((bool)((*(u8*)(e))&0x80))
#define FNT_ENTRY_FNLEN(e) ((*(u8*)(e))&~0x80)
#define FNT_ENTRY_LEN(e)   (1 + FNT_ENTRY_FNLEN(e) + (FNT_ENTRY_ISDIR(e)?2:0))
#define FNT_ENTRY_NEXT(e)  (((u8*)(e)) + FNT_ENTRY_LEN(e))


typedef struct {
    u32 subtable_offset;
    u16 file0_id;
    u16 parent_id; // total # of dirs for root entry
} __attribute__((packed)) NitroFntBaseEntry;

typedef struct {
    u32 start_address;
    u32 end_address;
} __attribute__((packed)) NitroFatEntry;


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
    u32 hdr_size = hdr ? sizeof(TwlHeader) : 0x200; // load full header if buffer provided
    UINT br;
    if ((fvx_qread(path, twl, 0, hdr_size, &br) != FR_OK) || (br != hdr_size) ||
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
    utf16_to_utf8((u8*) desc, title, TWLICON_SIZE_DESC, TWLICON_SIZE_DESC);
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

u32 FindNitroRomDir(u32 dirid, u32* fileid, u8** fnt_entry, TwlHeader* hdr, u8* fnt, u8* fat) {
    NitroFntBaseEntry* fnt_base = (NitroFntBaseEntry*) fnt;
    NitroFntBaseEntry* fnt_dir = &((NitroFntBaseEntry*) fnt)[dirid];
    NitroFatEntry* fat_lut = (NitroFatEntry*) fat;
    
    // base sanity checks
    if (fnt_base->parent_id*sizeof(NitroFntBaseEntry) > fnt_base->subtable_offset) return 1; // invalid FNT
    if (dirid >= fnt_base->parent_id) return 1; // dir ID out of bounds
    
    // set first FNT entry / fileid
    *fnt_entry = fnt + fnt_dir->subtable_offset;
    *fileid = fnt_dir->file0_id;
    
    // check subtable / directory validity
    u32 fid = *fileid;
    for (u8* entry = *fnt_entry;; entry = FNT_ENTRY_NEXT(entry)) {
        if (entry >= fnt + hdr->fnt_size) return 1; // corrupt subtable
        if (!*entry) break; // end of table reached
        if (fat_lut[fid].start_address > fat_lut[fid].end_address) return 1; // corrupt fat
        if (!FNT_ENTRY_ISDIR(entry)) fid++;
    }
    if (fid*sizeof(NitroFatEntry) > hdr->fat_size) return 1; // corrupt fnt / fat
    
    
    return 0;
}

u32 NextNitroRomEntry(u32* fileid, u8** fnt_entry) {
    // check for end of subtable
    if (!*fnt_entry || !**fnt_entry) return 1;
    
    // advance to next entry
    if (!FNT_ENTRY_ISDIR(*fnt_entry)) (*fileid)++;
    *fnt_entry += FNT_ENTRY_LEN(*fnt_entry);
    
    // check for end of subtable
    if (!**fnt_entry) return 1;
    
    return 0;
}

u32 ReadNitroRomEntry(u64* offset, u64* size, bool* is_dir, u32 fileid, u8* fnt_entry, u8* fat) {
    // check for end of subtable
    if (!fnt_entry || !*fnt_entry) return 1;
    
    // decipher FNT entry
    *is_dir = FNT_ENTRY_ISDIR(fnt_entry);
    if (!(*is_dir)) { // for files
        NitroFatEntry* fat_lut = (NitroFatEntry*) fat;
        *offset = fat_lut[fileid].start_address;
        *size = fat_lut[fileid].end_address - fat_lut[fileid].start_address;
    } else { // for dirs
        u32 fnlen = FNT_ENTRY_FNLEN(fnt_entry);
        *offset = (u64) (fnt_entry[1+fnlen]|(fnt_entry[1+fnlen+1]<<8)) & 0xFFF; // dir ID goes in offset
        *size = 0;
    }
    
    return 0;
}
