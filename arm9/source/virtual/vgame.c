#include "vgame.h"
#include "image.h"
#include "game.h"
#include "utf.h"
#include "aes.h"

#define VGAME_BUFFER_SIZE   0x200000 // at least 2MB, multiple of 0x200

#define VFLAG_NO_CRYPTO     (1UL<<18)
#define VFLAG_TAD           (1UL<<19)
#define VFLAG_CIA_CONTENT   (1UL<<20)
#define VFLAG_NDS           (1UL<<21)
#define VFLAG_NITRO_DIR     (1UL<<22)
#define VFLAG_NITRO         (1UL<<23)
#define VFLAG_FIRM          (1UL<<24)
#define VFLAG_EXEFS_FILE    (1UL<<25)
#define VFLAG_EXTHDR        (1UL<<26)
#define VFLAG_CIA           (1UL<<27)
#define VFLAG_NCSD          (1UL<<28)
#define VFLAG_NCCH          (1UL<<29)
#define VFLAG_EXEFS         (1UL<<30)
#define VFLAG_ROMFS         (1UL<<31)
#define VFLAG_GAMEDIR       (VFLAG_FIRM|VFLAG_CIA|VFLAG_NCSD|VFLAG_NCCH|VFLAG_EXEFS|VFLAG_ROMFS|VFLAG_LV3|VFLAG_NDS|VFLAG_NITRO_DIR|VFLAG_NITRO|VFLAG_TAD)
#define VFLAG_NCCH_CRYPTO   (VFLAG_EXEFS_FILE|VFLAG_EXTHDR|VFLAG_EXEFS|VFLAG_ROMFS|VFLAG_LV3|VFLAG_NCCH)

#define NAME_FIRM_HEADER    "header.bin"
#define NAME_FIRM_ARM9BIN   "arm9dec.bin"
#define NAME_FIRM_SECTION   "section%lu.%08lX.%s.bin" // number.address.arm9/.arm11
#define NAME_FIRM_NCCH      "%016llX.%.8s%s" // name.title_id(.ext)

#define NAME_CIA_HEADER     "header.bin"
#define NAME_CIA_CERT       "cert.bin"
#define NAME_CIA_TICKET     "ticket.bin"
#define NAME_CIA_TMD        "tmd.bin"
#define NAME_CIA_TMDCHUNK   "tmdchunks.bin"
#define NAME_CIA_META       "meta.bin"
#define NAME_CIA_ICON       "icon.bin"
#define NAME_CIA_CONTENT    "%04X.%08lX%s" // index.id(.ext)

#define NAME_NCSD_HEADER    "ncsd.bin"
#define NAME_NCSD_CARDINFO  "cardinfo.bin"
#define NAME_NCSD_DEVINFO   "devinfo.bin"
#define NAME_NCSD_TYPES     "game", "manual", "dlp", \
                            "unk", "unk", "unk", \
                            "update_n3ds", "update_o3ds"
#define NAME_NCSD_CONTENT   "content%lu.%s%s" // content?.type(.ext)

#define NAME_NCCH_HEADER    "ncch.bin"
#define NAME_NCCH_EXTHEADER "extheader.bin"
#define NAME_NCCH_PLAIN     "plain.bin"
#define NAME_NCCH_LOGO      "logo.bin"
#define NAME_NCCH_EXEFS     "exefs.bin"
#define NAME_NCCH_ROMFS     "romfs.bin"
#define NAME_NCCH_EXEFSDIR  "exefs"
#define NAME_NCCH_ROMFSDIR  "romfs"

#define NAME_NDS_HEADER     "header.bin"
#define NAME_NDS_ARM9       "arm9.bin"
#define NAME_NDS_ARM9I      "arm9i.bin"
#define NAME_NDS_ARM9OVL    "arm9overlay.bin"
#define NAME_NDS_ARM7       "arm7.bin"
#define NAME_NDS_ARM7I      "arm7i.bin"
#define NAME_NDS_ARM7OVL    "arm7overlay.bin"
#define NAME_NDS_BANNER     "banner.bin"
#define NAME_NDS_DATADIR    "data"

#define NAME_TAD_BANNER     "banner.bin"
#define NAME_TAD_HEADER     "header.bin"
#define NAME_TAD_FOOTER     "footer.bin"
#define NAME_TAD_TYPES      "tmd", "srl.app", "02.unk", \
                            "03.unk", "04.unk", "05.unk", \
                            "06.unk", "07.unk", "08.unk", \
                            "public.sav", "banner.sav", "11.unk"
#define NAME_TAD_CONTENT    "%016llX.%s" // titleid.type


static u64 vgame_type = 0;
static u32 base_vdir = 0;

static void* vgame_buffer = NULL;

static VirtualFile* templates_cia   = NULL;
static VirtualFile* templates_tad   = NULL;
static VirtualFile* templates_firm  = NULL;
static VirtualFile* templates_ncsd  = NULL;
static VirtualFile* templates_ncch  = NULL;
static VirtualFile* templates_nds   = NULL;
static VirtualFile* templates_exefs = NULL;
static int n_templates_cia   = -1;
static int n_templates_tad   = -1;
static int n_templates_firm  = -1;
static int n_templates_ncsd  = -1;
static int n_templates_ncch  = -1;
static int n_templates_exefs = -1;
static int n_templates_nds   = -1;

static u64 offset_firm  = (u64) -1;
static u64 offset_a9bin = (u64) -1;
static u64 offset_cia   = (u64) -1;
static u64 offset_ncsd  = (u64) -1;
static u64 offset_ncch  = (u64) -1;
static u64 offset_exefs = (u64) -1;
static u64 offset_romfs = (u64) -1;
static u64 offset_lv3   = (u64) -1;
static u64 offset_lv3fd = (u64) -1;
static u64 offset_nds   = (u64) -1;
static u64 offset_nitro = (u64) -1;
static u64 offset_ccnt  = (u64) -1;
static u64 offset_tad   = (u64) -1;
static u32 index_ccnt   = (u32) -1;

static CiaStub* cia       = NULL;
static TwlHeader* twl     = NULL;
static FirmA9LHeader* a9l = NULL;
static FirmHeader* firm   = NULL;
static NcsdHeader* ncsd   = NULL;
static NcchHeader* ncch   = NULL;
static ExeFsHeader* exefs = NULL;
static u8* romfslv3       = NULL;
static u8* nitrofs        = NULL;
static RomFsLv3Index lv3idx;
static u8 cia_titlekey[16];


