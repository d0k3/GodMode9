#include "vnand.h"
#include "nand.h"
#include "platform.h"

#define VFLAG_ON_O3DS       NAND_TYPE_O3DS
#define VFLAG_ON_N3DS       NAND_TYPE_N3DS
#define VFLAG_ON_NO3DS      NAND_TYPE_NO3DS
#define VFLAG_ON_NAND       (VFLAG_ON_O3DS | VFLAG_ON_N3DS | VFLAG_ON_NO3DS)
#define VFLAG_NEEDS_OTP     (1<<30)
#define VFLAG_NAND_SIZE     (1<<31)

// see: http://3dbrew.org/wiki/Flash_Filesystem#NAND_structure
static const VirtualFile vNandFileTemplates[] = {
    { "twln.bin"         , 0x00012E00, 0x08FB5200, 0x03, VFLAG_ON_NAND },
    { "twlp.bin"         , 0x09011A00, 0x020B6600, 0x03, VFLAG_ON_NAND },
    { "agbsave.bin"      , 0x0B100000, 0x00030000, 0x07, VFLAG_ON_NAND },
    { "firm0.bin"        , 0x0B130000, 0x00400000, 0x06, VFLAG_ON_NAND | VFLAG_A9LH_AREA},
    { "firm1.bin"        , 0x0B530000, 0x00400000, 0x06, VFLAG_ON_NAND | VFLAG_A9LH_AREA},
    { "ctrnand_fat.bin"  , 0x0B95CA00, 0x2F3E3600, 0x04, VFLAG_ON_O3DS },
    { "ctrnand_fat.bin"  , 0x0B95AE00, 0x41D2D200, 0x05, VFLAG_ON_N3DS },
    { "ctrnand_fat.bin"  , 0x0B95AE00, 0x41D2D200, 0x04, VFLAG_ON_NO3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x2F5D0000, 0x04, VFLAG_ON_O3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x41ED0000, 0x05, VFLAG_ON_N3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x41ED0000, 0x04, VFLAG_ON_NO3DS },
    { "sector0x96.bin"   , 0x00012C00, 0x00000200, 0x11, VFLAG_ON_NAND | VFLAG_NEEDS_OTP | VFLAG_A9LH_AREA },
    { "nand.bin"         , 0x00000000, 0x00000000, 0xFF, VFLAG_ON_NAND | VFLAG_NAND_SIZE | VFLAG_A9LH_AREA },
    { "nand_minsize.bin" , 0x00000000, 0x3AF00000, 0xFF, VFLAG_ON_O3DS | VFLAG_A9LH_AREA },
    { "nand_minsize.bin" , 0x00000000, 0x4D800000, 0xFF, VFLAG_ON_N3DS | VFLAG_ON_NO3DS | VFLAG_A9LH_AREA },
    { "nand_hdr.bin"     , 0x00000000, 0x00000200, 0xFF, VFLAG_ON_NAND | VFLAG_A9LH_AREA },
    { "twlmbr.bin"       , 0x000001BE, 0x00000042, 0x03, VFLAG_ON_NAND | VFLAG_A9LH_AREA }
};

bool CheckVNandDrive(u32 nand_src) {
    return GetNandSizeSectors(nand_src);
}

