#include "aes.h"
#include "gamecart.h"
#include "ncch.h"
#include "ncsd.h"
#include "crc16.h"
#include "sha.h"
#include "types.h"
#include "unittype.h"
#include <string.h>

typedef struct SaveBlockmapHeader {
    u32 flushcount;             // how many times the blockmap was entirely rewritten
    u32 common_min_remap_count; // every physical sector in the flash has been rewritten at least `common_min_remap_count` times
                                // that is, the total remap count for physical sector x representing virtual sector n = blockmap[n].remap_count + common_min_remap_count
} SaveBlockmapHeader;

typedef struct SaveBlockmapEntryV1 {
    struct {
        u8 phys_sec : 7; // physical sector representing this virtual sector
        u8 used : 1;     // whether or not this virtual sector is in use
    };

    u8 remap_count; // amount of times phys_sec has been rewritten

    u8 checksums[8]; // checksum values (if applicable) for each 0x200 sized sector in the full 0x1000-sized physical flash sector this structure covers
} SaveBlockmapEntryV1;

typedef struct SaveBlockmapEntryV2 {
    struct {
        u8 remap_count : 7; // amount of times phys_sec has been rewritten
        u8 used : 1; // whether or not this virtual sector is in use
    };
    u8 phys_sec; // physical sector representing this virtual sector
} SaveBlockmapEntryV2;

typedef struct SaveBlockmapEntry {
    u8 remap_count;  // how many times phys_sec was remapped (written to)
    u8 used;         // whether or not this virtual sector is used
    u8 phys_sec;     // the physical sector this virtual sector is mapped to
    u8 checksums[8]; // checksum values (if applicable) for each 0x200 sized sector in the full 0x1000-sized physical flash sector this structure covers
} SaveBlockmapEntry;

typedef struct SaveJournalEntryData
{
    u8 src_virt_sec; // virtual sector that is being remapped
    u8 dst_virt_sec; // virtual sector whose corresponding physical sector was used to remap src_virt_sec to
    u8 dst_phys_sec; // the physical sector of dst_virt_sec the data of src_virt_sec now resides in
    u8 src_phys_sec; // the previous physical sector where the data in src_virt_sec resided in before the remapping
    u8 new_dst_remap_count; // the new remap count (including this remap write) of the destination physical sector
    u8 src_remap_count; // the remap count of the source physical sector (unchanged)
    u8 new_dst_checksums[8]; // checksums for the data that was written to the destination physical sector, pending to be written to blockmap
} SaveJournalEntryData;

typedef struct SaveJournalEntry
{
    SaveJournalEntryData main;
    SaveJournalEntryData backup; // compared to main to check for integrity before applying to blockmap
    u32 pad;
} SaveJournalEntry;

typedef enum CardSaveWearLevelingType {
    CARD_SAVE_WEAR_LEVELING_V1   = 10,
    CARD_SAVE_WEAR_LEVELING_V2   =  2,

    CARD_SAVE_WEAR_LEVELING_NONE = -1,
} CardSaveWearLevelingType;

typedef enum CardSaveCryptoType {
    CARD_SAVE_CRYPTO_V0      = 0,
    CARD_SAVE_CRYPTO_V1      = 1,
    CARD_SAVE_CRYPTO_V1_N3DS = 2,
    CARD_SAVE_CRYPTO_V2      = 3,

    CARD_SAVE_CRYPTO_INVALID = 0x7FFFFFFF,
} CardSaveCryptoType;

typedef enum CardSaveCryptoKeyslot {
    CARD_SAVE_CMAC_KEYSLOT_O3DS   = 0x33,
    CARD_SAVE_CMAC_KEYSLOT_N3DS   = 0x19,
    CARD_SAVE_CRYPTO_KEYSLOT_O3DS = 0x37,
    CARD_SAVE_CRYPTO_KEYSLOT_N3DS = 0x1A,
} CardSaveCryptoKeyslot;

typedef struct CartSaveContext {
    // wear leveling

    /* min. 87, max 118 journal entries */
    SaveJournalEntry journal[118];
    /* max case is header + 127 v1 blockmap entries + crc16 */
    u8 blockmap[sizeof(SaveBlockmapHeader) + 127 * sizeof(SaveBlockmapEntryV1) + 2];

    CardSaveWearLevelingType type;
    u32 num_journal_entries;  // number of journal entries
    u32 num_blockmap_sectors; // number of sectors covered by the blockmap (nsectors - 1)
    u32 blockmap_size;        // size of the blockmap, in bytes
    u32 logical_size;         // actual number of available data sectors for the DISA save image (nsectors - 2)

    // crypto

    u32 crypto_type;
    bool repeating_ctr;

} CartSaveContext;

