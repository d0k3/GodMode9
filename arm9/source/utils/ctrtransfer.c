#include "ctrtransfer.h"
#include "nandcmac.h"
#include "fs.h"
#include "essentials.h"
#include "language.h"
#include "ui.h"
#include "sha.h"


/*static const u8 twl_mbr[0x42] = { // encrypted version inside the NCSD NAND header (@0x1BE)
    0x00, 0x04, 0x18, 0x00, 0x06, 0x01, 0xA0, 0x3F, 0x97, 0x00, 0x00, 0x00, 0xA9, 0x7D, 0x04, 0x00,
    0x00, 0x04, 0x8E, 0x40, 0x06, 0x01, 0xA0, 0xC3, 0x8D, 0x80, 0x04, 0x00, 0xB3, 0x05, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xAA
};*/

static const u8 ctr_mbr_o3ds[0x42] = { // found at the beginning of the CTRNAND partition (O3DS)
    0x00, 0x05, 0x2B, 0x00, 0x06, 0x02, 0x42, 0x80, 0x65, 0x01, 0x00, 0x00, 0x1B, 0x9F, 0x17, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xAA
};

static const u8 ctr_mbr_n3ds[0x42] = { // found at the beginning of the CTRNAND partition (N3DS)
    0x00, 0x05, 0x1D, 0x00, 0x06, 0x02, 0x82, 0x17, 0x57, 0x01, 0x00, 0x00, 0x69, 0xE9, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xAA
};


u32 CheckTransferableMbr(void* data) { // strict checks, custom partitions not allowed
    u8* mbr = (u8*) data;
    if (memcmp(mbr + (0x200 - sizeof(ctr_mbr_o3ds)), ctr_mbr_o3ds, sizeof(ctr_mbr_o3ds)) == 0)
        return 0; // is transferable, O3DS type
    else if (memcmp(mbr + (0x200 - sizeof(ctr_mbr_n3ds)), ctr_mbr_n3ds, sizeof(ctr_mbr_n3ds)) == 0)
        return 0; // is transferable, N3DS type
    return 1;
}

u32 TransferCtrNandImage(const char* path_img, const char* drv) {
    if (!CheckWritePermissions(drv)) return 1;

    // backup current mount path, mount new path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    path_store[255] = '\0';
    if (*path_store) path_bak = path_store;
    if (!InitImgFS(path_img)) {
        InitImgFS(path_bak);
        return 1;
    }

    // CTRNAND preparations
    SecureInfo secnfo_img;
    SecureInfo secnfo_loc;
    char path_secnfo_a[32];
    char path_secnfo_b[32];
    char path_secnfo_c[32];
    char path_tickdb[32];
    char path_tickdb_bak[32];

    snprintf(path_secnfo_a, 32, "%s/rw/sys/SecureInfo_A", drv);
    snprintf(path_secnfo_b, 32, "%s/rw/sys/SecureInfo_B", drv);
    snprintf(path_secnfo_c, 32, "%s/rw/sys/SecureInfo_C", drv);
    snprintf(path_tickdb, 32, "%s/dbs/ticket.db", drv);
    snprintf(path_tickdb_bak, 32, "%s/dbs/ticket.bak", drv);

    // special handling for out of region images (create SecureInfo_C)
    PathDelete(path_secnfo_c); // not required when transfering back to original region
    if (((FileGetData("7:/rw/sys/SecureInfo_A", (u8*) &secnfo_img, sizeof(SecureInfo), 0) == sizeof(SecureInfo)) ||
         (FileGetData("7:/rw/sys/SecureInfo_B", (u8*) &secnfo_img, sizeof(SecureInfo), 0) == sizeof(SecureInfo))) &&
        ((FileGetData(path_secnfo_a, (u8*) &secnfo_loc, sizeof(SecureInfo), 0) == sizeof(SecureInfo)) ||
         (FileGetData(path_secnfo_b, (u8*) &secnfo_loc, sizeof(SecureInfo), 0) == sizeof(SecureInfo))) &&
        (secnfo_img.region != secnfo_loc.region)) {
        secnfo_loc.region = secnfo_img.region;
        FileSetData(path_secnfo_c, (u8*) &secnfo_loc, sizeof(SecureInfo), 0, true);
    }
    // make a backup of ticket.db
    PathDelete(path_tickdb_bak);
    PathRename(path_tickdb, "ticket.bak");

    // disarm anti savegame restore (thanks @TurdPooCharger)
    char path_movable[32];
    char path_asr[96];
    u8 sd_keyy[0x10] __attribute__((aligned(4)));
    snprintf(path_movable, 32, "%s/private/movable.sed", drv);
    if (FileGetData(path_movable, sd_keyy, 0x10, 0x110) == 0x10) {
        u32 sha256sum[8];
        sha_quick(sha256sum, sd_keyy, 0x10, SHA256_MODE);
        snprintf(path_asr, 96, "%s/data/%08lx%08lx%08lx%08lx/sysdata/00010011/00000000",
            drv, sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
        PathDelete(path_asr);
    }

    // actual transfer - db files / titles
    static const char* dbnames[] = { "ticket.db", "certs.db", "title.db", "import.db", "tmp_t.db", "tmp_i.db" };
    char path_to[32];
    char path_from[32];
    char path_dbs[32];
    u32 flags = OVERWRITE_ALL;
    snprintf(path_dbs, 32, "%s/dbs", drv);
    for (u32 i = 0; i < sizeof(dbnames) / sizeof(char*); i++) {
        snprintf(path_to, 32, "%s/dbs/%s", drv, dbnames[i]);
        snprintf(path_from, 32, "7:/dbs/%s", dbnames[i]);
        PathDelete(path_to);
        PathCopy(path_dbs, path_from, &flags);
        FixFileCmac(path_to, true);
    }
    ShowString("%s", STR_CLEANING_UP_TITLES_PLEASE_WAIT);
    snprintf(path_to, 32, "%s/title", drv);
    snprintf(path_from, 32, "7:/title");
    PathDelete(path_to);
    PathCopy(drv, path_from, &flags);

    InitImgFS(path_bak);
    return 0;
}
