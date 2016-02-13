#include "fs.h"
#include "draw.h"
#include "decryptor/decryptor.h"
#include "decryptor/aes.h"


u32 CryptBuffer(CryptBufferInfo *info)
{
    u8 ctr[16] __attribute__((aligned(32)));
    memcpy(ctr, info->ctr, 16);

    u8* buffer = info->buffer;
    u32 size = info->size;
    u32 mode = info->mode;

    if (info->setKeyY) {
        u8 keyY[16] __attribute__((aligned(32)));
        memcpy(keyY, info->keyY, 16);
        setup_aeskeyY(info->keyslot, keyY);
        info->setKeyY = 0;
    }
    use_aeskey(info->keyslot);

    for (u32 i = 0; i < size; i += 0x10, buffer += 0x10) {
        set_ctr(ctr);
        if ((mode & (0x7 << 27)) == AES_CBC_DECRYPT_MODE)
            memcpy(ctr, buffer, 0x10);
        aes_decrypt((void*) buffer, (void*) buffer, 1, mode);
        if ((mode & (0x7 << 27)) == AES_CBC_ENCRYPT_MODE)
            memcpy(ctr, buffer, 0x10);
        else if ((mode & (0x7 << 27)) == AES_CTR_MODE)
            add_ctr(ctr, 0x1);
    }

    memcpy(info->ctr, ctr, 16);
    
    return 0;
}

u32 CreatePad(PadInfo *info)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 result = 0;
    
    if (!FileCreate(info->filename, true)) // No DebugFileCreate() here - messages are already given
        return 1;
        
    CryptBufferInfo decryptInfo = {.keyslot = info->keyslot, .setKeyY = info->setKeyY, .mode = info->mode, .buffer = buffer};
    memcpy(decryptInfo.ctr, info->ctr, 16);
    memcpy(decryptInfo.keyY, info->keyY, 16);
    u32 size_bytes = info->size_mb * 1024*1024;
    for (u32 i = 0; i < size_bytes; i += BUFFER_MAX_SIZE) {
        u32 curr_block_size = min(BUFFER_MAX_SIZE, size_bytes - i);
        decryptInfo.size = curr_block_size;
        memset(buffer, 0x00, curr_block_size);
        ShowProgress(i, size_bytes);
        CryptBuffer(&decryptInfo);
        if (!DebugFileWrite((void*)buffer, curr_block_size, i)) {
            result = 1;
            break;
        }
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}
