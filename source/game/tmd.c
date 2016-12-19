#include "tmd.h"
#include "sha.h"

u32 GetTmdCtr(u8* ctr, TmdContentChunk* chunk) {
    memset(ctr, 0, 16);
    memcpy(ctr, chunk->index, 2);
    return 0;
}

u32 FixTmdHashes(TitleMetaData* tmd) {
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    u32 content_count = getbe16(tmd->content_count);
    // recalculate content info hashes
    for (u32 i = 0, kc = 0; i < 64 && kc < content_count; i++) {
        TmdContentInfo* info = tmd->contentinfo + i;
        u32 k = getbe16(info->cmd_count);
        sha_quick(info->hash, content_list + kc, k * sizeof(TmdContentChunk), SHA256_MODE);
        kc += k;
    }
    sha_quick(tmd->contentinfo_hash, (u8*)tmd->contentinfo, 64 * sizeof(TmdContentInfo), SHA256_MODE);
    return 0;
}

u32 BuildFakeTmd(TitleMetaData* tmd, u8* title_id, u32 n_contents, u32 save_size) {
    const u8 sig_type[4] =  { TMD_SIG_TYPE };
    // safety check: number of contents
    if (n_contents > TMD_MAX_CONTENTS) return 1; // !!!
    // set TMD all zero for a clean start
    memset(tmd, 0x00, TMD_SIZE_N(n_contents));
    // file TMD values
    memcpy(tmd->sig_type, sig_type, 4);
    memset(tmd->signature, 0xFF, 0x100);
    snprintf((char*) tmd->issuer, 0x40, TMD_ISSUER);
    tmd->version = 0x01;
    memcpy(tmd->title_id, title_id, 8);
    tmd->title_type[3] = 0x40; // whatever
    for (u32 i = 0; i < 4; i++) tmd->save_size[i] = (save_size >> (i*8)) & 0xFF; // little endian?
    tmd->content_count[1] = (u8) n_contents;
    memset(tmd->contentinfo_hash, 0xFF, 0x20); // placeholder (hash)
    tmd->contentinfo[0].cmd_count[1] = (u8) n_contents;
    memset(tmd->contentinfo[0].hash, 0xFF, 0x20); // placeholder (hash)
    // nothing to do for content list (yet)
    
    return 0;
}
