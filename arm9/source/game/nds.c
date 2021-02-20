#include "nds.h"
#include "fatmbr.h"
#include "vff.h"
#include "crc16.h"
#include "utf.h"

#define FNT_ENTRY_ISDIR(e) ((bool)((*(u8*)(e))&0x80))
#define FNT_ENTRY_FNLEN(e) ((*(u8*)(e))&~0x80)
#define FNT_ENTRY_LEN(e)   (1 + FNT_ENTRY_FNLEN(e) + (FNT_ENTRY_ISDIR(e)?2:0))
#define FNT_ENTRY_NEXT(e)  (((u8*)(e)) + FNT_ENTRY_LEN(e))


typedef struct {
    u32 subtable_offset;
    u16 file0_id;
    u16 parent_id; // total # of dirs for root entry
} PACKED_ALIGN(1) NitroFntBaseEntry;

typedef struct {
    u32 start_address;
    u32 end_address;
} PACKED_ALIGN(1) NitroFatEntry;


u32 ValidateTwlHeader(TwlHeader* twl) {
    if (twl->logo_crc != NDS_LOGO_CRC16) return 1;
    return (crc16_quick(twl->logo, sizeof(twl->logo)) == NDS_LOGO_CRC16) ? 0 : 1;
}

u32 VerifyTwlIconData(TwlIconData* icon, u32 version) {
    u32 tsize = TWLICON_SIZE_DATA(version);
    u32 isize = TWLICON_SIZE_DATA(icon->version);
    u8* icn = (u8*) icon;

    if (!isize) return 1;
    if (version && (!tsize || tsize > isize)) return 1;

    u32 size = version ? tsize : isize;
    if ((size >= 0x0840) && (crc16_quick(icn + 0x0020, 0x0840 - 0x0020) != icon->crc_0x0020_0x0840)) return 1;
    if ((size >= 0x0940) && (crc16_quick(icn + 0x0020, 0x0940 - 0x0020) != icon->crc_0x0020_0x0940)) return 1;
    if ((size >= 0x1240) && (crc16_quick(icn + 0x0020, 0x0A40 - 0x0020) != icon->crc_0x0020_0x0A40)) return 1;
    if ((size >= 0x23C0) && (crc16_quick(icn + 0x1240, 0x23C0 - 0x1240) != icon->crc_0x1240_0x23C0)) return 1;
    
    return 0;
}

u32 BuildTwlSaveHeader(void* sav, u32 size) {
    const u16 sct_size = 0x200;
    if (size / (u32) sct_size > 0xFFFF)
        return 1;

    // fit max number of sectors into size
    // that's how Nintendo does it ¯\_(ツ)_/¯
    const u16 n_sct_max = size / sct_size;
    u16 n_sct = 1;
    u16 sct_track = 1;
    u16 sct_heads = 1;
    while (true) {
        if (sct_heads < sct_track) {
            u16 n_sct_next = sct_track * (sct_heads+1) * (sct_heads+1);
            if (n_sct_next < n_sct_max) {
                sct_heads++;
                n_sct = n_sct_next;
            } else break;
        } else {
            u16 n_sct_next = (sct_track+1) * sct_heads * sct_heads;
            if (n_sct_next < n_sct_max) {
                sct_track++;
                n_sct = n_sct_next;
            } else break;
        }
    }

    // sectors per cluster (should be identical to Nintendo)
    u8 clr_size = (n_sct > 8 * 1024) ? 8 : (n_sct > 1024) ? 4 : 1;

    // how many FAT sectors do we need?
    u16 tot_clr = align(n_sct, clr_size) / clr_size;
    u32 fat_byte = (align(tot_clr, 2) / 2) * 3; // 2 sectors -> 3 byte
    u16 fat_size = align(fat_byte, sct_size) / sct_size;

    // build the FAT header
    Fat16Header* fat = sav;
    memset(fat, 0x00, sizeof(Fat16Header));
    fat->jmp[0] = 0xE9; // E9 00 00
    memcpy(fat->oemname, "MSWIN4.1", 8);
    fat->sct_size = sct_size; // 512 byte / sector
    fat->clr_size = clr_size; // sectors per cluster
    fat->sct_reserved = 0x0001; // 1 reserved sector
    fat->fat_n = 0x02; // 2 FATs
    fat->root_n = 0x0020; // 32 root dir entries (2 sectors)
    fat->reserved0 = n_sct; // sectors in filesystem
    fat->mediatype = 0xF8; // "hard disk"
    fat->fat_size = fat_size; // sectors per fat (1 sector)
    fat->sct_track = sct_track; // sectors per track (legacy? see above)
    fat->sct_heads = sct_heads; // sectors per head (legacy? see above)
    fat->ndrive = 0x05; // for whatever reason
    fat->boot_sig = 0x29; // "boot signature"
    fat->vol_id = 0x12345678; // volume id
    memcpy(fat->vol_label, "VOLUMELABEL", 11); // standard volume label
    memcpy(fat->fs_type, "FAT12   ", 8); // filesystem type
    fat->magic = 0xAA55;

    return 0;
}

u32 LoadTwlMetaData(const char* path, TwlHeader* hdr, TwlIconData* icon) {
    u8 ALIGN(32) ntr_header[0x200]; // we only need the NTR header (ignore TWL stuff)
    TwlHeader* twl = hdr ? hdr : (void*) ntr_header;
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
u32 GetTwlIcon(u16* icon, const TwlIconData* twl_icon) {
    const u32 h = TWLICON_DIM_ICON; // fixed size
    const u32 w = TWLICON_DIM_ICON; // fixed size
    const u16* palette = twl_icon->palette;
    u8* pix4 = (u8*) twl_icon->icon;
    for (u32 y = 0; y < h; y += 8) {
        for (u32 x = 0; x < w; x += 8) {
            for (u32 i = 0; i < 8*8; i++) {
                u16 pix555;
                u8 r, g, b;
                u32 ix = x + (i & 0x7);
                u32 iy = y + (i >> 3);

                pix555 = palette[((i%2) ? (*pix4 >> 4) : *pix4) & 0xF];
                r = pix555 & 0x1F;
                g = ((pix555 >> 5) & 0x1F) << 1;
                g |= (g >> 1) & 1;
                b = (pix555 >> 10) & 0x1F;
                icon[(iy * w) + ix] = (r << 11) | (g << 5) | b;
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