int ReadCbcImageBlocks(void* buffer, u64 block, u64 count, u8* iv0, u64 block0) {
    int ret = ReadImageBytes(buffer, block * AES_BLOCK_SIZE, count * AES_BLOCK_SIZE);
    if ((ret == 0) && iv0) {
        u8 ctr[AES_BLOCK_SIZE] = { 0 };
        if (block == block0) memcpy(ctr, iv0, AES_BLOCK_SIZE);
        else if ((ret = ReadImageBytes(ctr, (block-1) * AES_BLOCK_SIZE, AES_BLOCK_SIZE)) != 0)
            return ret;
        
        u32 mode = AES_CNT_TITLEKEY_DECRYPT_MODE;
        cbc_decrypt(buffer, buffer, count, mode, ctr);
    }
    return ret;
}

int ReadCbcImageBytes(void* buffer, u64 offset, u64 count, u8* iv0, u64 offset0) {
    u32 off_fix = offset % AES_BLOCK_SIZE;
    u64 block0 = offset0 / AES_BLOCK_SIZE;
    u8 __attribute__((aligned(32))) temp[AES_BLOCK_SIZE];
    u8* buffer8 = (u8*) buffer;
    int ret = 0;
    
    if (off_fix) { // misaligned offset (at beginning)
        u32 fix_byte = ((off_fix + count) >= AES_BLOCK_SIZE) ? AES_BLOCK_SIZE - off_fix : count;
        if ((ret = ReadCbcImageBlocks(temp, offset / AES_BLOCK_SIZE, 1, iv0, block0)) != 0)
            return ret;
        memcpy(buffer8, temp + off_fix, fix_byte);
        buffer8 += fix_byte;
        offset += fix_byte;
        count -= fix_byte;
    }
    
    if (count >= AES_BLOCK_SIZE) {
        u64 blocks = count / AES_BLOCK_SIZE;
        if ((ret = ReadCbcImageBlocks(buffer8, offset / AES_BLOCK_SIZE, blocks, iv0, block0)) != 0)
            return ret;
        buffer8 += AES_BLOCK_SIZE * blocks;
        offset += AES_BLOCK_SIZE * blocks;
        count -= AES_BLOCK_SIZE * blocks;
    }
    
    if (count) { // misaligned offset (at end)
        if ((ret = ReadCbcImageBlocks(temp, offset / AES_BLOCK_SIZE, 1, iv0, block0)) != 0)
            return ret;
        memcpy(buffer8, temp, count);
        count = 0;
    }
    
    return ret;
}

int ReadCiaContentImageBytes(void* buffer, u64 offset, u64 count, u32 cia_cnt_idx, u64 offset0) {
    // setup key for CIA
    u8 tik[16] __attribute__((aligned(32)));
    memcpy(tik, cia_titlekey, 16);
    setup_aeskey(0x11, tik);
    use_aeskey(0x11);
    
    // setup IV0
    u8 iv0[AES_BLOCK_SIZE] = { 0 };
    iv0[0] = (cia_cnt_idx >> 8) & 0xFF;
    iv0[1] = (cia_cnt_idx >> 0) & 0xFF;
    
    // continue in next function
    u8* iv0_ptr = (cia_cnt_idx <= 0xFFFF) ? iv0 : NULL;
    return ReadCbcImageBytes(buffer, offset, count, iv0_ptr, offset0);
}

int ReadGameImageBytes(void* buffer, u64 offset, u64 count) {
    int ret = ((offset_ccnt != (u64) -1) && (index_ccnt <= 0xFFFF)) ?
        ReadCiaContentImageBytes(buffer, offset, count, index_ccnt, offset_ccnt) :
        ReadImageBytes(buffer, offset, count);
    if ((offset_a9bin != (u64) -1) && (DecryptFirm(buffer, offset, count, firm, a9l) != 0))
        return -1;
    return ret;
}

int ReadNcchImageBytes(void* buffer, u64 offset, u64 count) {
    int ret = ReadGameImageBytes(buffer, offset, count);
    if ((offset_ncch != (u64) -1) && NCCH_ENCRYPTED(ncch) && (DecryptNcch(buffer, offset - offset_ncch, count,
        ncch, (offset_exefs == (u64) -1) ? NULL : exefs) != 0)) return -1;
    return ret;
}

bool BuildVGameExeFsDir(void) {
    VirtualFile* templates = templates_exefs;
    u32 n = 0;
    
    for (u32 i = 0; i < 10; i++) {
        ExeFsFileHeader* file = exefs->files + i;
        if (file->size == 0) continue;
        snprintf(templates[n].name, 32, "%.8s", file->name);
        templates[n].offset = offset_exefs + sizeof(ExeFsHeader) + file->offset;
        templates[n].size = file->size;
        templates[n].keyslot = ((offset_ncch != (u64) -1) && NCCH_ENCRYPTED(ncch)) ?
            0x2C : 0xFF; // actual keyslot may be different
        templates[n].flags = VFLAG_EXEFS_FILE;
        n++;
    }
    
    n_templates_exefs = n;
    return true;
}

