#include "vnand.h"
#include "nand.h"
#include "agbsave.h"
#include "essentials.h"
#include "unittype.h"

#define VFLAG_MBR           (1UL<<28)
#define VFLAG_ESSENTIAL     (1UL<<29)
#define VFLAG_NEEDS_OTP     (1UL<<30)
#define VFLAG_NAND_SIZE     (1UL<<31)

typedef struct {
    char name[32];
    u32 type;
    u32 subtype;
    u32 index;
    u32 flags;
} __attribute__((packed)) VirtualNandTemplate;

// see NP_TYPE_ and NP_SUBTYPE_ in nand.h
static const VirtualNandTemplate vNandTemplates[] = {
    { "nand_hdr.bin"     , NP_TYPE_NCSD  , NP_SUBTYPE_CTR  , 0, 0 },
    { "twlmbr.bin"       , NP_TYPE_STD   , NP_SUBTYPE_TWL  , 0, VFLAG_MBR },
    { "essential.exefs"  , NP_TYPE_D0K3  , NP_SUBTYPE_NONE , 0, VFLAG_DELETABLE | VFLAG_ESSENTIAL },
    { "sector0x96.bin"   , NP_TYPE_SECRET, NP_SUBTYPE_CTR_N, 0, VFLAG_NEEDS_OTP },
    { "twln.bin"         , NP_TYPE_FAT   , NP_SUBTYPE_TWL  , 0, 0 },
    { "twlp.bin"         , NP_TYPE_FAT   , NP_SUBTYPE_TWL  , 1, 0 },
    { "agbsave.bin"      , NP_TYPE_AGB   , NP_SUBTYPE_CTR  , 0, VFLAG_DELETABLE },
    { "firm0.bin"        , NP_TYPE_FIRM  , NP_SUBTYPE_CTR  , 0, 0 },
    { "firm1.bin"        , NP_TYPE_FIRM  , NP_SUBTYPE_CTR  , 1, 0 },
    { "firm2.bin"        , NP_TYPE_FIRM  , NP_SUBTYPE_CTR  , 2, 0 },
    { "firm3.bin"        , NP_TYPE_FIRM  , NP_SUBTYPE_CTR  , 3, 0 },
    { "firm4.bin"        , NP_TYPE_FIRM  , NP_SUBTYPE_CTR  , 4, 0 },
    { "firm5.bin"        , NP_TYPE_FIRM  , NP_SUBTYPE_CTR  , 5, 0 },
    { "firm6.bin"        , NP_TYPE_FIRM  , NP_SUBTYPE_CTR  , 6, 0 },
    { "firm7.bin"        , NP_TYPE_FIRM  , NP_SUBTYPE_CTR  , 7, 0 },
    { "ctrnand_full.bin" , NP_TYPE_STD   , NP_SUBTYPE_CTR  , 0, 0 },
    { "ctrnand_full.bin" , NP_TYPE_STD   , NP_SUBTYPE_CTR_N, 0, 0 },
    { "ctrnand_fat.bin"  , NP_TYPE_FAT   , NP_SUBTYPE_CTR  , 0, 0 },
    { "ctrnand_fat.bin"  , NP_TYPE_FAT   , NP_SUBTYPE_CTR_N, 0, 0 },
    { "bonus.bin"        , NP_TYPE_BONUS , NP_SUBTYPE_CTR  , 0, VFLAG_DELETABLE },
    { "nand.bin"         , NP_TYPE_NONE  , NP_SUBTYPE_NONE , 0, VFLAG_NAND_SIZE },
    { "nand_minsize.bin" , NP_TYPE_NONE  , NP_SUBTYPE_NONE , 0, 0 }
};

bool CheckVNandDrive(u32 nand_src) {
    return GetNandSizeSectors(nand_src);
}

bool ReadVNandDir(VirtualFile* vfile, VirtualDir* vdir) { // uses a generic vdir object generated in virtual.c
    int n_templates = sizeof(vNandTemplates) / sizeof(VirtualNandTemplate);
    const VirtualNandTemplate* templates = vNandTemplates;
    u32 nand_src = vdir->flags & VRT_SOURCE;
    
    while (++vdir->index < n_templates) {
        const VirtualNandTemplate* template = templates + vdir->index;
        NandPartitionInfo prt_info;
        
        // set up virtual file
        if (template->flags & VFLAG_NAND_SIZE) { // override for "nand.bin"
            prt_info.sector = 0;
            prt_info.count = GetNandSizeSectors(nand_src);
            prt_info.keyslot = 0xFF;
        } else if (GetNandPartitionInfo(&prt_info, template->type, template->subtype, template->index, nand_src) != 0)
            continue;
        snprintf(vfile->name, 32, "%s%s", template->name, (nand_src == VRT_XORPAD) ? ".xorpad" : "");
        vfile->offset = ((u64) prt_info.sector) * 0x200;
        vfile->size = ((u64) prt_info.count) * 0x200;
        vfile->keyslot = prt_info.keyslot;
        vfile->flags = template->flags;
        
        // handle special cases
        if (!vfile->size) continue;
        if ((nand_src == VRT_XORPAD) && ((vfile->keyslot == 0x11) || (vfile->keyslot >= 0x40)))
            continue;
        if ((vfile->keyslot == 0x05) && !CheckSlot0x05Crypto())
            continue; // keyslot 0x05 not properly set up
        if ((vfile->flags & VFLAG_NEEDS_OTP) && !CheckSector0x96Crypto())
            continue; // sector 0x96 crypto not set up
        if (vfile->flags & VFLAG_MBR) {
            vfile->offset += 0x200 - 0x42;
            vfile->size = 0x42;
        }
        if (vfile->flags & VFLAG_ESSENTIAL) {
            const u8 magic[] = { ESSENTIAL_MAGIC };
            u8 data[sizeof(magic)];
            ReadNandBytes(data, vfile->offset, sizeof(magic), vfile->keyslot, nand_src);
            if (memcmp(data, magic, sizeof(magic)) != 0) continue;
            vfile->size = sizeof(EssentialBackup);
        }
        
        // found if arriving here
        vfile->flags |= nand_src;
        return true;
    }
    
    return false;
}

int ReadVNandFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    u32 nand_src = vfile->flags & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND|VRT_XORPAD);
    return ReadNandBytes(buffer, vfile->offset + offset, count, vfile->keyslot, nand_src);
}

int WriteVNandFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count) {
    u32 nand_dst = vfile->flags & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND|VRT_XORPAD);
    int res = WriteNandBytes(buffer, vfile->offset + offset, count, vfile->keyslot, nand_dst);
    return res;
}

u64 GetVNandDriveSize(u32 nand_src) {
    return GetNandSizeSectors(nand_src) * 0x200;
}
