#include "vmem.h"
#include "unittype.h"
#include "sha.h"
#include "aes.h"
#include "keydb.h"
#include "sdmmc.h"
#include "itcm.h"
#include "spiflash.h"
#include "i2c.h"

#define VFLAG_CALLBACK      (1UL<<26)
#define VFLAG_BOOT9         (1UL<<27)
#define VFLAG_BOOT11        (1UL<<28)
#define VFLAG_OTP           (1UL<<29)
#define VFLAG_OTP_KEY       (1UL<<30)
#define VFLAG_N3DS_EXT      (1UL<<31)

// checks for boot9 / boot11
#define HAS_BOOT9   (sha_cmp(boot9_sha256, (u8*) __BOOT9_ADDR, __BOOT9_LEN, SHA256_MODE) == 0)
#define HAS_BOOT11  (sha_cmp(boot11_sha256, (u8*) __BOOT11_ADDR, __BOOT11_LEN, SHA256_MODE) == 0)
#define HAS_OTP_KEY (HAS_BOOT9 || ((LoadKeyFromFile(NULL, 0x11, 'N', "OTP") == 0) && (LoadKeyFromFile(NULL , 0x11, 'I', "OTP") == 0)))

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
#define OTP_KEY ((u8*) __BOOT9_ADDR + ((IS_DEVKIT) ?  + 0xD700 : 0xD6E0))
#define OTP_IV  (OTP_KEY + 0x10)

// Custom read/write handlers.
typedef int ReadVMemFileCallback(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);

enum VMemCallbackType {
    VMEM_CALLBACK_OTP_DECRYPTED,
    VMEM_CALLBACK_MCU_REGISTERS,
    VMEM_CALLBACK_FLASH_CID,
    VMEM_CALLBACK_NVRAM,
    VMEM_NUM_CALLBACKS
};

ReadVMemFileCallback ReadVMemOTPDecrypted;
ReadVMemFileCallback ReadVMemMCURegisters;
ReadVMemFileCallback ReadVMemFlashCID;
ReadVMemFileCallback ReadVMemNVRAM;

static ReadVMemFileCallback* const vMemCallbacks[] = {
    ReadVMemOTPDecrypted,
    ReadVMemMCURegisters,
    ReadVMemFlashCID,
    ReadVMemNVRAM
};
STATIC_ASSERT(sizeof(vMemCallbacks) / sizeof(vMemCallbacks[0]) == VMEM_NUM_CALLBACKS);

// see: http://3dbrew.org/wiki/Memory_layout#ARM9
static const VirtualFile vMemFileTemplates[] = {
    { "itcm.mem"         , __ITCM_ADDR  , __ITCM_LEN  , 0xFF, 0 },
    { "arm9.mem"         , __A9RAM0_ADDR, __A9RAM0_LEN, 0xFF, 0 },
    { "arm9ext.mem"      , __A9RAM1_ADDR, __A9RAM1_LEN, 0xFF, VFLAG_N3DS_EXT },
    { "boot9.bin"        , __BOOT9_ADDR , __BOOT9_LEN , 0xFF, VFLAG_READONLY | VFLAG_BOOT9 },
    { "boot11.bin"       , __BOOT11_ADDR, __BOOT11_LEN, 0xFF, VFLAG_READONLY | VFLAG_BOOT11 },
    { "vram.mem"         , __VRAM_ADDR  , __VRAM_LEN  , 0xFF, 0 },
    { "dsp.mem"          , __DSP_ADDR   , __DSP_LEN   , 0xFF, 0 },
    { "axiwram.mem"      , __AWRAM_ADDR , __AWRAM_LEN , 0xFF, 0 },
    { "fcram.mem"        , __FCRAM0_ADDR, __FCRAM0_LEN, 0xFF, 0 },
    { "fcramext.mem"     , __FCRAM1_ADDR, __FCRAM1_LEN, 0xFF, VFLAG_N3DS_EXT },
    { "dtcm.mem"         , __DTCM_ADDR  , __DTCM_LEN  , 0xFF, 0 },
    { "otp.mem"          , __OTP_ADDR   , __OTP_LEN   , 0xFF, VFLAG_READONLY | VFLAG_OTP },
    // { "bootrom.mem"      , 0xFFFF0000, 0x00010000, 0xFF, 0 },
    // { "bootrom_unp.mem"  , 0xFFFF0000, 0x00008000, 0xFF, 0 }

    // Custom callback implementations.
    // Keyslot field has arbitrary meaning, and may not actually be a keyslot.
    { "otp_dec.mem"      , VMEM_CALLBACK_OTP_DECRYPTED, __OTP_LEN , 0x11, VFLAG_CALLBACK | VFLAG_READONLY | VFLAG_OTP | VFLAG_OTP_KEY },
    { "mcu_3ds_regs.mem" , VMEM_CALLBACK_MCU_REGISTERS, 0x00000100, I2C_DEV_MCU, VFLAG_CALLBACK | VFLAG_READONLY },
    { "mcu_dsi_regs.mem" , VMEM_CALLBACK_MCU_REGISTERS, 0x00000100, I2C_DEV_POWER, VFLAG_CALLBACK | VFLAG_READONLY },
    { "sd_cid.mem"       , VMEM_CALLBACK_FLASH_CID    , 0x00000010, 0x00, VFLAG_CALLBACK | VFLAG_READONLY },
    { "nand_cid.mem"     , VMEM_CALLBACK_FLASH_CID    , 0x00000010, 0x01, VFLAG_CALLBACK | VFLAG_READONLY },
    { "nvram.mem"        , VMEM_CALLBACK_NVRAM        , NVRAM_SIZE, 0x00, VFLAG_CALLBACK | VFLAG_READONLY }
};