bool BuildVGameNcchDir(void) {
    VirtualFile* templates = templates_ncch;
    u32 n = 0;
    
    // NCCH crypto
    bool ncch_crypto = (NCCH_ENCRYPTED(ncch)) && (SetupNcchCrypto(ncch, NCCH_NOCRYPTO) == 0);
    
    // header
    strncpy(templates[n].name, NAME_NCCH_HEADER, 32);
    templates[n].offset = offset_ncch + 0;
    templates[n].size = 0x200;
    templates[n].keyslot = 0xFF;
    templates[n].flags = VFLAG_NCCH;
    n++;
    
    // extended header
    if (ncch->size_exthdr) {
        strncpy(templates[n].name, NAME_NCCH_EXTHEADER, 32);
        templates[n].offset = offset_ncch + NCCH_EXTHDR_OFFSET;
        templates[n].size = NCCH_EXTHDR_SIZE;
        templates[n].keyslot = ncch_crypto ? 0x2C : 0xFF;
        templates[n].flags = VFLAG_EXTHDR;
        n++;
    }
    
    // plain region
    if (ncch->size_plain) {
        strncpy(templates[n].name, NAME_NCCH_PLAIN, 32);
        templates[n].offset = offset_ncch + (ncch->offset_plain * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_plain * NCCH_MEDIA_UNIT;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NCCH;
        n++;
    }
    
    // logo region
    if (ncch->size_logo) {
        strncpy(templates[n].name, NAME_NCCH_LOGO, 32);
        templates[n].offset = offset_ncch + (ncch->offset_logo * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_logo * NCCH_MEDIA_UNIT;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NCCH;
        n++;
    }
    
    // exefs
    if (ncch->size_exefs) {
        strncpy(templates[n].name, NAME_NCCH_EXEFS, 32);
        templates[n].offset = offset_ncch + (ncch->offset_exefs * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_exefs * NCCH_MEDIA_UNIT;
        templates[n].keyslot = ncch_crypto ? 0x2C : 0xFF; // real slot may be something else
        templates[n].flags = VFLAG_EXEFS;
        n++;
        if (!NCCH_ENCRYPTED(ncch) || ncch_crypto) {
            memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
            strncpy(templates[n].name, NAME_NCCH_EXEFSDIR, 32);
            templates[n].flags |= VFLAG_DIR;
            n++;
        }
    }
    
    // romfs
    if (ncch->size_romfs) {
        strncpy(templates[n].name, NAME_NCCH_ROMFS, 32);
        templates[n].offset = offset_ncch + (ncch->offset_romfs * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_romfs * NCCH_MEDIA_UNIT;
        templates[n].keyslot = ncch_crypto ? 0x2C : 0xFF; // real slot may be something else
        templates[n].flags = VFLAG_ROMFS;
        n++;
        if (!NCCH_ENCRYPTED(ncch) || ncch_crypto) {
            memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
            strncpy(templates[n].name, NAME_NCCH_ROMFSDIR, 32);
            templates[n].flags |= VFLAG_DIR;
            n++;
        }
    }
    
    n_templates_ncch = n;
    return true;
}
    
bool BuildVGameNcsdDir(void) {
    const char* name_type[] = { NAME_NCSD_TYPES };
    VirtualFile* templates = templates_ncsd;
    u32 n = 0;
    
    // header
    strncpy(templates[n].name, NAME_NCSD_HEADER, 32);
    templates[n].offset = 0;
    templates[n].size = 0x200;
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    n++;
    
    // card info header
    if (ncsd->partitions[0].offset * NCSD_MEDIA_UNIT >= NCSD_CINFO_OFFSET + NCSD_CINFO_SIZE) {
        strncpy(templates[n].name, NAME_NCSD_CARDINFO, 32);
        templates[n].offset = NCSD_CINFO_OFFSET;
        templates[n].size = NCSD_CINFO_SIZE;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // dev info header
    if (ncsd->partitions[0].offset * NCSD_MEDIA_UNIT >= NCSD_DINFO_OFFSET + NCSD_DINFO_SIZE) {
        strncpy(templates[n].name, NAME_NCSD_DEVINFO, 32);
        templates[n].offset = NCSD_DINFO_OFFSET;
        templates[n].size = NCSD_DINFO_SIZE;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // contents
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = ncsd->partitions + i;
        if ((partition->offset == 0) && (partition->size == 0))
            continue;
        snprintf(templates[n].name, 32, NAME_NCSD_CONTENT, i, name_type[i], ".app");
        templates[n].offset = partition->offset * NCSD_MEDIA_UNIT;
        templates[n].size = partition->size * NCSD_MEDIA_UNIT;
        templates[n].keyslot = 0xFF; // not encrypted
        templates[n].flags = 0;
        n++;
        memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
        snprintf(templates[n].name, 32, NAME_NCSD_CONTENT, i, name_type[i], "");
        templates[n].flags |= (VFLAG_NCCH | VFLAG_DIR);
        n++;
    }
    
    n_templates_ncsd = n;
    return true;
}

bool BuildVGameCiaDir(void) {
    CiaInfo info;
    VirtualFile* templates = templates_cia;
    u32 n = 0;
    
    if (GetCiaInfo(&info, &(cia->header)) != 0)
        return false;
    
    // header
    strncpy(templates[n].name, NAME_CIA_HEADER, 32);
    templates[n].offset = 0;
    templates[n].size = info.size_header;
    templates[n].keyslot = 0xFF;
    templates[n].flags = VFLAG_NO_CRYPTO;
    n++;
    
    // certificates
    if (info.size_cert) {
        strncpy(templates[n].name, NAME_CIA_CERT, 32);
        templates[n].offset = info.offset_cert;
        templates[n].size = info.size_cert;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NO_CRYPTO;
        n++;
    }
    
    // ticket
    if (info.size_ticket) {
        strncpy(templates[n].name, NAME_CIA_TICKET, 32);
        templates[n].offset = info.offset_ticket;
        templates[n].size = info.size_ticket;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NO_CRYPTO;
        n++;
    }
    
    // TMD (the full thing)
    if (info.size_tmd) {
        strncpy(templates[n].name, NAME_CIA_TMD, 32);
        templates[n].offset = info.offset_tmd;
        templates[n].size = info.size_tmd;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NO_CRYPTO;
        n++;
    }
    
    // TMD content chunks
    if (info.size_content_list) {
        strncpy(templates[n].name, NAME_CIA_TMDCHUNK, 32);
        templates[n].offset = info.offset_content_list;
        templates[n].size = info.size_content_list;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NO_CRYPTO;
        n++;
    }
    
    // meta
    if (info.size_meta) {
        strncpy(templates[n].name, NAME_CIA_META, 32);
        templates[n].offset = info.offset_meta;
        templates[n].size = info.size_meta;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NO_CRYPTO;
        n++;
        strncpy(templates[n].name, NAME_CIA_ICON, 32);
        templates[n].offset = info.offset_meta + 0x400;
        templates[n].size = info.size_meta - 0x400;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NO_CRYPTO;
        n++;
    }
    
    // contents
    if (info.size_content) {
        TmdContentChunk* content_list = cia->content_list;
        u32 content_count = getbe16(cia->tmd.content_count);
        u64 next_offset = info.offset_content;
        for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
            const u16 index = getbe16(content_list[i].index);
            const u32 id = getbe32(content_list[i].id);
            const u64 size = getbe64(content_list[i].size);
            const u32 keyslot = (getbe16(content_list[i].type) & 0x1) ? index : (u32) -1;
            
            u32 cnt_type = 0;
            if (size >= 0x200) {
                u8 header[0x200];
                ReadCiaContentImageBytes(header, next_offset, 0x200, keyslot, next_offset);
                cnt_type = (ValidateNcchHeader((NcchHeader*)(void*)header) == 0) ? VFLAG_NCCH :
                    (ValidateTwlHeader((TwlHeader*)(void*)header) == 0) ? VFLAG_NDS : 0;
            }
            
            snprintf(templates[n].name, 32, NAME_CIA_CONTENT, index, id, ".app");
            templates[n].offset = next_offset;
            templates[n].size = size;
            templates[n].keyslot = keyslot; // keyslot is used for CIA content index here
            templates[n].flags = VFLAG_CIA_CONTENT;
            n++;
            
            if (cnt_type) {
                memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
                snprintf(templates[n].name, 32, NAME_CIA_CONTENT, index, id, "");
                templates[n].flags |= (cnt_type | VFLAG_DIR);
                n++;
            }
            
            next_offset += size;
        }
    }
    
    n_templates_cia = n;
    return true;
}

bool BuildVGameNdsDir(void) {
    VirtualFile* templates = templates_nds;
    u32 n = 0;
    
    // header
    strncpy(templates[n].name, NAME_NDS_HEADER, 32);
    templates[n].offset = offset_nds + 0;
    templates[n].size = (twl->unit_code == TWL_UNITCODE_NTR) ? 0x200 : sizeof(TwlHeader);
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    n++;
    
    // banner
    if (twl->icon_offset) {
        u16 v = 0;
        ReadGameImageBytes(&v, offset_nds + twl->icon_offset, sizeof(u16));
        strncpy(templates[n].name, NAME_NDS_BANNER, 32);
        templates[n].offset = offset_nds + twl->icon_offset;
        templates[n].size = TWLICON_SIZE_DATA(v);
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // ARM9 section
    if (twl->arm9_size) {
        strncpy(templates[n].name, NAME_NDS_ARM9, 32);
        templates[n].offset = offset_nds + twl->arm9_rom_offset;
        templates[n].size = twl->arm9_size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // ARM9 overlay section
    if (twl->arm9_overlay_size) {
        strncpy(templates[n].name, NAME_NDS_ARM9OVL, 32);
        templates[n].offset = offset_nds + twl->arm9_overlay_offset;
        templates[n].size = twl->arm9_overlay_size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // ARM9i section
    if ((twl->unit_code != TWL_UNITCODE_NTR) && (twl->arm9i_size)) {
        strncpy(templates[n].name, NAME_NDS_ARM9I, 32);
        templates[n].offset = offset_nds + twl->arm9i_rom_offset;
        templates[n].size = twl->arm9i_size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // ARM7 section
    if (twl->arm7_size) {
        strncpy(templates[n].name, NAME_NDS_ARM7, 32);
        templates[n].offset = offset_nds + twl->arm7_rom_offset;
        templates[n].size = twl->arm7_size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // ARM7 overlay section
    if (twl->arm7_overlay_size) {
        strncpy(templates[n].name, NAME_NDS_ARM7OVL, 32);
        templates[n].offset = offset_nds + twl->arm7_overlay_offset;
        templates[n].size = twl->arm7_overlay_size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // ARM7i section
    if ((twl->unit_code != TWL_UNITCODE_NTR) && (twl->arm7i_size)) {
        strncpy(templates[n].name, NAME_NDS_ARM7I, 32);
        templates[n].offset = offset_nds + twl->arm7i_rom_offset;
        templates[n].size = twl->arm7i_size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // data
    if (twl->fnt_size && twl->fat_size && (twl->fnt_offset < twl->fat_offset)) {
        strncpy(templates[n].name, NAME_NDS_DATADIR, 32);
        templates[n].offset = offset_nds + 0;
        templates[n].size = twl->ntr_rom_size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NITRO_DIR | VFLAG_DIR;
        n++;
    }
    
    n_templates_nds = n;
    return true;
}

bool BuildVGameFirmDir(void) {
    VirtualFile* templates = templates_firm;
    u32 n = 0;
    
    // header
    strncpy(templates[n].name, NAME_FIRM_HEADER, 32);
    templates[n].offset = 0;
    templates[n].size = 0x200;
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    n++;
    
    // arm9 binary (only for encrypted)
    if (offset_a9bin != (u64) -1) {
        strncpy(templates[n].name, NAME_FIRM_ARM9BIN, 32);
        templates[n].offset = offset_a9bin;
        templates[n].size = GetArm9BinarySize(a9l);
        templates[n].keyslot = 0x15;
        templates[n].flags = 0;
        n++;
    }
    
    // section binaries & NCCHs
    for (u32 i = 0; i < 4; i++) {
        FirmSectionHeader* section = firm->sections + i;
        if (!section->size) continue;
        snprintf(templates[n].name, 32, NAME_FIRM_SECTION, i, section->address,
            (section->method == FIRM_NDMA_CPY) ? "ndma.a9"  :
            (section->method == FIRM_XDMA_CPY) ? "xdma.a11" :
            (section->method == FIRM_CPU_MEMCPY) ? "memcpy" : "unknown");
        templates[n].offset = section->offset;
        templates[n].size = section->size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_NO_CRYPTO;
        n++;
        if (section->method == FIRM_NDMA_CPY) { // ARM9 section, search for Process9
            NcchHeader p9_ncch;
            char name[8];
            u32 offset_p9 = 0;
            
            u8* buffer = (u8*) malloc(section->size);
            if (buffer) {
                if (ReadGameImageBytes(buffer, section->offset, section->size) != 0) break;
                for (u32 s = 0; (s < section->size - 0x400) && (!offset_p9); s += 0x10) {
                    if ((ValidateNcchHeader((NcchHeader*) (void*) (buffer + s)) == 0) &&
                        (ReadGameImageBytes((u8*) name, section->offset + s + 0x200, 8) == 0)) {
                        offset_p9 = section->offset + s;
                        memcpy(&p9_ncch, buffer + s, sizeof(NcchHeader));
                    }
                }
                free(buffer);
            }
            
            if (offset_p9) {
                snprintf(templates[n].name, 32, NAME_FIRM_NCCH, p9_ncch.programId, name, ".app");
                templates[n].offset = offset_p9;
                templates[n].size = p9_ncch.size * NCCH_MEDIA_UNIT;
                templates[n].keyslot = (offset_a9bin == (u64) -1) ? 0xFF : 0x15;
                templates[n].flags = 0;
                n++;
                memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
                snprintf(templates[n].name, 32, NAME_FIRM_NCCH, p9_ncch.programId, name, "");
                templates[n].flags |= (VFLAG_NCCH | VFLAG_DIR);
                n++;
            }
        } else if (section->method == FIRM_XDMA_CPY) { // ARM11 section, search for modules
            const u32 arm11_ncch_offset[] = { ARM11NCCH_OFFSET };
            NcchHeader firm_ncch;
            for (u32 v = 0; v < sizeof(arm11_ncch_offset) / sizeof(u32); v++) {
                u32 start = arm11_ncch_offset[v];
                for (u32 p = start; p < section->size; p += firm_ncch.size * NCCH_MEDIA_UNIT) {
                    char name[8];
                    if ((ReadImageBytes((u8*) &firm_ncch, section->offset + p, 0x200) != 0) ||
                        (ReadImageBytes((u8*) name, section->offset + p + 0x200, 0x8) != 0) ||
                        (ValidateNcchHeader(&firm_ncch) != 0))
                        break;
                    
                    snprintf(templates[n].name, 32, NAME_FIRM_NCCH, firm_ncch.programId, name, ".app");
                    templates[n].offset = section->offset + p;
                    templates[n].size = firm_ncch.size * NCCH_MEDIA_UNIT;
                    templates[n].keyslot = 0xFF;
                    templates[n].flags = 0;
                    n++;
                    memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
                    snprintf(templates[n].name, 32, NAME_FIRM_NCCH, firm_ncch.programId, name, "");
                    templates[n].flags |= (VFLAG_NCCH | VFLAG_DIR);
                    n++;
                }
            }
        }
    }
    
    n_templates_firm = n;
    return true;
}

bool BuildVGameTadDir(void) {
    const char* name_type[] = { NAME_TAD_TYPES };
    VirtualFile* templates = templates_tad;
    u32 content_offset = 0;
    u32 n = 0;
    
    // read header, setup table
    TadContentTable tbl;
    TadHeader hdr;
    ReadGameImageBytes(&hdr, TAD_HEADER_OFFSET, TAD_HEADER_LEN);
    if (BuildTadContentTable(&tbl, &hdr) != 0) {
        n_templates_tad = 0;
        return false;
    }
    
    // banner
    strncpy(templates[n].name, NAME_TAD_BANNER, 32);
    templates[n].offset = content_offset;
    templates[n].size = tbl.banner_end - content_offset - sizeof(TadBlockMetaData);
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    content_offset = tbl.banner_end;
    n++;
    
    // header
    strncpy(templates[n].name, NAME_TAD_HEADER, 32);
    templates[n].offset = content_offset;
    templates[n].size = tbl.header_end - content_offset - sizeof(TadBlockMetaData);
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    content_offset = tbl.header_end;
    n++;
    
    // footer
    strncpy(templates[n].name, NAME_TAD_FOOTER, 32);
    templates[n].offset = content_offset;
    templates[n].size = tbl.footer_end - content_offset - sizeof(TadBlockMetaData);
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    content_offset = tbl.footer_end;
    n++;
    
    // contents
    for (u32 i = 0; i < TAD_NUM_CONTENT; content_offset = tbl.content_end[i++]) {
        if (!hdr.content_size[i]) continue; // nothing in section
        // use proper names, fix TMD handling
        snprintf(templates[n].name, 32, NAME_TAD_CONTENT, hdr.title_id, name_type[i]);
        templates[n].offset = content_offset;
        templates[n].size = hdr.content_size[i];
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
        if (i == 1) { // SRL content
            memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
            snprintf(templates[n].name, 32, NAME_TAD_CONTENT, hdr.title_id, "srl");
            templates[n].flags |= (VFLAG_NDS | VFLAG_DIR);
            n++;
        }
    }
    
    n_templates_tad = n;
    return true;
}

void DeinitVGameDrive(void) {
    if (vgame_buffer) free(vgame_buffer);
    vgame_buffer = NULL;
}

u64 InitVGameDrive(void) { // prerequisite: game file mounted as image
    u64 type = GetMountState();
    
    vgame_type = 0;
    DeinitVGameDrive();
    
    offset_firm  = (u64) -1;
    offset_a9bin = (u64) -1;
    offset_cia   = (u64) -1;
    offset_ncsd  = (u64) -1;
    offset_ncch  = (u64) -1;
    offset_exefs = (u64) -1;
    offset_romfs = (u64) -1;
    offset_lv3   = (u64) -1;
    offset_lv3fd = (u64) -1;
    offset_nds   = (u64) -1;
    offset_nitro = (u64) -1;
    offset_tad   = (u64) -1;
    
    base_vdir =
        (type & SYS_FIRM  ) ? VFLAG_FIRM  :
        (type & GAME_CIA  ) ? VFLAG_CIA   :
        (type & GAME_NCSD ) ? VFLAG_NCSD  :
        (type & GAME_NCCH ) ? VFLAG_NCCH  :
        (type & GAME_EXEFS) ? VFLAG_EXEFS :
        (type & GAME_ROMFS) ? VFLAG_ROMFS :
        (type & GAME_NDS  ) ? VFLAG_NDS   :
        (type & GAME_TAD  ) ? VFLAG_TAD : 0;
    if (!base_vdir) return 0;
    
    // set up vgame buffer
    vgame_buffer = (void*) malloc(VGAME_BUFFER_SIZE);
    if (!vgame_buffer) return 0;
    
    templates_cia   = (VirtualFile*) ((u8*) vgame_buffer); // first 56kb reserved (enough for 1024 entries)
    templates_firm  = (VirtualFile*) (((u8*) vgame_buffer) + 0xE000); // 2kb reserved (enough for 36 entries)
    templates_ncsd  = (VirtualFile*) (((u8*) vgame_buffer) + 0xE800); // 2kb reserved (enough for 36 entries)
    templates_ncch  = (VirtualFile*) (((u8*) vgame_buffer) + 0xF000); // 1kb reserved (enough for 18 entries)
    templates_nds   = (VirtualFile*) (((u8*) vgame_buffer) + 0xF400); // 1kb reserved (enough for 18 entries)
    templates_exefs = (VirtualFile*) (((u8*) vgame_buffer) + 0xF800); // 1kb reserved (enough for 18 entries)
    templates_tad   = (VirtualFile*) (((u8*) vgame_buffer) + 0xFC00); // 1kb reserved (enough for 18 entries)
    cia   = (CiaStub*)       (void*) (((u8*) vgame_buffer) + 0x10000); // 61kB reserved - should be enough by far
    twl   = (TwlHeader*)     (void*) (((u8*) vgame_buffer) + 0x1F400); // 512 byte reserved (not the full thing)
    a9l   = (FirmA9LHeader*) (void*) (((u8*) vgame_buffer) + 0x1F600); // 512 byte reserved
    firm  = (FirmHeader*)    (void*) (((u8*) vgame_buffer) + 0x1F800); // 512 byte reserved
    ncsd  = (NcsdHeader*)    (void*) (((u8*) vgame_buffer) + 0x1FA00); // 512 byte reserved
    ncch  = (NcchHeader*)    (void*) (((u8*) vgame_buffer) + 0x1FC00); // 512 byte reserved
    exefs = (ExeFsHeader*)   (void*) (((u8*) vgame_buffer) + 0x1FE00); // 512 byte reserved
    romfslv3 = (((u8*) vgame_buffer) + 0x20000); // 1920kB reserved
    nitrofs  = (((u8*) vgame_buffer) + 0x20000); // 1920kB reserved (FNT+FAT combined)
    
    vgame_type = type;
    return type;
}

u64 CheckVGameDrive(void) {
    if (!vgame_buffer || (vgame_type != GetMountState())) vgame_type = 0; // very basic sanity check
    return vgame_type;
}

bool OpenVGameDir(VirtualDir* vdir, VirtualFile* ventry) {
    // build vdir object
    vdir->index = -1;
    if (!ventry) { // root dir
        vdir->offset = 0;
        vdir->size = GetMountSize();
        vdir->flags = VFLAG_DIR|base_vdir;
    } else { // non root dir
        if (!(ventry->flags & VFLAG_DIR) || !(ventry->flags & VFLAG_GAMEDIR))
            return false;
        vdir->offset = ventry->offset;
        vdir->size = ventry->size;
        vdir->flags = ventry->flags;
    }
    
    // CIA content special handling
    if (vdir->flags & VFLAG_CIA) { // disable content crypto
        offset_ccnt = (u64) -1;
        index_ccnt = (u32) -1;
    } else if (vdir->flags & VFLAG_CIA_CONTENT) { // enable content crypto
        offset_ccnt = vdir->offset;
        index_ccnt = ventry->keyslot;
    }
    
    // build directories where required
    if ((vdir->flags & VFLAG_FIRM) && (offset_firm != vdir->offset)) {
        if ((ReadImageBytes((u8*) firm, 0, sizeof(FirmHeader)) != 0) ||
            (ValidateFirmHeader(firm, 0) != 0)) return false;
        offset_firm = vdir->offset;
        FirmSectionHeader* arm9s = FindFirmArm9Section(firm);
        if (arm9s && (ReadImageBytes((u8*) a9l, arm9s->offset, sizeof(FirmA9LHeader)) == 0) &&
            (ValidateFirmA9LHeader(a9l) == 0) &&
            ((SetupArm9BinaryCrypto(a9l)) == 0))
            offset_a9bin = arm9s->offset + ARM9BIN_OFFSET;
        if (!BuildVGameFirmDir()) return false;
    } else if ((vdir->flags & VFLAG_TAD) && (offset_tad != vdir->offset)) {
        offset_tad = vdir->offset; // always zero(!)
        if (!BuildVGameTadDir()) return false;
    } else if ((vdir->flags & VFLAG_CIA) && (offset_cia != vdir->offset)) {
        CiaInfo info;
        if ((ReadImageBytes((u8*) cia, 0, 0x20) != 0) ||
            (ValidateCiaHeader(&(cia->header)) != 0) ||
            (GetCiaInfo(&info, &(cia->header)) != 0) ||
            (ReadImageBytes((u8*) cia, 0, info.offset_content) != 0))
            return false;
        offset_cia = vdir->offset; // always zero(!)
        GetTitleKey(cia_titlekey, &(cia->ticket));
        if (!BuildVGameCiaDir()) return false;
    } else if ((vdir->flags & VFLAG_NCSD) && (offset_ncsd != vdir->offset)) {
        if ((ReadImageBytes((u8*) ncsd, 0, sizeof(NcsdHeader)) != 0) ||
            (ValidateNcsdHeader(ncsd) != 0))
            return false;
        offset_ncsd = vdir->offset; // always zero(!)
        if (!BuildVGameNcsdDir()) return false;
    } else if ((vdir->flags & VFLAG_NCCH) && (offset_ncch != vdir->offset)) {
        offset_ncch = (u64) -1;
        if ((ReadNcchImageBytes((u8*) ncch, vdir->offset, sizeof(NcchHeader)) != 0) ||
            (ValidateNcchHeader(ncch) != 0))
            return false;
        offset_ncch = vdir->offset;
        if (!BuildVGameNcchDir()) return false;
        if (ncch->size_exefs) {
            u32 ncch_offset_exefs = offset_ncch + (ncch->offset_exefs * NCCH_MEDIA_UNIT);
            if ((ReadNcchImageBytes((u8*) exefs, ncch_offset_exefs, sizeof(ExeFsHeader)) != 0) ||
                (ValidateExeFsHeader(exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) != 0))
                return false;
            offset_exefs = ncch_offset_exefs;
            if (!BuildVGameExeFsDir()) return false;
        }
    } else if ((vdir->flags & VFLAG_EXEFS) && (offset_exefs != vdir->offset)) {
        if ((ReadNcchImageBytes((u8*) exefs, vdir->offset, sizeof(ExeFsHeader)) != 0) ||
            (ValidateExeFsHeader(exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) != 0))
            return false;
        offset_exefs = vdir->offset;
        if (!BuildVGameExeFsDir()) return false;
    } else if ((vdir->flags & VFLAG_ROMFS) && (offset_romfs != vdir->offset)) {
        offset_nitro = (u64) -1; // mutually exclusive
        // validate romFS magic
        u8 magic[] = { ROMFS_MAGIC };
        u8 header[sizeof(magic)];
        if ((ReadNcchImageBytes(header, vdir->offset, sizeof(magic)) != 0) ||
            (memcmp(magic, header, sizeof(magic)) != 0))
            return false;
        // validate lv3 header
        RomFsLv3Header* lv3 = (RomFsLv3Header*) romfslv3;
        for (u32 i = 1; i < 8; i++) {
            offset_lv3 = vdir->offset + (i*OFFSET_LV3);
            if (ReadNcchImageBytes(romfslv3, offset_lv3, sizeof(RomFsLv3Header)) != 0)
                return false;
            if (ValidateLv3Header(lv3, VGAME_BUFFER_SIZE - 0x20000) == 0)
                break;
            offset_lv3 = (u64) -1;
        }
        if ((offset_lv3 == (u64) -1) || (ReadNcchImageBytes(romfslv3, offset_lv3, lv3->offset_filedata) != 0))
            return false;
        offset_lv3fd = offset_lv3 + lv3->offset_filedata;
        offset_romfs = vdir->offset;
        BuildLv3Index(&lv3idx, romfslv3);
    } else if ((vdir->flags & VFLAG_NDS) && (offset_nds != vdir->offset)) {
        if ((ReadGameImageBytes(twl, vdir->offset, 0x200) != 0) ||
            (ValidateTwlHeader(twl) != 0))
            return false;
        offset_nds = vdir->offset;
        if (!BuildVGameNdsDir()) return false;
    } else if ((vdir->flags & VFLAG_NITRO_DIR) && (offset_nitro != offset_nds)) {
        offset_romfs = (u64) -1; // mutually exclusive
        // sanity checks
        if (!twl->fnt_size || !twl->fat_size ||
            (twl->fnt_offset >= twl->fat_offset))
            return false;
        // load NitroFNT & NitroFAT to memory
        u32 size_nitro = (twl->fat_offset + twl->fat_size) - twl->fnt_offset;
        if ((size_nitro > VGAME_BUFFER_SIZE - 0x20000) ||
            (ReadGameImageBytes(nitrofs, vdir->offset + twl->fnt_offset, size_nitro) != 0))
            return false;
        offset_nitro = offset_nds;
    }
    
    // for romfs/nitro dir: switch to lv3/nitro object 
    if (vdir->flags & (VFLAG_ROMFS|VFLAG_NITRO_DIR)) {
        vdir->index = -1;
        vdir->offset = 0;
        vdir->size = 0;
        if (vdir->flags & VFLAG_ROMFS) vdir->flags |= VFLAG_LV3;
        if (vdir->flags & VFLAG_NITRO_DIR) vdir->flags |= VFLAG_NITRO;
        vdir->flags &= ~(VFLAG_ROMFS|VFLAG_NITRO_DIR);
    }
    
    return true;
}

bool ReadVGameDirLv3(VirtualFile* vfile, VirtualDir* vdir) {
    vfile->name[0] = '\0';
    vfile->flags = VFLAG_LV3 | VFLAG_READONLY;
    vfile->keyslot = ((offset_ncch != (u64) -1) && NCCH_ENCRYPTED(ncch)) ? 
        0x2C : 0xFF; // actual keyslot may be different
    
    // start from parent dir object
    if (vdir->index == -1) vdir->index = 0; 
    
    // first child dir object, skip if not available
    if (vdir->index == 0) {
        RomFsLv3DirMeta* parent = LV3_GET_DIR(vdir->offset, &lv3idx);
        if (!parent) return false;
        if (parent->offset_child != (u32) -1) {
            vdir->offset = (u64) parent->offset_child;
            vdir->index = 1;
            vfile->flags |= VFLAG_DIR;
            vfile->offset = vdir->offset;
            return true;
        } else vdir->index = 2;
    }
    
    // parse sibling dirs
    if (vdir->index == 1) {
        RomFsLv3DirMeta* current = LV3_GET_DIR(vdir->offset, &lv3idx);
        if (!current) return false;
        if (current->offset_sibling != (u32) -1) {
            vdir->offset = (u64) current->offset_sibling;
            vfile->flags |= VFLAG_DIR;
            vfile->offset = vdir->offset;
            return true;
        } else if (current->offset_parent != (u32) -1) {
            vdir->offset = (u64) current->offset_parent;
            vdir->index = 2;
        } else return false;
    }
    
    // first child file object, skip if not available
    if (vdir->index == 2) {
        RomFsLv3DirMeta* parent = LV3_GET_DIR(vdir->offset, &lv3idx);
        if (!parent) return false;
        if (parent->offset_file != (u32) -1) {
            vdir->offset = (u64) parent->offset_file;
            vdir->index = 3;
            RomFsLv3FileMeta* lv3file = LV3_GET_FILE(vdir->offset, &lv3idx);
            if (!lv3file) return false;
            vfile->offset = vdir->offset;
            vfile->size = lv3file->size_data;
            return true;
        } else vdir->index = 4;
    }
    
    // parse sibling files
    if (vdir->index == 3) {
        RomFsLv3FileMeta* current = LV3_GET_FILE(vdir->offset, &lv3idx);
        if (!current) return false;
        if (current->offset_sibling != (u32) -1) {
            vdir->offset = current->offset_sibling;
            RomFsLv3FileMeta* lv3file = LV3_GET_FILE(vdir->offset, &lv3idx);
            if (!lv3file) return false;
            vfile->offset = vdir->offset;
            vfile->size = lv3file->size_data;
            return true;
        } else if (current->offset_parent != (u32) -1) {
            vdir->offset = current->offset_parent;
            vdir->index = 4;
        } else return false;
    }
    
    return false;
}

bool ReadVGameDirNitro(VirtualFile* vfile, VirtualDir* vdir) {
    u8* fnt = nitrofs;
    u8* fat = nitrofs + twl->fat_offset - twl->fnt_offset;
        
    vfile->name[0] = '\0';
    vfile->flags = VFLAG_NITRO | VFLAG_READONLY;
    vfile->keyslot = 0;
    
    // start from parent dir object
    if (vdir->index == -1) {
        u8* fnt_entry = NULL;
        u32 dirid = vdir->offset & 0xFFF;
        u32 fileid = 0;
        if (FindNitroRomDir(dirid, &fileid, &fnt_entry, twl, fnt, fat) == 0) {
            vdir->index = fileid; // store fileid in index
            vdir->offset = (vdir->offset&0xFFFFFFFF) | (((u64)(fnt_entry - fnt)) << 32); // store offsets in offset
        } else vdir->index = -3; // error
    }
    
    // read directory entries until done
    if (vdir->index >= 0) {
        u8* fnt_entry = fnt + (vdir->offset >> 32);
        u32 fileid = vdir->index;
        bool is_dir;
        if (ReadNitroRomEntry(&(vfile->offset), &(vfile->size), &is_dir, fileid, fnt_entry, fat) == 0) {
            if (!is_dir) vfile->offset += offset_nds;
            vfile->offset |= ((u64)(fnt_entry - fnt)) << 32;
            if (is_dir) vfile->flags |= VFLAG_DIR;
            // advance to next entry
            NextNitroRomEntry(&fileid, &fnt_entry);
            vdir->index = fileid;
            vdir->offset = (vdir->offset&0xFFFFFFFF) | (((u64)(fnt_entry - fnt)) << 32);
        } else vdir->index = -2; // end of dir
    }
    
    return (vdir->index >= 0);
}
    
bool ReadVGameDir(VirtualFile* vfile, VirtualDir* vdir) {
    VirtualFile* templates = NULL;
    int n = 0;
    
    if (vdir->flags & VFLAG_FIRM) {
        templates = templates_firm;
        n = n_templates_firm;
    } else if (vdir->flags & VFLAG_CIA) {
        templates = templates_cia;
        n = n_templates_cia;
    } else if (vdir->flags & VFLAG_NCSD) {
        templates = templates_ncsd;
        n = n_templates_ncsd;
    } else if (vdir->flags & VFLAG_NCCH) {
        templates = templates_ncch;
        n = n_templates_ncch;
    } else if (vdir->flags & VFLAG_EXEFS) {
        templates = templates_exefs;
        n = n_templates_exefs;
    } else if (vdir->flags & VFLAG_NDS) {
        templates = templates_nds;
        n = n_templates_nds;
    } else if (vdir->flags & VFLAG_TAD) {
        templates = templates_tad;
        n = n_templates_tad;
    } else if (vdir->flags & VFLAG_LV3) {
        return ReadVGameDirLv3(vfile, vdir);
    } else if (vdir->flags & VFLAG_NITRO) {
        return ReadVGameDirNitro(vfile, vdir);
    }
    
    if (++vdir->index < n) {
        // copy current template to vfile and set readonly flag
        memcpy(vfile, templates + vdir->index, sizeof(VirtualFile));
        vfile->flags |= VFLAG_READONLY;
        return true;
    }
    
    return false;
}

int ReadVGameFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    u32 vfoffset = vfile->offset;
    if (vfile->flags & VFLAG_LV3) {
        RomFsLv3FileMeta* lv3file;
        if (vfile->flags & VFLAG_DIR) return -1;
        lv3file = LV3_GET_FILE(vfile->offset, &lv3idx);
        vfoffset = offset_lv3fd + lv3file->offset_data;
    } else if (vfile->flags & VFLAG_NITRO) {
        vfoffset = vfile->offset & 0xFFFFFFFF;
    }
    if (vfile->flags & VFLAG_NO_CRYPTO)
        return ReadImageBytes(buffer, vfoffset + offset, count);
    else if (vfile->flags & VFLAG_CIA_CONTENT)
        return ReadCiaContentImageBytes(buffer, vfoffset + offset, count, vfile->keyslot, vfoffset);
    else if (vfile->flags & VFLAG_NCCH_CRYPTO)
        return ReadNcchImageBytes(buffer, vfoffset + offset, count);
    else return ReadGameImageBytes(buffer, vfoffset + offset, count);
}

bool FindVirtualFileInLv3Dir(VirtualFile* vfile, const VirtualDir* vdir, const char* name) {
    vfile->name[0] = '\0';
    vfile->flags = vdir->flags & ~VFLAG_DIR;
    vfile->keyslot = ((offset_ncch != (u64) -1) && NCCH_ENCRYPTED(ncch)) ?
        0x2C : 0xFF; // actual keyslot may be different
    
    RomFsLv3DirMeta* lv3dir = GetLv3DirMeta(name, vdir->offset, &lv3idx);
    if (lv3dir) {
        vfile->offset = ((u8*) lv3dir) - ((u8*) lv3idx.dirmeta);
        vfile->size = 0;
        vfile->flags |= VFLAG_DIR;
        return true;
    }
    
    RomFsLv3FileMeta* lv3file = GetLv3FileMeta(name, vdir->offset, &lv3idx);
    if (lv3file) {
        vfile->offset = ((u8*) lv3file) - ((u8*) lv3idx.filemeta);
        vfile->size = lv3file->size_data;
        return true;
    }
    
    return false;
}

bool GetVGameLv3Filename(char* name, const VirtualFile* vfile, u32 n_chars) {
    if (!(vfile->flags & VFLAG_LV3))
        return false;
    
    u16* wname = NULL;
    u32 name_len = 0;
    
    if (vfile->flags & VFLAG_DIR) {
        RomFsLv3DirMeta* dirmeta = LV3_GET_DIR(vfile->offset, &lv3idx);
        if (!dirmeta) return false;
        wname = dirmeta->wname;
        name_len = dirmeta->name_len / 2;
    } else {
        RomFsLv3FileMeta* filemeta = LV3_GET_FILE(vfile->offset, &lv3idx);
        if (!filemeta) return false;
        wname = filemeta->wname;
        name_len = filemeta->name_len / 2;
    }
    memset(name, 0, n_chars);
    utf16_to_utf8((u8*) name, wname, n_chars-1, name_len);
    
    return true;
}

bool GetVGameNitroFilename(char* name, const VirtualFile* vfile, u32 n_chars) {
    if (!(vfile->flags & VFLAG_NITRO))
        return false;
    
    u8* fnt_entry = nitrofs + (vfile->offset >> 32);
    u32 name_len = (*fnt_entry) & ~0x80;
    if (name_len >= n_chars) return false;
    memset(name, 0, n_chars);
    memcpy(name, fnt_entry + 1, name_len);
    for (u32 i = 0; i < name_len; i++)
        if (name[i] == '%') name[i] = '_';
    
    return true;
}

bool GetVGameFilename(char* name, const VirtualFile* vfile, u32 n_chars) {
    if (vfile->flags & VFLAG_LV3) return GetVGameLv3Filename(name, vfile, n_chars);
    else if (vfile->flags & VFLAG_NITRO) return GetVGameNitroFilename(name, vfile, n_chars);
    else strncpy(name, vfile->name, n_chars);
    return true;
}

bool MatchVGameFilename(const char* name, const VirtualFile* vfile, u32 n_chars) {
    if (vfile->flags & VFLAG_LV3) {
        char lv3_name[256];
        if (!GetVGameLv3Filename(lv3_name, vfile, 256)) return false;
        return (strncasecmp(name, lv3_name, n_chars) == 0);
    } else if (vfile->flags & VFLAG_NITRO) {
        char nitro_name[128];
        if (!GetVGameNitroFilename(nitro_name, vfile, 128)) return false;
        return (strncasecmp(name, nitro_name, n_chars) == 0);
    } else return (strncasecmp(name, vfile->name, 32) == 0);
}

u64 GetVGameDriveSize(void) {
    return (vgame_type) ? GetMountSize() : 0;
}
