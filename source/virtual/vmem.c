#include "vmem.h"
#include "unittype.h"
#include "sha.h"
#include "aes.h"

#define VFLAG_BOOT9         (1UL<<28)
#define VFLAG_BOOT11        (1UL<<29)
#define VFLAG_OTP           (1UL<<30)
#define VFLAG_N3DS_ONLY     (1UL<<31)

// offsets provided by SciresM
#define BOOT9_POS   0x08080000
#define BOOT11_POS  0x08090000
#define BOOT9_LEN   0x00010000
#define BOOT11_LEN  0x00010000

// see: https://www.youtube.com/watch?v=wogNzUypLuI
u8 boot9_sha256[0x20] = {
    0x2F, 0x88, 0x74, 0x4F, 0xEE, 0xD7, 0x17, 0x85, 0x63, 0x86, 0x40, 0x0A, 0x44, 0xBB, 0xA4, 0xB9,
    0xCA, 0x62, 0xE7, 0x6A, 0x32, 0xC7, 0x15, 0xD4, 0xF3, 0x09, 0xC3, 0x99, 0xBF, 0x28, 0x16, 0x6F
};
u8 boot11_sha256[0x20] = {
    0x74, 0xDA, 0xAC, 0xE1, 0xF8, 0x06, 0x7B, 0x66, 0xCC, 0x81, 0xFC, 0x30, 0x7A, 0x3F, 0xDB, 0x50,
    0x9C, 0xBE, 0xDC, 0x32, 0xF9, 0x03, 0xAE, 0xBE, 0x90, 0x61, 0x44, 0xDE, 0xA7, 0xA0, 0x75, 0x12
};

// see: https://github.com/SciresM/CTRAesEngine/blob/8312adc74b911a6b9cb9e03982ba3768b8e2e69c/CTRAesEngine/AesEngine.cs#L672-L688
#define OTP_KEY ((u8*) BOOT9_POS + ((IS_DEVKIT) ?  + 0xD700 : 0xD6E0))
#define OTP_IV  (OTP_KEY + 0x10)

// see: http://3dbrew.org/wiki/Memory_layout#ARM9
static const VirtualFile vMemFileTemplates[] = {
    { "itcm.mem"         , 0x01FF8000, 0x00008000, 0xFF, 0 },
    { "arm9.mem"         , 0x08000000, 0x00100000, 0xFF, 0 },
    { "arm9ext.mem"      , 0x08100000, 0x00080000, 0xFF, VFLAG_N3DS_ONLY },
    { "boot9.bin"        , BOOT9_POS , BOOT9_LEN , 0xFF, VFLAG_BOOT9 },
    { "boot11.bin"       , BOOT11_POS, BOOT11_LEN, 0xFF, VFLAG_BOOT11 },
    { "vram.mem"         , 0x18000000, 0x00600000, 0xFF, 0 },
    { "dsp.mem"          , 0x1FF00000, 0x00080000, 0xFF, 0 },
    { "axiwram.mem"      , 0x1FF80000, 0x00080000, 0xFF, 0 },
    { "fcram.mem"        , 0x20000000, 0x08000000, 0xFF, 0 },
    { "fcramext.mem"     , 0x28000000, 0x08000000, 0xFF, VFLAG_N3DS_ONLY },
    { "dtcm.mem"         , 0x30008000, 0x00004000, 0xFF, 0 },
    { "otp.mem"          , 0x10012000, 0x00000100, 0xFF, VFLAG_OTP },
    { "otp_dec.mem"      , 0x10012000, 0x00000100, 0x11, VFLAG_OTP | VFLAG_BOOT9 },
    // { "bootrom.mem"      , 0xFFFF0000, 0x00010000, 0xFF, 0 },
    // { "bootrom_unp.mem"  , 0xFFFF0000, 0x00008000, 0xFF, 0 }
};

bool ReadVMemDir(VirtualFile* vfile, VirtualDir* vdir) { // uses a generic vdir object generated in virtual.c
    int n_templates = sizeof(vMemFileTemplates) / sizeof(VirtualFile);
    const VirtualFile* templates = vMemFileTemplates;
    
    while (++vdir->index < n_templates) {
        // copy current template to vfile
        memcpy(vfile, templates + vdir->index, sizeof(VirtualFile));
        
        // process special flags
        if (((vfile->flags & VFLAG_N3DS_ONLY) && (IS_O3DS)) || // this is not on O3DS consoles
            ((vfile->flags & VFLAG_OTP) && !(IS_UNLOCKED)) || // OTP still locked
            ((vfile->flags & VFLAG_BOOT9) && (sha_cmp(boot9_sha256, (u8*) BOOT9_POS, BOOT9_LEN, SHA256_MODE) != 0)) || // boot9 not found
            ((vfile->flags & VFLAG_BOOT11) && (sha_cmp(boot11_sha256, (u8*) BOOT11_POS, BOOT11_LEN, SHA256_MODE) != 0))) // boot11 not found
            continue; 
        
        // found if arriving here
        return true;
    }
    
    return false;
}

int ReadVMemFile(const VirtualFile* vfile, u8* buffer, u64 offset, u64 count) {
    if ((vfile->flags & VFLAG_OTP) && (vfile->keyslot == 0x11)) {
        u8 __attribute__((aligned(32))) otp_local[vfile->size];
        u8 __attribute__((aligned(32))) otp_iv[0x10];
        u8* otp_mem = (u8*) (u32) vfile->offset;
        memcpy(otp_iv, OTP_IV, 0x10);
        setup_aeskey(0x11, OTP_KEY);
        use_aeskey(0x11);
        cbc_decrypt(otp_mem, otp_local, vfile->size / 0x10, AES_CNT_TITLEKEY_DECRYPT_MODE, otp_iv);
        memcpy(buffer, otp_local + offset, count);
    } else {
        u32 foffset = vfile->offset + offset;
        memcpy(buffer, (u8*) foffset, count);
    }
    return 0;
}

int WriteVMemFile(const VirtualFile* vfile, const u8* buffer, u64 offset, u64 count) {
    if (vfile->flags & (VFLAG_OTP|VFLAG_BOOT9|VFLAG_BOOT11)) {
        return 1; // not writable / writes blocked
    } else {
        u32 foffset = vfile->offset + offset;
        memcpy((u8*) foffset, buffer, count);
    }
    return 0;
}
