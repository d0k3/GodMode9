#include "gamecart.h"
#include "aes.h"
#include "crc16.h"
#include "fsdrive.h"
#include "fsinit.h"
#include "image.h"
#include "protocol.h"
#include "command_ctr.h"
#include "command_ntr.h"
#include "card_spi.h"
#include "nds.h"
#include "ncch.h"
#include "ncsd.h"
#include "rtc.h"
#include "sha.h"
#include "unittype.h"

#define CART_INSERTED (!(REG_CARDSTATUS & 0x1))

static DsTime init_time;
static bool encrypted_sa = false;

u32 GetCartName(char* name, CartData* cdata) {
    if (cdata->cart_type & CART_CTR) {
        CartDataCtr* cdata_i = (CartDataCtr*)cdata;
        NcsdHeader* ncsd = &(cdata_i->ncsd);
        snprintf(name, 24, "%016llX_v%02lu", ncsd->mediaId, cdata_i->rom_version);
    }  else if (cdata->cart_type & CART_NTR) {
        CartDataNtrTwl* cdata_i = (CartDataNtrTwl*)cdata;
        TwlHeader* nds = &(cdata_i->ntr_header);
        snprintf(name, 24, "%.12s_%.6s_%02u", nds->game_title, nds->game_code, nds->rom_version);
    } else return 1;
    for (char* c = name; *c != '\0'; c++)
        if ((*c == ':') || (*c == '*') || (*c == '?') || (*c == '/') || (*c == '\\') || (*c == ' ')) *c = '_';
    return 0;
}

u32 GetCartInfoString(char* info, size_t info_size, CartData* cdata) {
    size_t info_index = 0;
    u8 padding;

    // read the last byte of the cart storage, but ignore the result
    ReadCartBytes(&padding, cdata->cart_size - 1, 1, cdata, false);

    if (cdata->cart_type & CART_CTR) {
        CartDataCtr* cdata_i = (CartDataCtr*)cdata;
        NcsdHeader* ncsd = &(cdata_i->ncsd);
        NcchHeader* ncch = &(cdata_i->ncch);
        info_index += snprintf(info + info_index, info_size - info_index,
            "Title ID     : %016llX\n"
            "Product Code : %.10s\n"
            "Revision     : %lu\n"
            "Cart ID      : %08lX\n"
            "Cart ID2     : %08lX\n"
            "Platform     : %s\n",
            ncsd->mediaId, ncch->productcode, cdata_i->rom_version, cdata_i->cart_id, cdata_i->cart_id2,
            (ncch->flags[4] == 0x2) ? "N3DS" : "O3DS");
    }  else if (cdata->cart_type & CART_NTR) {
        CartDataNtrTwl* cdata_i = (CartDataNtrTwl*)cdata;
        TwlHeader* nds = &(cdata_i->ntr_header);
        info_index += snprintf(info + info_index, info_size - info_index,
            "Title String : %.12s\n"
            "Product Code : %.6s\n"
            "Revision     : %u\n"
            "Cart ID      : %08lX\n"
            "Platform     : %s\n",
            nds->game_title, nds->game_code, nds->rom_version, cdata_i->cart_id,
            (nds->unit_code == 0x2) ? "DSi Enhanced" : (nds->unit_code == 0x3) ? "DSi Exclusive" : "DS");
    } else return 1;

    info_index += snprintf(info + info_index, info_size - info_index,
        "Save Type    : %s\n",
        (cdata->save_type == CARD_SAVE_NONE) ? "NONE" :
        (cdata->save_type == CARD_SAVE_SPI) ? "SPI" :
        (cdata->save_type == CARD_SAVE_CARD2) ? "CARD2" :
        (cdata->save_type == CARD_SAVE_RETAIL_NAND) ? "RETAIL_NAND" : "UNK");

    if (cdata->save_type == CARD_SAVE_SPI) {
        u32 jedecid = 0;
        if (CardSPIReadJEDECIDAndStatusReg(cdata->spi_save_type.infrared, &jedecid, NULL) == 0) {
            info_index += snprintf(info + info_index, info_size - info_index,
                "Save chip ID : %06lX\n",
                jedecid);
        }
    } else info_index += snprintf(info + info_index, info_size - info_index,
        "Save chip ID : <none>\n");

    info_index += snprintf(info + info_index, info_size - info_index,
        "Padding Byte : %02X\n"
        "Timestamp    : 20%02X-%02X-%02X %02X:%02X:%02X\n"
        "GM9 Version  : %s\n",
        padding,
        init_time.bcd_Y, init_time.bcd_M, init_time.bcd_D,
        init_time.bcd_h, init_time.bcd_m, init_time.bcd_s,
        VERSION);
    return 0;
}

