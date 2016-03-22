#include "virtual.h"

#define VFLAG_ON_O3DS       NAND_TYPE_O3DS
#define VFLAG_ON_N3DS       NAND_TYPE_N3DS
#define VFLAG_ON_NO3DS      NAND_TYPE_NO3DS
#define VFLAG_ON_ALL        (VFLAG_ON_O3DS | VFLAG_ON_N3DS | VFLAG_ON_NO3DS)
#define VFLAG_NAND_SIZE     (1<<30)
#define VFLAG_ON_EMUNAND    (1<<31)

VirtualFile virtualFileTemplates[] = {
    { "twln.bin"         , 0x00012E00, 0x08FB5200, 0x03, VFLAG_ON_ALL },
    { "twlp.bin"         , 0x09011A00, 0x020B6600, 0x03, VFLAG_ON_ALL },
    { "agbsave.bin"      , 0x0B100000, 0x00030000, 0x07, VFLAG_ON_ALL },
    { "firm0.bin"        , 0x0B130000, 0x00400000, 0x06, VFLAG_ON_ALL },
    { "firm1.bin"        , 0x0B530000, 0x00400000, 0x06, VFLAG_ON_ALL },
    { "ctrnand_fat.bin"  , 0x0B95CA00, 0x2F3E3600, 0x04, VFLAG_ON_O3DS },
    { "ctrnand_fat.bin"  , 0x0B95AE00, 0x41D2D200, 0x05, VFLAG_ON_N3DS },
    { "ctrnand_fat.bin"  , 0x0B95AE00, 0x41D2D200, 0x04, VFLAG_ON_NO3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x2F5D0000, 0x04, VFLAG_ON_O3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x41ED0000, 0x05, VFLAG_ON_N3DS },
    { "ctrnand_full.bin" , 0x0B930000, 0x41ED0000, 0x04, VFLAG_ON_NO3DS },
    { "nand.bin"         , 0x00000000, 0x00000000, 0xFF, VFLAG_ON_ALL | VFLAG_NAND_SIZE },
    { "nand_minsize.bin" , 0x00000000, 0x3AF00000, 0xFF, VFLAG_ON_O3DS },
    { "nand_minsize.bin" , 0x00000000, 0x4D800000, 0xFF, VFLAG_ON_N3DS | VFLAG_ON_NO3DS },
    { "sector0x96.bin"   , 0x00012C00, 0x00000200, 0xFF, VFLAG_ON_ALL },
    { "nand_hdr.bin"     , 0x00000000, 0x00000200, 0xFF, VFLAG_ON_ALL }
};    

u32 IsVirtualPath(const char* path) {
    u32 plen = strnlen(path, 16);
    if (strncmp(path, "S:/", (plen >= 3) ? 3 : 2) == 0)
        return VRT_SYSNAND;
    else if (strncmp(path, "E:/", (plen >= 3) ? 3 : 2) == 0)
        return VRT_EMUNAND;
    return 0;
}

bool CheckVirtualPath(const char* path) {
    u32 vp_nand = IsVirtualPath(path);
    if (vp_nand == VRT_SYSNAND) {
        return true; // this is safe because we re-check for slot0x05 crypto
    } else if (vp_nand == VRT_EMUNAND) {
        return GetNandSizeSectors(true);
    }
    return false;
}

bool FindVirtualFile(VirtualFile* vfile, const char* path, u32 size)
{
    char* fname = strchr(path, '/');
    bool on_emunand = false;
    u8 nand_type = 0;
    
    // fix the name
    if (!fname) return false;
    fname++;
    
    // check path vailidity
    if (!IsVirtualPath(path) || (fname - path != 3))
        return false;
    
    // check NAND type
    on_emunand = (IsVirtualPath(path) == VRT_EMUNAND);
    nand_type = CheckNandType(on_emunand);
    
    // parse the template list, get the correct one
    u32 n_templates = sizeof(virtualFileTemplates) / sizeof(VirtualFile);
    VirtualFile* curr_template = NULL;
    for (u32 i = 0; i < n_templates; i++) {
        curr_template = &virtualFileTemplates[i];
        if ((curr_template->flags & nand_type) && (strncasecmp(fname, curr_template->name, 32) == 0))
            break;
        else if (size && (curr_template->size == size)) //search by size should be a last resort solution
            break;
        curr_template = NULL;
    }
    if (!curr_template) return false;
    
    // copy current template to vfile
    memcpy(vfile, curr_template, sizeof(VirtualFile));
    
    // process special flags
    if ((vfile->keyslot == 0x05) && !CheckSlot0x05Crypto())
        return false; // keyslot 0x05 not properly set up
    if (vfile->flags & VFLAG_NAND_SIZE) {
        if (on_emunand && (GetNandSizeSectors(false) != GetNandSizeSectors(true)))
            return false; // EmuNAND is too small
        vfile->size = GetNandSizeSectors(false) * 0x200;
    }
    if (on_emunand) vfile->flags |= VFLAG_ON_EMUNAND;
    
    return true;
}

int ReadVirtualFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count)
{
    // simple wrapper function for ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, bool read_emunand)
    return ReadNandSectors(buffer, (vfile->offset + offset) / 0x200, (count+0x1FF) / 0x200, vfile->keyslot, vfile->flags & VFLAG_ON_EMUNAND);
}

int WriteVirtualFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count)
{
    // simple wrapper function for WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, bool write_emunand)
    return WriteNandSectors(buffer, (vfile->offset + offset) / 0x200, (count+0x1FF) / 0x200, vfile->keyslot, vfile->flags & VFLAG_ON_EMUNAND);
}
