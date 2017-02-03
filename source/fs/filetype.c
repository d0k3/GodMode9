#include "filetype.h"
#include "fsutil.h"
#include "fatmbr.h"
#include "game.h"
#include "chainload.h"

u32 IdentifyFileType(const char* path) {
    const u8 romfs_magic[] = { ROMFS_MAGIC };
    u8 header[0x200] __attribute__((aligned(32))); // minimum required size
    void* data = (void*) header;
    size_t fsize = FileGetSize(path);
    char* fname = strrchr(path, '/');
    char* ext = (fname) ? strrchr(++fname, '.') : NULL;
    if (ext) ext++;
    if (FileGetData(path, header, 0x200, 0) < ((fsize > 0x200) ? 0x200 : fsize)) return 0;
    
    if (fsize >= 0x200) {
        if ((getbe32(header + 0x100) == 0x4E435344) && (getbe64(header + 0x110) == (u64) 0x0104030301000000) &&
            (getbe64(header + 0x108) == (u64) 0) && (fsize >= 0x8FC8000)) {
            return IMG_NAND; // NAND image
        } else if (ValidateFatHeader(header) == 0) {
            return IMG_FAT; // FAT image file
        } else if (ValidateMbrHeader((MbrHeader*) data) == 0) {
            MbrHeader* mbr = (MbrHeader*) data;
            MbrPartitionInfo* partition0 = mbr->partitions;
            if ((partition0->sector + partition0->count) <= (fsize / 0x200)) // size check
                return IMG_FAT; // possibly an MBR -> also treat as FAT image
        } else if (ValidateCiaHeader((CiaHeader*) data) == 0) {
            // this only works because these functions ignore CIA content index
            CiaInfo info;
            GetCiaInfo(&info, (CiaHeader*) header);
            if (fsize >= info.size_cia)
                return GAME_CIA; // CIA file
        } else if (ValidateNcsdHeader((NcsdHeader*) data) == 0) {
            NcsdHeader* ncsd = (NcsdHeader*) data;
            if (fsize >= GetNcsdTrimmedSize(ncsd))
                return GAME_NCSD; // NCSD (".3DS") file
        } else if (ValidateNcchHeader((NcchHeader*) data) == 0) {
            NcchHeader* ncch = (NcchHeader*) data;
            u32 type = GAME_NCCH | (NCCH_IS_CXI(ncch) ? FLAG_CXI : 0);
            if (fsize >= (ncch->size * NCCH_MEDIA_UNIT))
                return type; // NCCH (".APP") file
        } else if (ValidateExeFsHeader((ExeFsHeader*) data, fsize) == 0) {
            return GAME_EXEFS; // ExeFS file (false positives possible)
        } else if (memcmp(header, romfs_magic, sizeof(romfs_magic)) == 0) {
            return GAME_ROMFS; // RomFS file (check could be better)
        } else if (strncmp(TMD_ISSUER, (char*) (header + 0x140), 0x40) == 0) {
            if (fsize >= TMD_SIZE_N(getbe16(header + 0x1DE)))
                return GAME_TMD; // TMD file
        } else if (ValidateFirmHeader((FirmHeader*) data, fsize) == 0) {
            return SYS_FIRM; // FIRM file
        }
    }
    if ((fsize > sizeof(BossHeader)) &&
        (ValidateBossHeader((BossHeader*) data, fsize) == 0)) {
        return GAME_BOSS; // BOSS (SpotPass) file
    } else if ((fsize > sizeof(NcchInfoHeader)) &&
        (GetNcchInfoVersion((NcchInfoHeader*) data)) &&
        fname && (strncasecmp(fname, NCCHINFO_NAME, 32) == 0)) {
        return BIN_NCCHNFO; // ncchinfo.bin file
    #if PAYLOAD_MAX_SIZE <= TEMP_BUFFER_SIZE
    } else if ((fsize <= PAYLOAD_MAX_SIZE) && ext && (strncasecmp(ext, "bin", 4) == 0)) {
        return BIN_LAUNCH; // assume it's an ARM9 payload
    #endif
    }
    
    return 0;
}