u32 SetSecureAreaEncryption(bool encrypted) {
    encrypted_sa = encrypted;
    return 0;
}

static u32 GetCtrCartSaveSize(CartData* cdata) {
    NcsdHeader* ncsd = (NcsdHeader*) (void*) cdata->header;
    u32 ncch_sector = ncsd->partitions[0].offset;

    // Load header and ExHeader for first partition
    u8 buffer[0x400];
    CTR_CmdReadData(ncch_sector, 0x200, 2, buffer);
    NcchHeader* ncch = (NcchHeader*) (void*) buffer;
    if (ValidateNcchHeader(ncch) != 0) {
        return 0;
    }

    // Ensure first partition has ExHeader
    if (ncch->size_exthdr < 0x200) {
        return 0;
    }

    // Decrypt ExHeader
    if ((NCCH_ENCRYPTED(ncch)) && (SetupNcchCrypto(ncch, NCCH_NOCRYPTO) == 0)) {
        DecryptNcch(buffer + NCCH_EXTHDR_OFFSET, NCCH_EXTHDR_OFFSET, sizeof(buffer) - NCCH_EXTHDR_OFFSET, ncch, NULL);
    }
    u64 savesize = getle64(buffer + NCCH_EXTHDR_OFFSET + 0x1C0);

    // check our work
    if (savesize <= UINT32_MAX) {
        return (u32) savesize;
    } else {
        return 0;
    }
}

