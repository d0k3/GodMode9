#include "vtickdb.h"
#include "image.h"
#include "keydb.h"

#define VKEYDB_BUFFER_SIZE  STD_BUFFER_SIZE // 1MB, enough for 32768 entries

#define NAME_LEGKEY "slot0x%02lXKey%.10s%s.bin" // keyslot / type string / unit extension

static AesKeyInfo* key_info = NULL;
static u32 n_keys = 0;

void DeinitVKeyDbDrive(void) {
    if (key_info) free(key_info);
    key_info = NULL;
}

u64 InitVKeyDbDrive(void) { // prerequisite: aeskeydb.bin mounted as image
    if (!(GetMountState() & BIN_KEYDB)) return 0;
    n_keys = 0;
    
    // sanity check
    u64 fsize = GetMountSize();
    if (!fsize || (fsize % sizeof(AesKeyInfo)) || (fsize > VKEYDB_BUFFER_SIZE)) {
        n_keys = 0;
        return 0;
    }
    
    // setup vkeydb buffer
    DeinitVKeyDbDrive(); // dangerous shit
    key_info = (AesKeyInfo*) malloc(VKEYDB_BUFFER_SIZE);
    if (!key_info) return 0;
    
    // load the full database into memory
    n_keys = fsize / sizeof(AesKeyInfo);
    if (ReadImageBytes((u8*) key_info, 0, fsize) != 0) n_keys = 0;
    
    // decrypt keys if required
    for (u32 i = 0; i < n_keys; i++) {
        if (key_info[i].isEncrypted) CryptAesKeyInfo(&(key_info[i]));
    }
    
    if (!n_keys) DeinitVKeyDbDrive();
    return (n_keys) ? BIN_KEYDB : 0;
}

u64 CheckVKeyDbDrive(void) {
    if ((GetMountState() & BIN_KEYDB) && key_info) // very basic sanity check
        return BIN_KEYDB;
    return 0;
}

bool ReadVKeyDbDir(VirtualFile* vfile, VirtualDir* vdir) {
    if (++vdir->index < (int) n_keys) {
        AesKeyInfo* key_entry = key_info + vdir->index;
        u32 keyslot = key_entry->slot;
        char typestr[16] = { 0 };
        char* unitext =
            (key_entry->keyUnitType == KEYS_DEVKIT) ? ".dev" :
            (key_entry->keyUnitType == KEYS_RETAIL) ? ".ret" : "";
        snprintf(typestr, 12 + 1, "%s%.10s", (key_entry->type == 'I') ? "IV" :
            (key_entry->type == 'X') ? "X" : (key_entry->type == 'Y') ? "Y" : "", key_entry->id);
        
        memset(vfile, 0, sizeof(VirtualFile));
        snprintf(vfile->name, 32, NAME_LEGKEY, keyslot, typestr, unitext);
        vfile->offset = vdir->index * sizeof(AesKeyInfo);
        vfile->size = 16; // standard size of a key
        vfile->keyslot = 0xFF;
        vfile->flags = VFLAG_READONLY;
        
        return true; // found
    }
    
    return false;
}

int ReadVKeyDbFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    AesKeyInfo* key_entry = key_info + (vfile->offset / sizeof(AesKeyInfo));
    memcpy(buffer, key_entry->key + offset, count);
    return 0;
}

u64 GetVKeyDbDriveSize(void) {
    return (n_keys) ? GetMountSize() : 0;
}
