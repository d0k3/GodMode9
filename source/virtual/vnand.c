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

bool FindVNandFile(VirtualFile* vfile, u32 nand_src, const char* name, u32 size) {
    // get virtual type (O3DS/N3DS/NO3DS)
    u32 virtual_type = CheckNandType(nand_src);
    // workaround if CheckNandType() comes up with no result (empty EmuNAND)
    if (!virtual_type) virtual_type = (GetUnitPlatform() == PLATFORM_3DS) ? NAND_TYPE_O3DS : NAND_TYPE_N3DS;
    
    // parse the template list, get the correct one
    u32 n_templates = sizeof(vNandFileTemplates) / sizeof(VirtualFile);
    const VirtualFile* curr_template = NULL;
    for (u32 i = 0; i < n_templates; i++) {
        curr_template = &vNandFileTemplates[i];    
        if ((curr_template->flags & virtual_type) && ((strncasecmp(name, curr_template->name, 32) == 0) ||
            (size && (curr_template->size == size)))) // search by size should be a last resort solution
            break; 
        curr_template = NULL;
    }
    if (!curr_template) return false;
    
    // copy current template to vfile
    memcpy(vfile, curr_template, sizeof(VirtualFile));
    
    // process special flags
    if ((vfile->keyslot == 0x05) && !CheckSlot0x05Crypto())
        return false; // keyslot 0x05 not properly set up
    if ((vfile->flags & VFLAG_NEEDS_OTP) && !CheckSector0x96Crypto())
        return false; // sector 0x96 crypto not set up
    if (!(nand_src & VRT_SYSNAND) || (*(vu32*) 0x101401C0))
        vfile->flags &= ~VFLAG_A9LH_AREA; // flag is meaningless outside of A9LH / SysNAND
    if (vfile->flags & VFLAG_NAND_SIZE) {
        if ((nand_src != NAND_SYSNAND) && (GetNandSizeSectors(NAND_SYSNAND) != GetNandSizeSectors(nand_src)))
            return false; // EmuNAND/ImgNAND is too small
        vfile->size = GetNandSizeSectors(NAND_SYSNAND) * 0x200;
    }
    
    return true;
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
