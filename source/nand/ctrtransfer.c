#include "ctrtransfer.h"
#include "nandcmac.h"
#include "fsutil.h"
#include "fsinit.h"
#include "fsperm.h"
#include "image.h"
#include "essentials.h"
#include "ui.h"

u32 AdaptCtrNandImage(const char* path) {
    if (!CheckWritePermissions(path)) return 1;
    
    // a little warning...
    if (!ShowPrompt(true, "This will alter the provided image\nto make it transferable to your NAND."))
        return 1;
    
    // backup current mount path, mount new path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    if (*path_store) path_bak = path_store;
    if (!InitImgFS(path)) {
        InitImgFS(path_bak);
        return 1;
    }
    
    // fixing CMACs
    ShowString("Fixing .db CMACs, please wait...");
    if ((FixFileCmac("7:/dbs/ticket.db") != 0) ||
        (FixFileCmac("7:/dbs/certs.db") != 0) ||
        (FixFileCmac("7:/dbs/title.db") != 0)) {
        ShowPrompt(false, "Error: .db file missing.");
        InitImgFS(path_bak);
        return 1;
    }
    FixFileCmac("7:/dbs/import.db");
    FixFileCmac("7:/dbs/tmp_t.db");
    FixFileCmac("7:/dbs/tmp_i.db");
    
    // cleanup this image
    ShowString("Cleaning up image, please wait...");
    // special handling for out of region images
    SecureInfo* secnfo_img = (SecureInfo*) TEMP_BUFFER;
    SecureInfo* secnfo_loc = (SecureInfo*) (TEMP_BUFFER + 0x200);
    if (((FileGetData("7:/rw/sys/SecureInfo_A", (u8*) secnfo_img, sizeof(SecureInfo), 0) == sizeof(SecureInfo)) || 
         (FileGetData("7:/rw/sys/SecureInfo_B", (u8*) secnfo_img, sizeof(SecureInfo), 0) == sizeof(SecureInfo))) &&
        ((FileGetData("1:/rw/sys/SecureInfo_A", (u8*) secnfo_loc, sizeof(SecureInfo), 0) == sizeof(SecureInfo)) || 
         (FileGetData("1:/rw/sys/SecureInfo_B", (u8*) secnfo_loc, sizeof(SecureInfo), 0) == sizeof(SecureInfo))) && 
        (secnfo_img->region != secnfo_loc->region)) {
        secnfo_loc->region = secnfo_img->region;
        FileSetData("1:/rw/sys/SecureInfo_C", (u8*) secnfo_loc, sizeof(SecureInfo), 0, true);
    }
    // actual cleanup
    PathDelete("7:/private/movable.sed");
    PathDelete("7:/rw/sys/LocalFriendCodeSeed_B");
    PathDelete("7:/rw/sys/LocalFriendCodeSeed_A");
    PathDelete("7:/rw/sys/SecureInfo_A");
    PathDelete("7:/rw/sys/SecureInfo_B");
    PathDelete("7:/data");
    
    // inject all required files to image
    u32 flags = OVERWRITE_ALL;
    if (!PathCopy("7:/private", "1:/private/movable.sed", &flags) ||
        !(PathCopy("7:/rw/sys", "1:/rw/sys/LocalFriendCodeSeed_B", &flags) ||
          PathCopy("7:/rw/sys", "1:/rw/sys/LocalFriendCodeSeed_A", &flags)) ||
        !(PathCopy("7:/rw/sys", "1:/rw/sys/SecureInfo_A", &flags) ||
          PathCopy("7:/rw/sys", "1:/rw/sys/SecureInfo_B", &flags))) {
        ShowPrompt(false, "Error: Failed transfering data.");
        InitImgFS(path_bak);
        return 1;
    }
    PathCopy("7:", "1:/data", &flags);
    
    InitImgFS(path_bak);
    return 0;
}