void SetBlockmapEntry(u32 index, SaveBlockmapEntry *in_entry, CartWearLevelingData *wldata) {
    u8 *base_ptr = &wldata->blockmap[wldata->type == CARD_SAVE_WEAR_LEVELING_V2 ? 0 : sizeof(SaveBlockmapHeader)];
    if (wldata->type == CARD_SAVE_WEAR_LEVELING_V2) {
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

SaveBlockmapEntry GetBlockmapEntry(u32 index, CartWearLevelingData *wldata) {
    u8 *base_ptr = &wldata->blockmap[wldata->type == CARD_SAVE_WEAR_LEVELING_V2 ? 0 : sizeof(SaveBlockmapHeader)];
    if (wldata->type == CARD_SAVE_WEAR_LEVELING_V2) {
        SaveBlockmapEntryV2 *entries = (SaveBlockmapEntryV2 *)base_ptr;
        return (SaveBlockmapEntry) { .checksums = { 0 }, .phys_sec = entries[index].phys_sec, .remap_count = entries[index].remap_count, .used = entries[index].used };
    } else {
        SaveBlockmapEntryV1 *entries = (SaveBlockmapEntryV1 *)base_ptr;
        SaveBlockmapEntry out = (SaveBlockmapEntry) { .phys_sec = entries[index].phys_sec, .remap_count = entries[index].remap_count, .used = entries[index].used };
        memcpy(out.checksums, entries[index].checksums, sizeof(out.checksums));
        return out;
    }
}

void ApplyJournalEntryToBlockmap(SaveJournalEntry *entry, CartWearLevelingData *wldata) {
    SaveBlockmapEntry ent = {
        .used = 1,
        .phys_sec = entry->main.dst_phys_sec,
        .remap_count = entry->main.new_dst_remap_count
    };
    memcpy(ent.checksums, entry->main.new_dst_checksums, sizeof(ent.checksums));
    
    // remap the virtual sector to the new physical sector
    SetBlockmapEntry(entry->main.src_virt_sec, &ent, wldata);
    
    // unless an actual remap didn't occur, we must mark (the physical sector
    // whose virtual sector number we used for remapping this virtual sector to)
    // as free and ready to use for another remap
    if (entry->main.dst_phys_sec != entry->main.src_phys_sec) {
        ent.used = 0;
        ent.phys_sec = entry->main.src_phys_sec;
        ent.remap_count = entry->main.src_remap_count;
        memset(ent.checksums, 0, sizeof(ent.checksums));
        SetBlockmapEntry(entry->main.dst_virt_sec, &ent, wldata);
    }
}

void InitCtrCardSaveCryptoKey(NcsdHeader *ncsd, CartData *cdata) {
    CartDataCtr *ctr_cdata = (CartDataCtr *)cdata;
    
    // save data crypto (if supported)
    u32 save_media_old = ncsd->partition_flags[7];
    u32 save_media_new = ncsd->partition_flags[3];
    u8 save_crypto_keysel_base = ncsd->partition_flags[1];
    u8 save_crypto_keysel_extra = ncsd->extra_save_keysel;
    s32 save_crypto_keysel = 0;
    bool is_card2 = cdata->cart_id & 0x8000000;
    
    bool supported_save_crypto =
        (save_media_new == 0 && save_media_old == 0) || /* SPI save flash (very old carts) */
        (save_media_new == 1) || /* newer(?) SPI flash */
        (save_media_new == 0 && save_media_old == 1) || /* also newer(?) SPI flash */
        is_card2 /* CARD2 */;
    
    // skip everything if there isn't save data or its crypto is not supported
    if (cdata->save_type == CARD_SAVE_NONE || !supported_save_crypto) {
        ctr_cdata->save_crypto_type = CARD_SAVE_CRYPTO_INVALID;
        return;
    }
    
    if (save_media_old == 0 && save_media_new == 0 && !is_card2)
        ctr_cdata->save_crypto_repeating_ctr = true;

    if (is_card2)
        save_crypto_keysel = save_crypto_keysel_base + save_crypto_keysel_extra;
    else if (save_media_new != 0)
        save_crypto_keysel = 0;
    else if (save_media_old != 0)
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
            ctr_cdata->save_crypto_type = CARD_SAVE_CRYPTO_V0;
            break;

        case 1: // "backup security version" 0
        case 11: // "backup security version" 10
        {
            // this type is not supported on O3DS
            if (IS_O3DS && save_crypto_keysel == 11) {
                ctr_cdata->save_crypto_type = CARD_SAVE_CRYPTO_INVALID;
                return;
            }

            u8 tmpbuf[0x48];
            u8 hash[0x20];
            memcpy(tmpbuf, hdrs.exthdr.signature, 8);
            memcpy(&tmpbuf[8], cart_unique_id, 0x40);

            sha_quick(hash, tmpbuf, 0x48, SHA256_MODE);
            memcpy(save_key_y, hash, 16);
            ctr_cdata->save_crypto_type = save_crypto_keysel == 11 ? CARD_SAVE_CRYPTO_V1_N3DS : CARD_SAVE_CRYPTO_V1;
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
            ctr_cdata->save_crypto_type = CARD_SAVE_CRYPTO_V2;
            break;
        }
        default:
            ctr_cdata->save_crypto_type = CARD_SAVE_CRYPTO_INVALID;
            return;
    };
    
    u32 cmac_keyslot = ctr_cdata->save_crypto_type == CARD_SAVE_CRYPTO_V1_N3DS ? CARD_SAVE_CMAC_KEYSLOT_N3DS : CARD_SAVE_CMAC_KEYSLOT_O3DS;
    u32 crypto_keyslot = ctr_cdata->save_crypto_type == CARD_SAVE_CRYPTO_V1_N3DS ? CARD_SAVE_CRYPTO_KEYSLOT_N3DS : CARD_SAVE_CRYPTO_KEYSLOT_O3DS;

    setup_aeskeyY(cmac_keyslot, save_key_y); // savedata CMAC key
    setup_aeskeyY(crypto_keyslot, save_key_y); // savedata crypto key
}

void InitCtrCardSaveWearLeveling(CartData *cdata) {
    CartDataCtr *ctr_cdata = (CartDataCtr *)cdata;
    ctr_cdata->wear_leveling.initialized = false;
    
    // CARD2 does not implement wear leveling for the writable portion of the "ROM"
    if (ctr_cdata->cart_id & 0x8000000) {
        ctr_cdata->wear_leveling.type = CARD_SAVE_WEAR_LEVELING_NONE;
        ctr_cdata->wear_leveling.initialized = true;
        return;
    }
    
    // SPI flash savedata implements wear leveling
    u32 num_physical_sectors = ctr_cdata->save_size / 0x1000;
    ctr_cdata->wear_leveling.logical_sectors = num_physical_sectors - 1;
    ctr_cdata->wear_leveling.logical_size = (num_physical_sectors - 1) * 0x1000;
    
    if (num_physical_sectors > 128) {
        // V2, header + blockmap_entries_v2[511] + crc
        ctr_cdata->wear_leveling.blockmap_size = 0x400; // fixed size
        ctr_cdata->wear_leveling.type = CARD_SAVE_WEAR_LEVELING_V2;
    } else {
        // V1, header + blockmap_entries_v1[n] + crc
        ctr_cdata->wear_leveling.blockmap_size = sizeof(SaveBlockmapHeader) + ctr_cdata->wear_leveling.logical_sectors * sizeof(SaveBlockmapEntryV1) + sizeof(u16);
        ctr_cdata->wear_leveling.type = CARD_SAVE_WEAR_LEVELING_V1;
    }
    
    // TODO: add CMAC verification ability for DISA 
    
    if (ReadCartSave(ctr_cdata->wear_leveling.blockmap, 0, ctr_cdata->wear_leveling.blockmap_size, cdata) != 0)
        return;
    
    u32 journal_size = 0x1000 - ctr_cdata->wear_leveling.blockmap_size;
    ctr_cdata->wear_leveling.num_journal_entries = journal_size / sizeof(SaveJournalEntry);
    
    if (ReadCartSave((u8 *)ctr_cdata->wear_leveling.journal, ctr_cdata->wear_leveling.blockmap_size, journal_size, cdata) != 0)
        return;

    
    u8 *crcLoc = &ctr_cdata->wear_leveling.blockmap[ctr_cdata->wear_leveling.blockmap_size - 2];
    u16 expected_bmap_crc = crcLoc[0] | crcLoc[1] << 8;
    u16 calc_bmap_crc = crc16_quick(ctr_cdata->wear_leveling.blockmap, ctr_cdata->wear_leveling.blockmap_size - 2);
    if (expected_bmap_crc != calc_bmap_crc)
        return;
    
    u32 journal_idx = 0;

    for (; journal_idx < ctr_cdata->wear_leveling.num_journal_entries; journal_idx++) {
        SaveJournalEntry *ent = &ctr_cdata->wear_leveling.journal[journal_idx];
        
        // journal entry should point to a valid physical sector
        if (ent->main.dst_phys_sec) {
            // reached the end of the journal
            if (ent->main.src_virt_sec >= ctr_cdata->wear_leveling.logical_sectors)
                break;
            
            ApplyJournalEntryToBlockmap(ent, &ctr_cdata->wear_leveling);
        }
    }
    
    static const u8 blank[sizeof(SaveJournalEntry)] = { 0xFF };
    u32 num_bad_journal_entries = 0;
    for (; journal_idx < ctr_cdata->wear_leveling.num_journal_entries; journal_idx++) {
        num_bad_journal_entries += memcmp((u8 *)&ctr_cdata->wear_leveling.journal[journal_idx], blank, sizeof(SaveJournalEntry)) == 0;
    }
    
    if (num_bad_journal_entries)
        return;
    
    ctr_cdata->wear_leveling.initialized = true;
}

u32 InitCartRead(CartData* cdata) {
    get_dstime(&init_time);
    encrypted_sa = false;
    memset(cdata, 0x00, sizeof(CartData));
    cdata->cart_type = CART_NONE;
    if (!CART_INSERTED) return 1;
    Cart_Init();
    cdata->cart_id = Cart_GetID();
    cdata->cart_type = (cdata->cart_id & 0x10000000) ? CART_CTR : CART_NTR;

    // Use the cart ID to determine the ROM size.
    // (ROM header might be incorrect on dev carts.)
    switch ((cdata->cart_id >> 16) & 0xFF) {
        case 0x07:  cdata->cart_size = 8ULL*1024*1024; break;
        case 0x0F:  cdata->cart_size = 16ULL*1024*1024; break;
        case 0x1F:  cdata->cart_size = 32ULL*1024*1024; break;
        case 0x3F:  cdata->cart_size = 64ULL*1024*1024; break;
        case 0x7F:  cdata->cart_size = 128ULL*1024*1024; break;
        case 0xFF:  cdata->cart_size = 256ULL*1024*1024; break;
        case 0xFE:  cdata->cart_size = 512ULL*1024*1024; break;
        case 0xFA:  cdata->cart_size = 1024ULL*1024*1024; break;
        case 0xF8:  cdata->cart_size = 2048ULL*1024*1024; break;
        case 0xF0:  cdata->cart_size = 4096ULL*1024*1024; break;
        default:    cdata->cart_size = 0; break;
    }

    if (cdata->cart_type & CART_CTR) { // CTR cartridges
        memset(cdata, 0xFF, 0x4000 + PRIV_HDR_SIZE); // switch the padding to 0xFF

        // init, NCCH header
        static u32 sec_keys[4];
        u8* ncch_header = cdata->header + 0x1000;
        cdata->cart_id2 = Cart_GetID2();
        CTR_CmdReadHeader(ncch_header);
        Cart_Secure_Init((u32*) (void*) ncch_header, sec_keys);

        // NCSD header and CINFO
        // Cart_Dummy();
        // Cart_Dummy();
        CTR_CmdReadData(0, 0x200, 8, cdata->header);

        // safety checks, cart size
        NcsdHeader* ncsd = (NcsdHeader*) (void*) cdata->header;
        NcchHeader* ncch = (NcchHeader*) (void*) ncch_header;
        if ((ValidateNcsdHeader(ncsd) != 0) || (ValidateNcchHeader(ncch) != 0))
            return 1;
        if (cdata->cart_size == 0)
            cdata->cart_size = (u64) ncsd->size * NCSD_MEDIA_UNIT;
        cdata->data_size = GetNcsdTrimmedSize(ncsd);
        if (cdata->cart_size > 0x100000000) return 1; // carts > 4GB don't exist
        // else if (cdata->cart_size == 0x100000000) cdata->cart_size -= 0x200; // silent 4GB fix
        if (cdata->data_size > cdata->cart_size) return 1;

        // private header
        u8* priv_header = cdata->header + 0x4000;
        CTR_CmdReadUniqueID(priv_header);
        memcpy(priv_header + 0x40, &(cdata->cart_id), 4);
        memcpy(priv_header + 0x44, &(cdata->cart_id2), 4);
        memset(priv_header + 0x48, 0xFF, 8);

        bool is_card2 = cdata->cart_id & 0x8000000;
        u32 card2_offset = getle32(cdata->header + 0x200);
        
        if (is_card2 && card2_offset != 0xFFFFFFFF) {
            cdata->save_type = CARD_SAVE_CARD2;
            cdata->save_size = GetCtrCartSaveSize(cdata);
            // Sanity checks
            if ((cdata->save_size == 0) ||
                (card2_offset * NCSD_MEDIA_UNIT >= cdata->cart_size) ||
                (card2_offset * NCSD_MEDIA_UNIT + cdata->save_size > cdata->cart_size)) {
                cdata->save_type = CARD_SAVE_NONE;
            }
        } else {
            cdata->spi_save_type = (CardSPIType) { FLASH_CTR_GENERIC, false };
            cdata->save_size = CardSPIGetCapacity(cdata->spi_save_type);
            if (cdata->save_size == 0) {
                cdata->spi_save_type = (CardSPIType) { NO_CHIP, false };
            }
            if (cdata->spi_save_type.chip == NO_CHIP) {
                cdata->save_type = CARD_SAVE_NONE;
            } else {
                cdata->save_type = CARD_SAVE_SPI;
                cdata->save_size = CardSPIGetCapacity(cdata->spi_save_type);
            }
        }
        
        // for compatibility purposes save crypto and wear leveling init are optional
        
        // all CTRCARDs use save crypto
        InitCtrCardSaveCryptoKey(ncsd, cdata);
        
        // all CTRCARDs except CARD2 use wear leveling for save data
        InitCtrCardSaveWearLeveling(cdata);

    } else { // NTR/TWL cartridges
        // NTR header
        TwlHeader* nds_header = (void*)cdata->header;
        u8 secure_area_enc[0x4000];
        NTR_CmdReadHeader(cdata->header);
        if (!(*(cdata->header))) return 1; // error reading the header
        if (!NTR_Secure_Init(cdata->header, secure_area_enc, Cart_GetID(), 0)) return 1;


        // cartridge size, trimmed size, twl presets
        if (nds_header->device_capacity >= 15) return 1; // too big, not valid
        if (cdata->cart_size == 0)
            cdata->cart_size = (128 * 1024) << nds_header->device_capacity;
        cdata->data_size = nds_header->ntr_rom_size;
        cdata->arm9i_rom_offset = 0;

        // TWL header
        if (nds_header->unit_code != 0x00) { // DSi or NDS+DSi
            cdata->cart_type |= CART_TWL;
            cdata->data_size = nds_header->ntr_twl_rom_size;
            cdata->arm9i_rom_offset = nds_header->arm9i_rom_offset;
            if ((cdata->arm9i_rom_offset < nds_header->ntr_rom_size) ||
                (cdata->arm9i_rom_offset + MODC_AREA_SIZE > cdata->data_size))
                return 1; // safety first

            // Some NTR dev carts have TWL ROMs flashed to them.
            // We'll only want to use TWL secure init if this is a TWL cartridge.
            if (cdata->cart_id & 0x40000000U) { // TWL cartridge
                Cart_Init();
                NTR_CmdReadHeader(cdata->storage);
                if (!NTR_Secure_Init(cdata->storage, NULL, Cart_GetID(), 1)) return 1;
            }
        } else {
            // Check if immediately after the reported cart size
            // is the magic number string 'ac' (auth code).
            // If found, add 0x88 bytes for the download play RSA key.
            u16 rsaMagic;
            ReadCartBytes(&rsaMagic, cdata->data_size, 2, cdata, false);
            if(rsaMagic == 0x6361) {
                cdata->data_size += 0x88;
            }
        }

        // store encrypted secure area
        memcpy(cdata->storage, secure_area_enc, 0x4000);

        // last safety check
        if (cdata->data_size > cdata->cart_size) return 1;

        // save data
        bool infrared = *(nds_header->game_code) == 'I';
        cdata->spi_save_type = CardSPIGetCardSPIType(infrared);
        if (cdata->spi_save_type.chip == NO_CHIP) {
            cdata->save_type = CARD_SAVE_NONE;
        } else {
            cdata->save_type = CARD_SAVE_SPI;
            cdata->save_size = CardSPIGetCapacity(cdata->spi_save_type);
        }
    }
    return 0;
}

u32 ReadCartSectors(void* buffer, u32 sector, u32 count, CartData* cdata, bool card2_blanking) {
    u8* buffer8 = (u8*) buffer;
    if (!CART_INSERTED) return 1;
    // header
    const u32 header_sectors = 0x4000/0x200;
    if (sector < header_sectors) {
        u32 header_count = (sector + count > header_sectors) ? header_sectors - sector : count;
        memcpy(buffer8, cdata->header + (sector * 0x200), header_count * 0x200);
        buffer8 += header_count * 0x200;
        sector += header_count;
        count -= header_count;
    }
    if (!count) return 0;
    // actual cart reads
    if (cdata->cart_type & CART_CTR) {
        // don't read more than 1MB at once
        const u32 max_read = 0x800;
        u8* buff = buffer8;
        for (u32 i = 0; i < count; i += max_read) {
            // Cart_Dummy();
            // Cart_Dummy();
            CTR_CmdReadData(sector + i, 0x200, min(max_read, count - i), buff);
            buff += max_read * 0x200;
        }

        // overwrite the card2 savegame with 0xFF
        u32 card2_offset = getle32(cdata->header + 0x200);
        u32 save_sectors = cdata->save_size / 0x200;
        if (card2_blanking &&
            (cdata->save_type == CARD_SAVE_CARD2) &&
            ((card2_offset * 0x200) >= cdata->data_size) &&
            (sector + count > card2_offset) && // requested area ends after the save starts
            (sector < card2_offset + save_sectors)) { // requested area starts before the save ends
            u32 blank_start_sector, blank_end_sector;
            if (sector > card2_offset) {
                blank_start_sector = sector;
            } else {
                blank_start_sector = card2_offset;
            }
            if (sector + count < card2_offset + save_sectors) {
                blank_end_sector = sector + count;
            } else {
                blank_end_sector = card2_offset + save_sectors;
            }

            memset(buffer8 + (blank_start_sector - sector) * 0x200, 0xFF,
                (blank_end_sector - blank_start_sector) * 0x200);
        }
    } else if (cdata->cart_type & CART_NTR) {
        u8* buff = buffer8;

        // secure area handling
        const u32 sa_sector_end = 0x8000/0x200;
        if (sector < sa_sector_end) {
            CartDataNtrTwl* cdata_twl = (CartDataNtrTwl*) cdata;
            u8* sa = encrypted_sa ? cdata_twl->secure_area_enc : cdata_twl->secure_area;
            u32 count_sa = ((sector + count) > sa_sector_end) ?  sa_sector_end - sector : count;
            memcpy(buff, sa + ((sector - header_sectors) * 0x200), count_sa * 0x200);
            buff += count_sa * 0x200;
            sector += count_sa;
            count -= count_sa;
        }

        // regular cart data
        u32 off = sector * 0x200;
        for (u32 i = 0; i < count; i++, off += 0x200, buff += 0x200)
            NTR_CmdReadData(off, buff);

        // modcrypt area handling
        if ((cdata->cart_type & CART_TWL) &&
            ((sector+count) * 0x200 > cdata->arm9i_rom_offset) &&
            (sector * 0x200 < cdata->arm9i_rom_offset + MODC_AREA_SIZE)) {
            u32 arm9i_rom_offset = cdata->arm9i_rom_offset;
            u8* buffer_arm9i = buffer8;
            u32 offset_i = 0;
            u32 size_i = MODC_AREA_SIZE;
            if (arm9i_rom_offset < (sector * 0x200))
                offset_i = (sector * 0x200) - arm9i_rom_offset;
            else buffer_arm9i = buffer8 + (arm9i_rom_offset - (sector * 0x200));
            size_i = MODC_AREA_SIZE - offset_i;
            if (size_i > (count * 0x200) - (buffer_arm9i - buffer8))
                size_i = (count * 0x200) - (buffer_arm9i - buffer8);
            if (size_i) memcpy(buffer_arm9i, cdata->storage + 0x4000 + offset_i, size_i);
        }
    } else return 1;
    return 0;
}

u32 ReadCartBytes(void* buffer, u64 offset, u64 count, CartData* cdata, bool card2_blanking) {
    if (!(offset % 0x200) && !(count % 0x200)) { // aligned data -> simple case
        // simple wrapper function for ReadCartSectors(...)
        return ReadCartSectors(buffer, offset / 0x200, count / 0x200, cdata, card2_blanking);
    } else { // misaligned data -> -___-
        u8* buffer8 = (u8*) buffer;
        u8 l_buffer[0x200];
        if (offset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (offset % 0x200);
            if (ReadCartSectors(l_buffer, offset / 0x200, 1, cdata, card2_blanking) != 0) return 1;
            memcpy(buffer8, l_buffer + 0x200 - offset_fix, min(offset_fix, count));
            if (count <= offset_fix) return 0;
            offset += offset_fix;
            buffer8 += offset_fix;
            count -= offset_fix;
        } // offset is now aligned and part of the data is read
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            if (ReadCartSectors(buffer8, offset / 0x200, count / 0x200, cdata, card2_blanking) != 0) return 1;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            if (ReadCartSectors(l_buffer, (offset + count) / 0x200, 1, cdata, card2_blanking) != 0) return 1;
            memcpy(buffer8 + count - count_fix, l_buffer, count_fix);
        }
        return 0;
    }
}