static CartSaveContext savectx;

// wear leveling

static void SetBlockmapEntry(u32 index, SaveBlockmapEntry *in_entry) {
    u8 *base_ptr = &savectx.blockmap[sizeof(SaveBlockmapHeader)];
    if (savectx.type == CARD_SAVE_WEAR_LEVELING_V2) {
        SaveBlockmapEntryV2 *entry = &((SaveBlockmapEntryV2 *)base_ptr)[index];
        entry->phys_sec = in_entry->phys_sec;
        entry->remap_count = in_entry->remap_count;
        entry->used = in_entry->used;
    } else {
        SaveBlockmapEntryV1 *entry = &((SaveBlockmapEntryV1 *)base_ptr)[index];
        entry->phys_sec = in_entry->phys_sec;
        entry->remap_count = in_entry->remap_count;
        entry->used = in_entry->used;
        memcpy(entry->checksums, in_entry->checksums, sizeof(entry->checksums));
    }
}

static SaveBlockmapEntry GetBlockmapEntry(u32 index) {
    u8 *base_ptr = &savectx.blockmap[sizeof(SaveBlockmapHeader)];
    if (savectx.type == CARD_SAVE_WEAR_LEVELING_V2) {
        SaveBlockmapEntryV2 *entries = (SaveBlockmapEntryV2 *)base_ptr;
        return (SaveBlockmapEntry) { .checksums = { 0 }, .phys_sec = entries[index].phys_sec, .remap_count = entries[index].remap_count, .used = entries[index].used };
    } else {
        SaveBlockmapEntryV1 *entries = (SaveBlockmapEntryV1 *)base_ptr;
        SaveBlockmapEntry out = (SaveBlockmapEntry) { .phys_sec = entries[index].phys_sec, .remap_count = entries[index].remap_count, .used = entries[index].used };
        memcpy(out.checksums, entries[index].checksums, sizeof(out.checksums));
        return out;
    }
}

static void ApplyJournalEntryToBlockmap(SaveJournalEntry *entry) {
    SaveBlockmapEntry ent = {
        .used = 1,
        .phys_sec = entry->main.dst_phys_sec,
        .remap_count = entry->main.new_dst_remap_count
    };
    memcpy(ent.checksums, entry->main.new_dst_checksums, sizeof(ent.checksums));

    // remap the virtual sector to the new physical sector
    SetBlockmapEntry(entry->main.src_virt_sec, &ent);

    // unless an actual remap didn't occur, we must mark (the physical sector
    // whose virtual sector number we used for remapping this virtual sector to)
    // as free and ready to use for another remap
    if (entry->main.dst_phys_sec != entry->main.src_phys_sec) {
        ent.used = 0;
        ent.phys_sec = entry->main.src_phys_sec;
        ent.remap_count = entry->main.src_remap_count;
        memset(ent.checksums, 0, sizeof(ent.checksums));
        SetBlockmapEntry(entry->main.dst_virt_sec, &ent);
    }
}