bool ReadVNandDir(VirtualFile* vfile, u32 nand_src) {
    static int num = -1;
    int n_templates = sizeof(vNandFileTemplates) / sizeof(VirtualFile);
    const VirtualFile* templates = vNandFileTemplates;
    
    if (!vfile) { // NULL pointer -> reset dir reader / internal number
        num = -1;
        return true;
    }
    
    while (++num < n_templates) { 
        // get NAND type (O3DS/N3DS/NO3DS), workaround for empty EmuNAND
        u32 nand_type = CheckNandType(nand_src);
        if (!nand_type) nand_type = (GetUnitPlatform() == PLATFORM_3DS) ? NAND_TYPE_O3DS : NAND_TYPE_N3DS;
        
        // copy current template to vfile
        memcpy(vfile, templates + num, sizeof(VirtualFile));
        
        // process / check special flags
        if (!(vfile->flags & nand_type))
            continue; // virtual file has wrong NAND type
        if ((vfile->keyslot == 0x05) && !CheckSlot0x05Crypto())
            continue; // keyslot 0x05 not properly set up
        if ((vfile->flags & VFLAG_NEEDS_OTP) && !CheckSector0x96Crypto())
            return false; // sector 0x96 crypto not set up
        if (!(nand_src & VRT_SYSNAND) || !CheckA9lh())
            vfile->flags &= ~VFLAG_A9LH_AREA; // flag is meaningless outside of A9LH / SysNAND
        if (vfile->flags & VFLAG_NAND_SIZE) {
            if ((nand_src != NAND_SYSNAND) && (GetNandSizeSectors(NAND_SYSNAND) != GetNandSizeSectors(nand_src)))
                continue; // EmuNAND/ImgNAND is too small
            vfile->size = GetNandSizeSectors(NAND_SYSNAND) * 0x200;
        }
        
        // found if arriving here
        vfile->flags |= nand_src;
        return true;
    }
    
    return false;
}

int ReadVNandFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count) {
    u32 foffset = vfile->offset + offset;
    u32 nand_src = vfile->flags & (VRT_SYSNAND | VRT_EMUNAND | VRT_IMGNAND);
    u32 keyslot = vfile->keyslot;
    
    if (!(foffset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, u32 src)
        return ReadNandSectors(buffer, foffset / 0x200, count / 0x200, keyslot, nand_src);
    } else { // misaligned data -> -___-
        u8 l_buffer[0x200];
        int errorcode = 0;
        if (foffset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (foffset % 0x200);
            errorcode = ReadNandSectors(l_buffer, foffset / 0x200, 1, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
            memcpy(buffer, l_buffer + 0x200 - offset_fix, min(offset_fix, count));
            if (count <= offset_fix) return 0;
            foffset += offset_fix;
            buffer += offset_fix;
            count -= offset_fix;
        } // foffset is now aligned and part of the data is read
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            errorcode = ReadNandSectors(buffer, foffset / 0x200, count / 0x200, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            errorcode = ReadNandSectors(l_buffer, (foffset + count) / 0x200, 1, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
            memcpy(buffer + count - count_fix, l_buffer, count_fix);
        }
        return errorcode;
    }
}

int WriteVNandFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count) {
    u32 foffset = vfile->offset + offset;
    u32 nand_dst = vfile->flags & (VRT_SYSNAND | VRT_EMUNAND | VRT_IMGNAND);
    u32 keyslot = vfile->keyslot;
    
    if (!(foffset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, u32 dest)
        return WriteNandSectors(buffer, foffset / 0x200, count / 0x200, keyslot, nand_dst);
    } else { // misaligned data -> -___-
        u8 l_buffer[0x200];
        int errorcode = 0;
        if (foffset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (foffset % 0x200);
            errorcode = ReadNandSectors(l_buffer, foffset / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            memcpy(l_buffer + 0x200 - offset_fix, buffer, min(offset_fix, count));
            errorcode = WriteNandSectors((const u8*) l_buffer, foffset / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            if (count <= offset_fix) return 0;
            foffset += offset_fix;
            buffer += offset_fix;
            count -= offset_fix;
        } // foffset is now aligned and part of the data is written
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            errorcode = WriteNandSectors(buffer, foffset / 0x200, count / 0x200, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            errorcode = ReadNandSectors(l_buffer, (foffset + count) / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            memcpy(l_buffer, buffer + count - count_fix, count_fix);
            errorcode = WriteNandSectors((const u8*) l_buffer, (foffset + count) / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
        }
        return errorcode;
    }
}