bool ReadVMemDir(VirtualFile* vfile, VirtualDir* vdir) { // uses a generic vdir object generated in virtual.c
    int n_templates = sizeof(vMemFileTemplates) / sizeof(VirtualFile);
    const VirtualFile* templates = vMemFileTemplates;
    
    while (++vdir->index < n_templates) {
        // copy current template to vfile
        memcpy(vfile, templates + vdir->index, sizeof(VirtualFile));
        
        // process special flags
        if (((vfile->flags & VFLAG_N3DS_EXT) && (IS_O3DS || IS_SIGHAX)) || // this is not on O3DS consoles and locked by sighax
            ((vfile->flags & VFLAG_OTP) && !(IS_UNLOCKED))   || // OTP still locked
            ((vfile->flags & VFLAG_BOOT9) && !(HAS_BOOT9))   || // boot9 not found
            ((vfile->flags & VFLAG_BOOT11) && !(HAS_BOOT11)) || // boot11 not found
            ((vfile->flags & VFLAG_OTP_KEY) && !(HAS_OTP_KEY))) // OTP key not found
            continue; 
        
        // found if arriving here
        return true;
    }
    
    return false;
}

// Read decrypted OTP.
int ReadVMemOTPDecrypted(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    (void) vfile;

    alignas(32) u8 otp_local[__OTP_LEN];
    alignas(32) u8 otp_iv[0x10];
    alignas(32) u8 otp_key[0x10];
    u8* otp_mem = (u8*) __OTP_ADDR;
    
    if (HAS_BOOT9) { // easy setup when boot9 available
        memcpy(otp_iv, OTP_IV, 0x10);
        memcpy(otp_key, OTP_KEY, 0x10);
    } else { // a little bit more complicated without
        if ((LoadKeyFromFile(otp_key, 0x11, 'N', "OTP") != 0) ||
            (LoadKeyFromFile(otp_iv , 0x11, 'I', "OTP") != 0))
            return 1; // crypto not available
    }
    
    setup_aeskey(0x11, otp_key);
    use_aeskey(0x11);
    cbc_decrypt(otp_mem, otp_local, __OTP_LEN / 0x10, AES_CNT_TITLEKEY_DECRYPT_MODE, otp_iv);
    memcpy(buffer, otp_local + offset, count);
    return 0;
}

// Read MCU registers.
int ReadVMemMCURegisters(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    // While it is possible to write MCU registers, that's a good way to
    // brick your system in a way that even ntrboothax can't fix.

    // The table puts the device ID into the keyslot field.
    u8 device = (u8) vfile->keyslot;

    // Read the data.
    return (I2C_readRegBuf(device, (u8) offset, (u8*) buffer, (u32) count)) ? 0 : 1;
}

// Read NAND / SD CID.
int ReadVMemFlashCID(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    // NAND CID if keyslot field != 0.
    bool is_nand = (bool)vfile->keyslot;
    
    u32 cid[4]; // CID is 16 byte in size
    sdmmc_get_cid(is_nand, (u32*) cid);
    memcpy(buffer, ((u8*) cid) + offset, count);
    return 0;
}

// Read NVRAM.
int ReadVMemNVRAM(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    static bool wififlash_initialized = false;
    (void) vfile;
    
    if (!wififlash_initialized) {
        wififlash_initialized = spiflash_get_status();
        if (!wififlash_initialized) return 1;
    }
    
    spiflash_read((u32) offset, (u32) count, buffer);
    return 0;
}

int ReadVMemFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    if (vfile->flags & VFLAG_CALLBACK) {
        if ((offset + count > vfile->size) || (0u + offset + count < offset))
            return 1;
        return vMemCallbacks[vfile->offset](vfile, buffer, offset, count);
    } else {
        u32 foffset = vfile->offset + offset;
        memcpy(buffer, (u8*) foffset, count);
        return 0;
    }
}

int WriteVMemFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count) {
    if (vfile->flags & (VFLAG_READONLY|VFLAG_CALLBACK)) {
        return 1; // not writable / writes blocked
    } else {
        u32 foffset = vfile->offset + offset;
        memcpy((u8*) foffset, buffer, count);
        return 0;
    }
}