static int InitSaveWearLeveling(CartData *cdata, u32 header_offset) {
    // CARD2 does not implement wear leveling for the writable portion of the "ROM"
    if (cdata->cart_id & 0x8000000) {
        savectx.type = CARD_SAVE_WEAR_LEVELING_NONE;
        return 0;
    }

    // SPI flash savedata implements wear leveling
    u32 num_physical_sectors = cdata->save_size / 0x1000;
    // for some reason, the blockmap also covers the sector for the backup blockmap + journal header,
    // despite the logical size calculation (see below) removing this sector from the available logical space
    savectx.num_blockmap_sectors = num_physical_sectors - 1;
    // the blockmap + journal header (combined size 0x1000) exists twice, the first being the main one and the second being the "backup" one.
    // therefore, the actual number of usable data sectors for save data is num_sectors - 2.
    savectx.logical_size = (num_physical_sectors - 2) * 0x1000;

    if (num_physical_sectors > 128) {
        // V2, header + blockmap_entries_v2[511] + crc
        savectx.blockmap_size = 0x400; // fixed size
        savectx.type = CARD_SAVE_WEAR_LEVELING_V2;
    } else {
        // V1, header + blockmap_entries_v1[n] + crc
        savectx.blockmap_size = sizeof(SaveBlockmapHeader) + savectx.num_blockmap_sectors * sizeof(SaveBlockmapEntryV1) + sizeof(u16);
        savectx.type = CARD_SAVE_WEAR_LEVELING_V1;
    }

    // TODO: add CMAC verification ability for DISA

    if (ReadCartSave(savectx.blockmap, header_offset, savectx.blockmap_size, cdata) != 0)
        return 1;

    u32 journal_size = 0x1000 - savectx.blockmap_size;
    savectx.num_journal_entries = journal_size / sizeof(SaveJournalEntry);

    if (ReadCartSave((u8 *)savectx.journal, header_offset + savectx.blockmap_size, journal_size, cdata) != 0)
        return 1;

    u8 *crcLoc = &savectx.blockmap[savectx.blockmap_size - 2];
    u16 expected_bmap_crc = crcLoc[0] | crcLoc[1] << 8;
    u16 calc_bmap_crc = crc16_quick(savectx.blockmap, savectx.blockmap_size - 2);
    if (expected_bmap_crc != calc_bmap_crc) {
        return 1;
    }

    u32 journal_idx = 0;

    for (; journal_idx < savectx.num_journal_entries; journal_idx++) {
        SaveJournalEntry *ent = &savectx.journal[journal_idx];

        // journal entry should point to a valid physical sector
        if (ent->main.dst_phys_sec) {
            // reached the end of the journal
            if (ent->main.src_virt_sec >= savectx.num_blockmap_sectors)
                break;

            ApplyJournalEntryToBlockmap(ent);
        }
    }

    u8 blank[sizeof(SaveJournalEntry)];
    memset(blank, 0xFF, sizeof(blank));
    u32 num_bad_journal_entries = 0;
    for (; journal_idx < savectx.num_journal_entries; journal_idx++) {
        num_bad_journal_entries += memcmp((u8 *)&savectx.journal[journal_idx], blank, sizeof(SaveJournalEntry)) != 0;
    }

    if (num_bad_journal_entries) {
        return 1;
    }

    return 0;
}

// crypto

