#include "virtual.h"
#include "platform.h"

#define VRT_ANYNAND         (VRT_SYSNAND | VRT_EMUNAND | VRT_IMGNAND)
#define VFLAG_ON_O3DS       NAND_TYPE_O3DS
#define VFLAG_ON_N3DS       NAND_TYPE_N3DS
#define VFLAG_ON_NO3DS      NAND_TYPE_NO3DS
#define VFLAG_ON_NAND       (VFLAG_ON_O3DS | VFLAG_ON_N3DS | VFLAG_ON_NO3DS)
#define VFLAG_ON_MEMORY     VRT_MEMORY
#define VFLAG_N3DS_ONLY     (1<<30)
#define VFLAG_NAND_SIZE     (1<<31)

// see: http://3dbrew.org/wiki/Flash_Filesystem#NAND_structure
// see: http://3dbrew.org/wiki/Memory_layout#ARM9
VirtualFile virtualFileTemplates[] = {
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
    { "nand.bin"         , 0x00000000, 0x00000000, 0xFF, VFLAG_ON_NAND | VFLAG_NAND_SIZE | VFLAG_A9LH_AREA },
    { "nand_minsize.bin" , 0x00000000, 0x3AF00000, 0xFF, VFLAG_ON_O3DS | VFLAG_A9LH_AREA},
    { "nand_minsize.bin" , 0x00000000, 0x4D800000, 0xFF, VFLAG_ON_N3DS | VFLAG_ON_NO3DS | VFLAG_A9LH_AREA },
    { "sector0x96.bin"   , 0x00012C00, 0x00000200, 0xFF, VFLAG_ON_NAND | VFLAG_A9LH_AREA },
    { "nand_hdr.bin"     , 0x00000000, 0x00000200, 0xFF, VFLAG_ON_NAND | VFLAG_A9LH_AREA },
    { "itcm.mem"         , 0x01FF8000, 0x00008000, 0xFF, VFLAG_ON_MEMORY },
    { "arm9.mem"         , 0x08000000, 0x00100000, 0xFF, VFLAG_ON_MEMORY },
    { "arm9ext.mem"      , 0x08010000, 0x00100000, 0xFF, VFLAG_ON_MEMORY | VFLAG_N3DS_ONLY },
    { "vram.mem"         , 0x18000000, 0x00600000, 0xFF, VFLAG_ON_MEMORY },
    { "dsp.mem"          , 0x1FF00000, 0x00080000, 0xFF, VFLAG_ON_MEMORY },
    { "axiwram.mem"      , 0x1FF80000, 0x00080000, 0xFF, VFLAG_ON_MEMORY },
    { "fcram.mem"        , 0x20000000, 0x08000000, 0xFF, VFLAG_ON_MEMORY },
    { "fcramext.mem"     , 0x28000000, 0x08000000, 0xFF, VFLAG_ON_MEMORY | VFLAG_N3DS_ONLY },
    { "dtcm.mem"         , 0x30008000, 0x00004000, 0xFF, VFLAG_ON_MEMORY },
    // { "bootrom.mem"      , 0xFFFF0000, 0x00010000, 0xFF, VFLAG_ON_MEMORY },
    { "bootrom_unp.mem"  , 0xFFFF0000, 0x00008000, 0xFF, VFLAG_ON_MEMORY }
};    

u32 GetVirtualSource(const char* path) {
    u32 plen = strnlen(path, 16);
    if (strncmp(path, "S:/", (plen >= 3) ? 3 : 2) == 0)
        return VRT_SYSNAND;
    else if (strncmp(path, "E:/", (plen >= 3) ? 3 : 2) == 0)
        return VRT_EMUNAND;
    else if (strncmp(path, "I:/", (plen >= 3) ? 3 : 2) == 0)
        return VRT_IMGNAND;
    else if (strncmp(path, "M:/", (plen >= 3) ? 3 : 2) == 0)
        return VRT_MEMORY;
    return 0;
}

bool CheckVirtualDrive(const char* path) {
    u32 virtual_src = GetVirtualSource(path);
    if ((virtual_src == VRT_EMUNAND) || (virtual_src == VRT_IMGNAND)) {
        return GetNandSizeSectors(virtual_src);
    }
    return virtual_src; // this is safe for SysNAND & memory
}

bool FindVirtualFile(VirtualFile* vfile, const char* path, u32 size)
{
    char* fname = strchr(path, '/');
    u32 virtual_src = 0;
    u32 virtual_type = 0;
    
    // fix the name
    if (!fname) return false;
    fname++;
    
    // check path vailidity
    virtual_src = GetVirtualSource(path);
    if (!virtual_src || (fname - path != 3))
        return false;
    
    // check NAND type
    virtual_type = (virtual_src & VRT_ANYNAND) ? CheckNandType(virtual_src) : virtual_src;
    
    // parse the template list, get the correct one
    u32 n_templates = sizeof(virtualFileTemplates) / sizeof(VirtualFile);
    VirtualFile* curr_template = NULL;
    for (u32 i = 0; i < n_templates; i++) {
        curr_template = &virtualFileTemplates[i];    
        if ((curr_template->flags & virtual_type) && ((strncasecmp(fname, curr_template->name, 32) == 0) ||
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
    if (!(virtual_src & VRT_SYSNAND) || (*(vu32*) 0x101401C0))
        vfile->flags &= ~VFLAG_A9LH_AREA; // flag is meaningless outside of A9LH / SysNAND
    if ((vfile->flags & VFLAG_N3DS_ONLY) && (GetUnitPlatform() != PLATFORM_N3DS))
        return false; // this is not on O3DS consoles
    if (vfile->flags & VFLAG_NAND_SIZE) {
        if ((virtual_src != NAND_SYSNAND) && (GetNandSizeSectors(NAND_SYSNAND) != GetNandSizeSectors(virtual_src)))
            return false; // EmuNAND/IMGNAND is too small
        vfile->size = GetNandSizeSectors(NAND_SYSNAND) * 0x200;
    }
    vfile->flags |= virtual_src;
    
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
    
    if (vfile->flags & VFLAG_ON_NAND) {
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
    } else if (vfile->flags & VFLAG_ON_MEMORY) {
        memcpy(buffer, (u8*) foffset, count);
        return 0;
    }
    
    return -1;
}

int WriteVirtualFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count, u32* bytes_written)
{
    u32 foffset = vfile->offset + offset;
    if (offset >= vfile->size)
        return 0;
    else if ((offset + count) > vfile->size)
        count = vfile->size - offset;
    if (bytes_written) *bytes_written = count;
    
    if (vfile->flags & VFLAG_ON_NAND) {
        if (!(foffset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
            // simple wrapper function for WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, u32 dest)
            return WriteNandSectors(buffer, foffset / 0x200, count / 0x200, vfile->keyslot,
                vfile->flags & (VRT_SYSNAND | VRT_EMUNAND | VRT_IMGNAND));
        } else return -1; // misaligned data -> not implemented (!!!)
    } else if (vfile->flags & VFLAG_ON_MEMORY) {
        memcpy((u8*) foffset, buffer, count);
        return 0;
    }
    
    return -1;
}
