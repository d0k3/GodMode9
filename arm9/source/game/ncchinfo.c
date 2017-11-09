#include "ncchinfo.h"
#include "ncch.h"
#include "aes.h"

u32 GetNcchInfoVersion(NcchInfoHeader* info) {
    if (!info->n_entries) return 0; // cannot be empty
    if (info->ncch_info_version == NCCHINFO_V3_MAGIC) return 3;
    else if (info->ncch_info_version == NCCHINFO_V4_MAGIC) return 4;
    else return 0;
}

u32 FixNcchInfoEntry(NcchInfoEntry* entry, u32 version) {
    // convert ncchinfo if v3
    if (version == 3) { // ncchinfo v3
        u8* entry_data = (u8*) (entry);
        memmove(entry_data + 56, entry_data + 48, 112);
        memset(entry_data + 48, 0, 8); // zero out nonexistent title id
    } else if (version != 4) { // !ncchinfo v4.0/v4.1/v4.2
        return 1;
    }
    
    // poor man's UTF-16 -> UTF-8
    if (entry->filename[1] == 0x00) {
        for (u32 i = 1; i < (sizeof(entry->filename) / 2); i++)
            entry->filename[i] = entry->filename[i*2];
    }
    
    // fix sdmc: prefix
    if (memcmp(entry->filename, "sdmc:", 5) == 0)
        memmove(entry->filename, entry->filename + 5, 112 - 5);
    
    // workaround (1) for older (v4.0) ncchinfo.bin
    // this combination means seed crypto rather than FixedKey
    if ((entry->ncchFlag7 == 0x01) && entry->ncchFlag3)
        entry->ncchFlag7 = 0x20;
    
    // workaround (2) for older (v4.1) ncchinfo.bin
    if (!entry->size_b) entry->size_b = entry->size_mb * 1024 * 1024;
    
    return 0;
}

u32 BuildNcchInfoXorpad(void* buffer, NcchInfoEntry* entry, u32 size, u32 offset) {
    // set NCCH key
    // build faux NCCH header from entry
    NcchHeader ncch = { 0 };
    memcpy(ncch.signature, entry->keyY, 16);
    ncch.flags[3] = (u8) entry->ncchFlag3;
    ncch.flags[7] = (u8) (entry->ncchFlag7 & ~0x04);
    ncch.programId = ncch.partitionId = entry->titleId;
    if (SetNcchKey(&ncch, NCCH_GET_CRYPTO(&ncch), 1) != 0)
        return 1;
    
    // write xorpad
    memset(buffer, 0, size);
    ctr_decrypt_byte(buffer, buffer, size, offset, AES_CNT_CTRNAND_MODE, entry->ctr);
    
    return 0;
}