static int InitCtrCardSaveCryptoKey(CartData *cdata) {
    NcsdHeader *ncsd = (NcsdHeader *) (void *) cdata->header;
    // save data crypto (if supported)
    u32 save_media_old = ncsd->partition_flags[7];
    u32 save_media_new = ncsd->partition_flags[3];
    u8 save_crypto_keysel_base = ncsd->partition_flags[1];
    u8 save_crypto_keysel_extra = ncsd->extra_save_keysel;
    s32 save_crypto_keysel = 0;
    bool is_card2 = cdata->cart_id & 0x8000000;

    bool supported_save_crypto =
        (save_media_old == 0 && save_media_new == 0) || /* SPI save flash (very old carts) */
        (save_media_old == 1) || /* old SPI flash */
        (save_media_old == 0 && save_media_new == 1) || /* newer SPI flash */
        is_card2 /* CARD2 */;

    // skip everything if there isn't save data or its crypto is not supported
    if (cdata->save_type == CARD_SAVE_NONE || !supported_save_crypto) {
        savectx.crypto_type = CARD_SAVE_CRYPTO_INVALID;
        return 1;
    }

    if (save_media_old == 0 && save_media_new == 0 && !is_card2)
        savectx.repeating_ctr = true;

    if (is_card2)
        save_crypto_keysel = save_crypto_keysel_base + save_crypto_keysel_extra;
    else if (save_media_old != 0)
        save_crypto_keysel = 0;
    else if (save_media_new != 0)
        save_crypto_keysel = save_crypto_keysel_base + save_crypto_keysel_extra;
    else
        save_crypto_keysel = -1;

    save_crypto_keysel += 1;

    u8 save_key_y[16] = { 0 };

    struct {
        NcchHeader ncch;
        NcchExtHeader exthdr;
    } hdrs;

    ReadCartBytes(&hdrs, ncsd->partitions[0].offset * NCSD_MEDIA_UNIT, sizeof(hdrs), cdata, false);
    DecryptNcch(&hdrs.exthdr, NCCH_EXTHDR_OFFSET, sizeof(NcchExtHeader), &hdrs.ncch, NULL);
    u8 *cart_unique_id = cdata->header + 0x4000;

    switch (save_crypto_keysel) {
        case 0: // "backup security version" -1
            memcpy(save_key_y, hdrs.exthdr.signature, 8);
            memcpy(&save_key_y[8], &cdata->cart_id, 4);
            memcpy(&save_key_y[12], &cdata->cart_id2, 4);
            savectx.crypto_type = CARD_SAVE_CRYPTO_V0;
            break;

        case 1: // "backup security version" 0
        case 11: // "backup security version" 10
        {
            // version 10 (11) is not supported on O3DS
            if (IS_O3DS && save_crypto_keysel == 11) {
                savectx.crypto_type = CARD_SAVE_CRYPTO_INVALID;
                return 1;
            }

            u8 tmpbuf[0x48];
            u8 hash[0x20];
            memcpy(tmpbuf, hdrs.exthdr.signature, 8);
            memcpy(&tmpbuf[8], cart_unique_id, 0x40);

            sha_quick(hash, tmpbuf, 0x48, SHA256_MODE);
            memcpy(save_key_y, hash, 16);
            savectx.crypto_type = save_crypto_keysel == 11 ? CARD_SAVE_CRYPTO_V1_N3DS : CARD_SAVE_CRYPTO_V1;
            break;
        }

        case 2: // "backup security version" 1
        {
            static bool save60KeyYSetup = false;

            if (!save60KeyYSetup) {
                static const u8 save60KeyY[16] = { 0xC3, 0x69, 0xBA, 0xA2, 0x1E, 0x18, 0x8A, 0x88, 0xA9, 0xAA, 0x94, 0xE5, 0x50, 0x6A, 0x9F, 0x16 };
                setup_aeskeyY(0x2F, save60KeyY);
                save60KeyYSetup = true;
            }

            u8 hash[0x20];
            u8 tmpbuf[0x70];

            u32 in_ncch_offset = hdrs.ncch.offset_exefs * NCCH_MEDIA_UNIT + 0x1E0;
            ReadCartBytes(&hash, ncsd->partitions[0].offset * NCSD_MEDIA_UNIT + in_ncch_offset, sizeof(hash), cdata, false);
            DecryptNcch(hash, in_ncch_offset, sizeof(hash), &hdrs.ncch, NULL);

            memcpy(tmpbuf, hdrs.exthdr.signature, 8);
            memcpy(&tmpbuf[8], cart_unique_id, 0x40);
            memcpy(&tmpbuf[0x48], &hdrs.ncch.programId, 8);
            memcpy(&tmpbuf[0x50], hash, 0x20);
            sha_quick(hash, tmpbuf, 0x70, SHA256_MODE);

            use_aeskey(0x2F);
            aes_cmac(hash, save_key_y, 2);
            savectx.crypto_type = CARD_SAVE_CRYPTO_V2;
            break;
        }
        default:
            savectx.crypto_type = CARD_SAVE_CRYPTO_INVALID;
            return 1;
    };

    u32 cmac_keyslot = savectx.crypto_type == CARD_SAVE_CRYPTO_V1_N3DS ? CARD_SAVE_CMAC_KEYSLOT_N3DS : CARD_SAVE_CMAC_KEYSLOT_O3DS;
    u32 crypto_keyslot = savectx.crypto_type == CARD_SAVE_CRYPTO_V1_N3DS ? CARD_SAVE_CRYPTO_KEYSLOT_N3DS : CARD_SAVE_CRYPTO_KEYSLOT_O3DS;

    setup_aeskeyY(cmac_keyslot, save_key_y); // savedata CMAC key
    setup_aeskeyY(crypto_keyslot, save_key_y); // savedata crypto key
    return 0;
}

int InitCtrCardSave(CartData *cdata) {
    memset(&savectx, 0, sizeof(savectx));

    // the wear leveling header exists twice:
    // the one at 0x0 is the main one
    // the one at 0x1000 is used as a failsafe if the one above is corrupt
    if (InitSaveWearLeveling(cdata, 0) != 0 && InitSaveWearLeveling(cdata, 0x1000) != 0)
        return 1;

    if (InitCtrCardSaveCryptoKey(cdata) != 0)
        return 1;

    return 0;
}