u32 ReadCartPrivateHeader(void* buffer, u64 offset, u64 count, CartData* cdata) {
    if (!(cdata->cart_type & CART_CTR)) return 1;
    if (offset < PRIV_HDR_SIZE) {
        u8* priv_hdr = cdata->header + 0x4000;
        if (offset + count > PRIV_HDR_SIZE) count = PRIV_HDR_SIZE - offset;
        memcpy(buffer, priv_hdr + offset, count);
    }
    return 0;
}

u32 ReadCartInfo(u8* buffer, u64 offset, u64 count, CartData* cdata) {
    char info[301];
    u32 len;

    GetCartInfoString(info, sizeof(info), cdata);
    len = strnlen(info, 300);

    if (offset >= len) return 0;
    if (offset + count > len) count = len - offset;
    memcpy(buffer, info + offset, count);

    return 0;
}

u32 ReadCartSave(u8* buffer, u64 offset, u64 count, CartData* cdata) {
    if (offset >= cdata->save_size) return 1;
    if (offset + count > cdata->save_size) count = cdata->save_size - offset;
    switch (cdata->save_type) {
        case CARD_SAVE_SPI:
            return (CardSPIReadSaveData(cdata->spi_save_type, offset, buffer, count) == 0) ? 0 : 1;
            break;

        case CARD_SAVE_CARD2:
        {
            u32 card2_offset = getle32(cdata->header + 0x200);
            return ReadCartBytes(buffer, card2_offset * NCSD_MEDIA_UNIT + offset, count, cdata, false);
            break;
        }

        default:
            return 1;
            break;
    }
}

