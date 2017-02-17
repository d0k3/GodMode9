#include "vnand.h"
#include "nand.h"
#include "agbsave.h"
#include "platform.h"

#define VFLAG_ON_O3DS       NAND_TYPE_O3DS
#define VFLAG_ON_N3DS       NAND_TYPE_N3DS
#define VFLAG_ON_NO3DS      NAND_TYPE_NO3DS
#define VFLAG_ON_NAND       (VFLAG_ON_O3DS | VFLAG_ON_N3DS | VFLAG_ON_NO3DS)
#define VFLAG_GBA_VC        (1<<29)
#define VFLAG_NEEDS_OTP     (1<<30)
#define VFLAG_NAND_SIZE     (1<<31)

// see: http://3dbrew.org/wiki/Flash_Filesystem#NAND_structure
// too much hardcoding, but more readable this way
static const VirtualFile vNandFileTemplates[] = {
    { "twln.bin"         , 0x00012E00, 0x08FB5200, 0x03, VFLAG_ON_NAND },
    { "twlp.bin"         , 0x09011A00, 0x020B6600, 0x03, VFLAG_ON_NAND },
    { "agbsave.bin"      , 0x0B100000, 0x00030000, 0x07, VFLAG_ON_NAND },
    { "firm0.bin"        , 0x0B130000, 0x00400000, 0x06, VFLAG_ON_NAND},
    { "firm1.bin"        , 0x0B530000, 0x00400000, 0x06, VFLAG_ON_NAND},
    { "ctrnand_fat.bin"  , 0x0B95CA00, 0x2F3E3600, 0x04, VFLAG_ON_O3DS },
    { "ctrnand_fat.bin"  , 0x0B95AE00, 0x41D2D200, 0x05, VFLAG_ON_N3DS },
    { "ctrnand_fat.bin"  , 0x0B95AE00, 0x41D2D200, 0x04, VFLAG_ON_NO3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x2F5D0000, 0x04, VFLAG_ON_O3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x41ED0000, 0x05, VFLAG_ON_N3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x41ED0000, 0x04, VFLAG_ON_NO3DS },
    { "sector0x96.bin"   , 0x00012C00, 0x00000200, 0x11, VFLAG_ON_NAND | VFLAG_NEEDS_OTP },
    { "nand.bin"         , 0x00000000, 0x00000000, 0xFF, VFLAG_ON_NAND | VFLAG_NAND_SIZE },
    { "nand_minsize.bin" , 0x00000000, 0x3AF00000, 0xFF, VFLAG_ON_O3DS },
    { "nand_minsize.bin" , 0x00000000, 0x4D800000, 0xFF, VFLAG_ON_N3DS | VFLAG_ON_NO3DS },
    { "nand_hdr.bin"     , 0x00000000, 0x00000200, 0xFF, VFLAG_ON_NAND },
    { "twlmbr.bin"       , 0x000001BE, 0x00000042, 0x03, VFLAG_ON_NAND },
    { "free0x01.bin"     , 0x00000200, 0x00012A00, 0xFF, VFLAG_ON_NAND },
    { "gbavc.sav"        , 0x0B100200, 0x00000000, 0x07, VFLAG_ON_NAND | VFLAG_GBA_VC },
};

bool CheckVNandDrive(u32 nand_src) {
    return GetNandSizeSectors(nand_src);
}

bool ReadVNandDir(VirtualFile* vfile, VirtualDir* vdir) { // uses a generic vdir object generated in virtual.c
    int n_templates = sizeof(vNandFileTemplates) / sizeof(VirtualFile);
    const VirtualFile* templates = vNandFileTemplates;
    u32 nand_src = vdir->flags & VRT_SOURCE;
    
    while (++vdir->index < n_templates) { 
        // get NAND type (O3DS/N3DS/NO3DS), workaround for empty EmuNAND
        u32 nand_type = CheckNandType(nand_src);
        if (!nand_type) nand_type = (GetUnitPlatform() == PLATFORM_3DS) ? NAND_TYPE_O3DS : NAND_TYPE_N3DS;
        
        // copy current template to vfile
        memcpy(vfile, templates + vdir->index, sizeof(VirtualFile));
        
        // XORpad drive handling
        if (nand_src == VRT_XORPAD) {
            snprintf(vfile->name, 32, "%s.xorpad", templates[vdir->index].name);
            if ((vfile->keyslot == 0x11) || (vfile->keyslot >= 0x40) || (vfile->flags & VFLAG_GBA_VC))
                continue;
        }
        
        // process / check special flags
        if (!(vfile->flags & nand_type))
            continue; // virtual file has wrong NAND type
        if ((vfile->keyslot == 0x05) && !CheckSlot0x05Crypto())
            continue; // keyslot 0x05 not properly set up
        if ((vfile->flags & VFLAG_NEEDS_OTP) && !CheckSector0x96Crypto())
            return false; // sector 0x96 crypto not set up
        if (vfile->flags & VFLAG_NAND_SIZE) {
            if ((nand_src != VRT_SYSNAND) && (GetNandSizeSectors(NAND_SYSNAND) != GetNandSizeSectors(nand_src)))
                continue; // EmuNAND/ImgNAND is too small
            vfile->size = GetNandSizeSectors(NAND_SYSNAND) * 0x200;
        }
        if (vfile->flags & VFLAG_GBA_VC) {
            if (CheckAgbSaveCmac(nand_src) != 0) continue;
            vfile->size = GetAgbSaveSize(nand_src);
        }
        
        // found if arriving here
        vfile->flags |= nand_src;
        return true;
    }
    
    return false;
}

int ReadVNandFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count) {
    u32 nand_src = vfile->flags & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND|VRT_XORPAD);
    return ReadNandBytes(buffer, vfile->offset + offset, count, vfile->keyslot, nand_src);
}

int WriteVNandFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count) {
    u32 nand_dst = vfile->flags & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND|VRT_XORPAD);
    int res = WriteNandBytes(buffer, vfile->offset + offset, count, vfile->keyslot, nand_dst);
    if ((res == 0) && (vfile->flags & VFLAG_GBA_VC)) res = FixAgbSaveCmac(nand_dst);
    return res;
}

u64 GetVNandDriveSize(u32 nand_src) {
    return GetNandSizeSectors(nand_src) * 0x200;
}
