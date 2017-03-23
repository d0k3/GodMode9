#include "ctrtransfer.h"
#include "nandcmac.h"
#include "fsutil.h"
#include "fsinit.h"
#include "fsperm.h"
#include "image.h"
#include "gameutil.h"
#include "essentials.h"
#include "ui.h"


u32 TransferCtrNandImage(const char* path_img, const char* drv) {
    if (!CheckWritePermissions(drv)) return 1;
    
    // backup current mount path, mount new path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    if (*path_store) path_bak = path_store;
    if (!InitImgFS(path_img)) {
        InitImgFS(path_bak);
        return 1;
    }
    
    // CTRNAND preparations
    SecureInfo* secnfo_img = (SecureInfo*) TEMP_BUFFER;
    SecureInfo* secnfo_loc = (SecureInfo*) (TEMP_BUFFER + 0x200);
    char* path_secnfo_a = (char*) (TEMP_BUFFER + 0x400);
    char* path_secnfo_b = (char*) (TEMP_BUFFER + 0x420);
    char* path_secnfo_c = (char*) (TEMP_BUFFER + 0x440);
    char* path_tickdb = (char*) (TEMP_BUFFER + 0x460);
    char* path_tickdb_bak = (char*) (TEMP_BUFFER + 0x480);
    snprintf(path_secnfo_a, 32, "%s/rw/sys/SecureInfo_A", drv);
    snprintf(path_secnfo_b, 32, "%s/rw/sys/SecureInfo_B", drv);
    snprintf(path_secnfo_c, 32, "%s/rw/sys/SecureInfo_C", drv);
    snprintf(path_tickdb, 32, "%s/dbs/ticket.db", drv);
    snprintf(path_tickdb_bak, 32, "%s/dbs/ticket.bak", drv);
    // special handling for out of region images (create SecureInfo_C)
    if (((FileGetData("7:/rw/sys/SecureInfo_A", (u8*) secnfo_img, sizeof(SecureInfo), 0) == sizeof(SecureInfo)) || 
         (FileGetData("7:/rw/sys/SecureInfo_B", (u8*) secnfo_img, sizeof(SecureInfo), 0) == sizeof(SecureInfo))) &&
        ((FileGetData(path_secnfo_a, (u8*) secnfo_loc, sizeof(SecureInfo), 0) == sizeof(SecureInfo)) || 
         (FileGetData(path_secnfo_b, (u8*) secnfo_loc, sizeof(SecureInfo), 0) == sizeof(SecureInfo))) && 
        (secnfo_img->region != secnfo_loc->region)) {
        secnfo_loc->region = secnfo_img->region;
        FileSetData(path_secnfo_c, (u8*) secnfo_loc, sizeof(SecureInfo), 0, true);
    }
    // make a backup of ticket.db
    PathDelete(path_tickdb_bak);
    PathRename(path_tickdb, "ticket.bak");
    
    // actual transfer - db files / titles / H&S inject markfile
    const char* dbnames[] = { "ticket.db", "certs.db", "title.db", "import.db", "tmp_t.db", "tmp_i.db" };
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
        FixFileCmac(path_to);
    }
    snprintf(path_to, 32, "%s/" HSINJECT_MARKFILE, drv);
    PathDelete(path_to);
    ShowString("Cleaning up titles, please wait...");
    snprintf(path_to, 32, "%s/title", drv);
    snprintf(path_from, 32, "7:/title");
    PathDelete(path_to);
    PathCopy(drv, path_from, &flags);
    
    InitImgFS(path_bak);
    return 0;
}
