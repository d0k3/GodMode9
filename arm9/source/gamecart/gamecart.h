#pragma once

#include "common.h"
#include "card_spi.h"
#include "ncch.h"
#include "ncsd.h"
#include "nds.h"

#define CART_NONE   0
#define CART_CTR    (1<<0)
#define CART_NTR    (1<<1)
#define CART_TWL    (1<<2)

#define MODC_AREA_SIZE          0x4000
#define PRIV_HDR_SIZE           0x50

typedef enum CardSaveType {
    CARD_SAVE_NONE,
    CARD_SAVE_SPI,
    CARD_SAVE_CARD2,
    CARD_SAVE_RETAIL_NAND,
} CardSaveType;

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

typedef enum CardSaveWearLevelingType {
    CARD_SAVE_WEAR_LEVELING_V1   = 10,
    CARD_SAVE_WEAR_LEVELING_V2   =  2,

    CARD_SAVE_WEAR_LEVELING_NONE = -1,
} CardSaveWearLevelingType;

typedef struct {
    u8  header[0x8000]; // NTR header + secure area / CTR header + private header
    u8  storage[0x8000]; // encrypted secure area + modcrypt area / unused
    u32 cart_type;
    u32 cart_id;
    u32 cart_id2; // crypto type, some special dev stuff, normally all-0 for retail
    u64 cart_size;
    u64 data_size;
    u32 save_size;
    CardSaveType save_type;
    CardSPIType spi_save_type; // Specific data for SPI save 
    u32 arm9i_rom_offset; // TWL specific
} PACKED_ALIGN(16) CartData;

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

typedef struct CartWearLevelingData {
    /* min. 87, max 118 journal entries */
    SaveJournalEntry journal[118];
    /* max case is header + 127 v1 blockmap entries + crc16 */
    u8 blockmap[sizeof(SaveBlockmapHeader) + 127 * sizeof(SaveBlockmapEntryV1) + 2];
    u32 blockmap_size;
    u32 num_journal_entries;
    CardSaveWearLevelingType type;
    u32 logical_sectors;
    u32 logical_size;
    bool initialized;
    u8 unused[0x28];
} CartWearLevelingData;

typedef struct CartDataCtr {
    NcsdHeader ncsd;
    u32 card2_offset;
    u8  cinfo0[0x312 - (0x200 + sizeof(u32))];
    u32 rom_version;
    u8  cinfo1[0x1000 - (0x312 + sizeof(u32))];
    NcchHeader ncch;
    u8  padding[0x3000 - 0x200];
    u8  private[PRIV_HDR_SIZE];
    CartWearLevelingData wear_leveling;
    u8  unused[0x4000 + 0x8000 - PRIV_HDR_SIZE - sizeof(CartWearLevelingData)]; // 0xFF
    u32 cart_type;
    u32 cart_id;
    u32 cart_id2;
    u64 cart_size;
    u64 data_size;
    u32 save_size;
    CardSPIType save_type;
    struct {
        u32 save_crypto_type : 31;
        u32 save_crypto_repeating_ctr : 1;
    };
    u32 pad;
} PACKED_ALIGN(16) CartDataCtr;

typedef struct {
    TwlHeader ntr_header;
    u8 ntr_padding[0x3000]; // 0x00
    u8 secure_area[0x4000];
    u8 secure_area_enc[0x4000];
    u8 modcrypt_area[0x4000];
    u32 cart_type;
    u32 cart_id;
    u32 cart_id2; // meaningless on TWL
    u64 cart_size;
    u64 data_size;
    u32 save_size;
    CardSPIType save_type;
    u32 arm9i_rom_offset;
} PACKED_ALIGN(16) CartDataNtrTwl;

STATIC_ASSERT(sizeof(CartData) == sizeof(CartDataCtr) && sizeof(CartData) == sizeof(CartDataNtrTwl));

u32 GetCartName(char* name, CartData* cdata);
u32 GetCartInfoString(char* info, size_t info_size, CartData* cdata);
u32 SetSecureAreaEncryption(bool encrypted);
u32 InitCartRead(CartData* cdata);
u32 ReadCartSectors(void* buffer, u32 sector, u32 count, CartData* cdata, bool card2_blanking);
u32 ReadCartBytes(void* buffer, u64 offset, u64 count, CartData* cdata, bool card2_blanking);
u32 ReadCartPrivateHeader(void* buffer, u64 offset, u64 count, CartData* cdata);
u32 ReadCartInfo(u8* buffer, u64 offset, u64 count, CartData* cdata);
u32 ReadCartSave(u8* buffer, u64 offset, u64 count, CartData* cdata);
u32 ReadDecryptedCartSave(u8* buffer, u64 offset, u64 count, CartData* cdata);
u32 WriteCartSave(const u8* buffer, u64 offset, u64 count, CartData* cdata);
