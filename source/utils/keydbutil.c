#include "keydbutil.h"
#include "fsperm.h"
#include "filetype.h"
#include "unittype.h"
#include "vff.h"
#include "ui.h"

#define MAX_KEYDB_SIZE  (TEMP_BUFFER_SIZE)

u32 CryptAesKeyDb(const char* path, bool inplace, bool encrypt) {
    AesKeyInfo* keydb = (AesKeyInfo*) MAIN_BUFFER;
    const char* path_out = (inplace) ? path : OUTPUT_PATH "/" KEYDB_NAME;
    u32 n_keys;
    UINT bt, btw;
    
    // write permissions
    if (!CheckWritePermissions(path_out))
        return 1;
    
    if (!inplace) {
        // ensure the output dir exists
        if (fvx_rmkdir(OUTPUT_PATH) != FR_OK)
            return 1;
    }
    
    // load key database
    if ((fvx_qread(path, keydb, 0, MAIN_BUFFER_SIZE, &bt) != FR_OK) ||
        (bt % sizeof(AesKeyInfo)) || (bt >= MAIN_BUFFER_SIZE))
        return 1;
    
    // en-/decrypt keys
    n_keys = bt / sizeof(AesKeyInfo);
    for (u32 i = 0; i < n_keys; i++) {
        if ((bool) keydb[i].isEncrypted == !encrypt)
            CryptAesKeyInfo(&(keydb[i]));
    }
    
    // dump key database
    if (!inplace) f_unlink(path_out);
    if ((fvx_qwrite(path_out, keydb, 0, bt, &btw) != FR_OK) || (bt != btw))
        return 1;
    
    return 0;
}

u32 AddKeyToDb(AesKeyInfo* key_info, AesKeyInfo* key_entry) {
    AesKeyInfo* key = key_info;
    if (key_entry) { // key entry provided
        for (; key->slot < 0x40; key++) {
            if ((u8*) key - (u8*) key_info >= MAX_KEYDB_SIZE) return 1;
            if ((key_entry->slot == key->slot) && (key_entry->type == key->type) &&
                (strncasecmp(key_entry->id, key->id, 10) == 0) &&
                (key_entry->keyUnitType == key->keyUnitType))
                return 0; // key already in db
        }
        memcpy(key++, key_entry, sizeof(AesKeyInfo));
    }
    if ((u8*) key - (u8*) key_info >= MAX_KEYDB_SIZE) return 1;
    memset(key, 0xFF, sizeof(AesKeyInfo)); // this used to signal keydb end
    return 0;
}

u32 BuildKeyDb(const char* path, bool dump) {
    AesKeyInfo* key_info = (AesKeyInfo*) MAIN_BUFFER;
    const char* path_out = OUTPUT_PATH "/" KEYDB_NAME;
    const char* path_in = path;
    UINT br;
    
    // write permissions
    if (!CheckWritePermissions(path_out))
        return 1;
    
    if (!path_in && !dump) { // no input path given - initialize
        AddKeyToDb(key_info, NULL);
        if ((fvx_stat(path_out, NULL) == FR_OK) &&
            (ShowPrompt(true, "%s\nOutput file already exists.\nUpdate this?", path_out)))
            path_in = path_out;
        else return 0;
    }
    
    u32 filetype = path_in ? IdentifyFileType(path_in) : 0;
    if (filetype & BIN_KEYDB) { // AES key database
        AesKeyInfo* key_info_merge = (AesKeyInfo*) TEMP_BUFFER;
        if ((fvx_qread(path_in, key_info_merge, 0, TEMP_BUFFER_SIZE, &br) != FR_OK) ||
            (br % sizeof(AesKeyInfo)) || (br >= MAIN_BUFFER_SIZE)) return 1;
        u32 n_keys = br / sizeof(AesKeyInfo);
        for (u32 i = 0; i < n_keys; i++) {
            if (key_info_merge[i].isEncrypted) // build an unencrypted db
                CryptAesKeyInfo(&(key_info_merge[i]));
            if (AddKeyToDb(key_info, key_info_merge + i) != 0) return 1;
        }
    } else if (filetype & BIN_LEGKEY) { // legacy key file
        AesKeyInfo key;
        unsigned int keyslot = 0xFF;
        char typestr[32] = { 0 };
        char* name_in = strrchr(path_in, '/');
        memset(&key, 0, sizeof(AesKeyInfo));
        key.type = 'N';
        if (!name_in || (strnlen(++name_in, 32) > 24)) return 1; // safety
        if ((sscanf(name_in, "slot0x%02XKey%s", &keyslot, typestr) != 2) &&
            (sscanf(name_in, "slot0x%02Xkey%s", &keyslot, typestr) != 2)) return 1;
        char* ext = strchr(typestr, '.');
        if (!ext) return 1;
        *(ext++) = '\0';
        if ((*typestr == 'X') || (*typestr == 'Y')) {
            key.type = *typestr;
            strncpy(key.id, typestr + 1, 10);
        } else if ((typestr[0] == 'I') && (typestr[1] == 'V')) {
            key.type = 'I';
            strncpy(key.id, typestr + 2, 10);
        } else strncpy(key.id, typestr, 10);
        key.slot = keyslot;
        key.keyUnitType = (strncasecmp(ext, "ret.bin", 10) == 0) ? KEYS_RETAIL :
            (strncasecmp(ext, "dev.bin", 10) == 0) ? KEYS_DEVKIT : 0;
        if ((fvx_qread(path_in, key.key, 0, 16, &br) != FR_OK) || (br != 16)) return 1;
        if (AddKeyToDb(key_info, &key) != 0) return 1;
    }
    
    if (dump) {
        u32 dump_size = 0;
        for (AesKeyInfo* key = key_info; key->slot <= 0x40; key++) {
            dump_size += sizeof(AesKeyInfo);
            if (dump_size >= MAX_KEYDB_SIZE) return 1;
        }
        if (fvx_rmkdir(OUTPUT_PATH) != FR_OK) // ensure the output dir exists
            return 1;
        f_unlink(path_out);
        if (!dump_size || (fvx_qwrite(path_out, key_info, 0, dump_size, &br) != FR_OK) || (br != dump_size))
            return 1;
    }
    
    return 0;
}