static u32 ReadDecryptedCard1Save(u8 *buffer, u64 offset, u64 count, CartData* cdata) {
    if (offset >= savectx.logical_size) return 1;
    if (offset + count > savectx.logical_size) return 1;

    u32 first_sector = offset / 0x1000;
    u32 last_sector = (offset + count - 1) / 0x1000;
    u32 outbuf_offset = 0;

    SaveBlockmapEntry ent;
    u8 sectorbuf[0x1000];
    u8 ctr[16];
    memset(ctr, 0, sizeof(ctr));

    u32 crypto_keyslot = savectx.crypto_type == CARD_SAVE_CRYPTO_V1_N3DS ? CARD_SAVE_CRYPTO_KEYSLOT_N3DS : CARD_SAVE_CRYPTO_KEYSLOT_O3DS;
    use_aeskey(crypto_keyslot);

    // the minimum one can read from the flash is 4K sectors anyway, and blockmap scatters it, so we do it sector by sector
    for (u32 cur_sector = first_sector; cur_sector < last_sector + 1; cur_sector++) {
        ent = GetBlockmapEntry(cur_sector);

        // where in this sector we need to start reading data from
        u32 in_sector_start = cur_sector == first_sector ? offset % 0x1000 : 0;
        // how much data we can read in this sector assuming the start offset above
        u32 in_sector_size = 0x1000 - in_sector_start;
        // how much data we actually need to read from the sector
        u32 chunksize = min(count, in_sector_size);

        if (!ent.used) {
            memset(&sectorbuf, 0xFF, sizeof(sectorbuf));
        } else {
            if (ReadCartSave(sectorbuf, ent.phys_sec * 0x1000, sizeof(sectorbuf), cdata) != 0)
                return 1;
        }

        if (savectx.repeating_ctr) {
            for (u32 cursect = 0; cursect < 8; cursect++) {
                memset(ctr, 0, sizeof(ctr));
                ctr_decrypt_byte(&sectorbuf[0x200 * cursect], &sectorbuf[0x200 * cursect], 0x200, 0, AES_CNT_CART_SAVE_MODE, ctr);
            }
        } else {
            ctr_decrypt_byte(sectorbuf, sectorbuf, sizeof(sectorbuf), cur_sector * 0x1000, AES_CNT_CART_SAVE_MODE, ctr);
        }

        memcpy(&buffer[outbuf_offset], &sectorbuf[in_sector_start], chunksize);
        outbuf_offset += chunksize;
        count -= chunksize;
        offset += chunksize;
    }

    return 0;
}

static u32 ReadDecryptedCard2Save(u8 *buffer, u64 offset, u64 count, CartData* cdata) {
    if (offset >= cdata->save_size) return 1;
    if (offset + count > cdata->save_size) count = cdata->save_size - offset;

    u32 first_sector = offset / 0x200;
    u32 outbuf_offset = 0;

    u8 ctr[16];
    memset(ctr, 0, sizeof(ctr));

    u32 crypto_keyslot = savectx.crypto_type == CARD_SAVE_CRYPTO_V1_N3DS ? CARD_SAVE_CRYPTO_KEYSLOT_N3DS : CARD_SAVE_CRYPTO_KEYSLOT_O3DS;
    use_aeskey(crypto_keyslot);

    u8 sector_tmp[0x200];

    // handle misalignment at the start
    u32 in_first_sector_offset = offset % 0x200;
    if (in_first_sector_offset) {
        u32 sector_remain = 0x200 - in_first_sector_offset;
        u32 misalignsize = min(sector_remain, count);

        if (ReadCartSave(sector_tmp, first_sector * 0x200, sizeof(sector_tmp), cdata) != 0)
            return 1;

        ctr_decrypt_byte(sector_tmp, sector_tmp, sizeof(sector_tmp), first_sector * 0x200, AES_CNT_CART_SAVE_MODE, ctr);
        memcpy(buffer, &sector_tmp[in_first_sector_offset], misalignsize);
        outbuf_offset += misalignsize;
        offset += misalignsize;
        count -= misalignsize;
    }

    // offset is now aligned, size may still not be, though
    u32 aligned_size = count & ~(0x200-1);
    if (aligned_size) {
        if (ReadCartSave(&buffer[outbuf_offset], offset, aligned_size, cdata) != 0)
            return 1;

        ctr_decrypt_byte(&buffer[outbuf_offset], &buffer[outbuf_offset], aligned_size, offset, AES_CNT_CART_SAVE_MODE, ctr);

        count -= aligned_size;
        offset += aligned_size;
        outbuf_offset += aligned_size;
    }

    // only ending misalignment remains, if applicable
    if (count) {
        if (ReadCartSave(sector_tmp, offset, sizeof(sector_tmp), cdata) != 0)
            return 1;

        ctr_decrypt_byte(sector_tmp, sector_tmp, sizeof(sector_tmp), offset, AES_CNT_CART_SAVE_MODE, ctr);

        memcpy(&buffer[outbuf_offset], &sector_tmp, count);
    }

    return 0;
}

u32 ReadDecryptedCtrCardSave(u8* buffer, u64 offset, u64 count, CartData* cdata) {
    switch (cdata->save_type) {
        case CARD_SAVE_SPI:
            return ReadDecryptedCard1Save(buffer, offset, count, cdata);
            break;

        case CARD_SAVE_CARD2:
            return ReadDecryptedCard2Save(buffer, offset, count, cdata);
            break;

        default:
            return 1;
    }
}