static u32 ReadDecryptedCard1Save(u8 *buffer, u64 offset, u64 count, CartData* cdata) {
    CartDataCtr *ctr_cdata = (CartDataCtr *)cdata;

    if (offset >= ctr_cdata->wear_leveling.logical_size) return 1;
    if (offset + count > ctr_cdata->wear_leveling.logical_size) return 1;

    u32 first_sector = offset / 0x1000;
    u32 last_sector = (offset + count - 1) / 0x1000;
    u32 outbuf_offset = 0;
    
    SaveBlockmapEntry ent;
    u8 sectorbuf[0x1000];
    u8 ctr[16];
    memset(ctr, 0, sizeof(ctr));

    u32 crypto_keyslot = ctr_cdata->save_crypto_type == CARD_SAVE_CRYPTO_V1_N3DS ? CARD_SAVE_CRYPTO_KEYSLOT_N3DS : CARD_SAVE_CRYPTO_KEYSLOT_O3DS;
    use_aeskey(crypto_keyslot);
    
    // the minimum one can read from the flash is 4K sectors anyway, and blockmap scatters it, so we do it sector by sector
    for (u32 cur_sector = first_sector; cur_sector < last_sector + 1; cur_sector++) {
        ent = GetBlockmapEntry(cur_sector, &ctr_cdata->wear_leveling);
        
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
        
        if (ctr_cdata->save_crypto_repeating_ctr) {
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
    
    CartDataCtr *ctr_cdata = (CartDataCtr *)cdata;

    u32 first_sector = offset / 0x200;
    u32 outbuf_offset = 0;
    
    u8 ctr[16];
    memset(ctr, 0, sizeof(ctr));

    u32 crypto_keyslot = ctr_cdata->save_crypto_type == CARD_SAVE_CRYPTO_V1_N3DS ? CARD_SAVE_CRYPTO_KEYSLOT_N3DS : CARD_SAVE_CRYPTO_KEYSLOT_O3DS;
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

u32 ReadDecryptedCartSave(u8* buffer, u64 offset, u64 count, CartData* cdata) {
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

u32 WriteCartSave(const u8* buffer, u64 offset, u64 count, CartData* cdata) {
    if (offset >= cdata->save_size) return 1;
    if (offset + count > cdata->save_size) count = cdata->save_size - offset;
    int res = CardSPIWriteSaveData(cdata->spi_save_type, offset, buffer, count);
    if (cdata->cart_type & CART_CTR) {
        CartDataCtr *ctr_cdata = (CartDataCtr *)cdata;
        if (ctr_cdata->wear_leveling.type != CARD_SAVE_WEAR_LEVELING_NONE) {
            InitCtrCardSaveWearLeveling(cdata); // reload wear-leveling data, might've been invalidated by user
            if (*GetMountPath() && !ctr_cdata->wear_leveling.initialized && (DriveType(GetMountPath()) & DRV_CART) && strstr(GetMountPath(), ".sav")) {
                // unmount the virtual DISA archive if user invalidated the encrypted and wear-leveled source
                InitImgFS(NULL);
            }
        }
    }
    return res == 0 ? 0 : 1;
}
