#include "virtual.h"

#define VFLAG_ON_O3DS       NAND_TYPE_O3DS
#define VFLAG_ON_N3DS       NAND_TYPE_N3DS
#define VFLAG_ON_NO3DS      NAND_TYPE_NO3DS
#define VFLAG_ON_ALL        (VFLAG_ON_O3DS | VFLAG_ON_N3DS | VFLAG_ON_NO3DS)
#define VFLAG_NAND_SIZE     (1<<31)

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
    else if (strncmp(path, "I:/", (plen >= 3) ? 3 : 2) == 0)
        return VRT_IMGNAND;
    return 0;
}

bool CheckVirtualPath(const char* path) {
    u32 vp_nand = IsVirtualPath(path);
    if ((vp_nand == VRT_EMUNAND) || (vp_nand == VRT_IMGNAND)) {
        return GetNandSizeSectors(vp_nand);
    }
    return vp_nand; // this is safe for SysNAND because we re-check for slot0x05 crypto
}

bool FindVirtualFile(VirtualFile* vfile, const char* path, u32 size)
{
    char* fname = strchr(path, '/');
    u8 nand_src = 0;
    u8 nand_type = 0;
    
    // fix the name
    if (!fname) return false;
    fname++;
    
    // check path vailidity
    nand_src = IsVirtualPath(path);
    if (!nand_src || (fname - path != 3))
        return false;
    
    // check NAND type
    nand_type = CheckNandType(nand_src);
    
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
        if ((nand_src != NAND_SYSNAND) && (GetNandSizeSectors(NAND_SYSNAND) != GetNandSizeSectors(nand_src)))
            return false; // EmuNAND/IMGNAND is too small
        vfile->size = GetNandSizeSectors(NAND_SYSNAND) * 0x200;
    }
    vfile->flags |= nand_src;
    
    return true;
}

int ReadVirtualFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count, u32* bytes_read)
{
    u32 foffset = vfile->offset + offset;
    if (offset >= vfile->size)
        return 0;
    else if ((offset + count) > vfile->size)
        count = vfile->size - offset;
    if (bytes_read) *bytes_read = count;
    if (!(foffset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, u32 src)
        return ReadNandSectors(buffer, foffset / 0x200, count / 0x200, vfile->keyslot,
            vfile->flags & (VRT_SYSNAND | VRT_EMUNAND | VRT_IMGNAND));
    } else { // nonaligned data -> -___-
        u8 l_buffer[0x200];
        u32 nand_src = vfile->flags & (VRT_SYSNAND | VRT_EMUNAND | VRT_IMGNAND);
        u32 keyslot = vfile->keyslot;
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

int WriteVirtualFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count, u32* bytes_written)
{
    u32 foffset = vfile->offset + offset;
    if (offset >= vfile->size)
        return 0;
    else if ((offset + count) > vfile->size)
        count = vfile->size - offset;
    if (bytes_written) *bytes_written = count;
    if (!(foffset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, u32 dest)
        return WriteNandSectors(buffer, foffset / 0x200, count / 0x200, vfile->keyslot,
            vfile->flags & (VRT_SYSNAND | VRT_EMUNAND | VRT_IMGNAND));
    } else return -1; // misaligned data -> not implemented (!!!)
}
