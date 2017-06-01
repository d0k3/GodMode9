#include "boss.h"
#include "sha.h"
#include "aes.h"

// http://3dbrew.org/wiki/SpotPass#Content_Header
u32 CheckBossHash(BossHeader* boss, bool encrypted) {
    u8 hash_area[0x14] = { 0 };
    u8 boss_sha256[0x20];
    u8 l_sha256[0x20];
    
    // calculate hash
    memcpy(hash_area, ((u8*) boss) + 0x28, 0x12);
    memcpy(boss_sha256, boss->hash_header, 0x20);
    if (encrypted) {
        CryptBoss(hash_area, 0x28, 0x12, boss);
        CryptBoss(boss_sha256, 0x28 + 0x12, 0x20, boss);
    }
    sha_quick(l_sha256, hash_area, 0x14, SHA256_MODE);
    
    return (memcmp(boss_sha256, l_sha256, 0x20) == 0) ? 0 : 1;
}

u32 ValidateBossHeader(BossHeader* header, u32 fsize) {
    u8 boss_magic[] = { BOSS_MAGIC };
    
    // base checks
    if ((memcmp(header->magic, boss_magic, sizeof(boss_magic)) != 0) ||
        (fsize && (fsize != getbe32(header->filesize))) ||
        (getbe32(header->filesize) < sizeof(BossHeader)) ||
        (getbe16(header->unknown0) != 0x0001) ||
        (getbe16(header->cnthdr_hash_type) != 0x0002) ||
        (getbe16(header->cnthdr_rsa_size) != 0x0002))
        return 1;
    
    // hash check
    if ((CheckBossHash(header, false) != 0) &&
        (CheckBossHash(header, true) != 0))
        return 1;
    
    return 0;
}

u32 GetBossPayloadHashHeader(u8* header, BossHeader* boss) {
    memset(header, 0, BOSS_SIZE_PAYLOAD_HEADER);
    memcpy(header, ((u8*) boss) + 0x15A, 0x1C);
    return 0;
}

u32 CheckBossEncrypted(BossHeader* boss) {
    return CheckBossHash(boss, true);
}

// on the fly de-/encryptor for BOSS
u32 CryptBoss(void* data, u32 offset, u32 size, BossHeader* boss) {
    // check data area (encrypted area starts @0x28)
    if (offset + size < 0x28) return 0;
    else if (offset < 0x28) {
        data = ((u8*)data + 0x28 - offset);
        size -= 0x28 - offset;
        offset = 0x28;
    }
    
    // decrypt BOSS data
    u8 ctr[16] = { 0 };
    memcpy(ctr, boss->ctr12, 12);
    ctr[15] = 0x01;
    use_aeskey(0x38);
    ctr_decrypt_byte(data, data, size, offset - 0x28, AES_CNT_CTRNAND_MODE, ctr);
    
    return 0;
}

// on the fly de-/encryptor for BOSS - sequential
u32 CryptBossSequential(void* data, u32 offset, u32 size) {
    // warning: this will only work for sequential processing
    // unexpected results otherwise
    static BossHeader boss = { 0 };
    static BossHeader* bossptr = NULL;
    
    // fetch boss header from data
    if ((offset == 0) && (size >= sizeof(BossHeader))) {
        bossptr = NULL;
        memcpy(&boss, data, sizeof(BossHeader));
        if (((CheckBossEncrypted(&boss) == 0) &&
             (CryptBoss((u8*) &boss, 0, sizeof(BossHeader), &boss) != 0)) ||
            (ValidateBossHeader(&boss, 0) != 0))
            return 1;
        bossptr = &boss;
    }
    
    // safety check, boss pointer
    if (!bossptr) return 1;
    
    return CryptBoss(data, offset, size, bossptr);
}
