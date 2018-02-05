#include "keydbutil.h"
#include "fs.h"
#include "ui.h"
#include "unittype.h"

#define MAX_KEYDB_SIZE  (STD_BUFFER_SIZE)

u32 CryptAesKeyDb(const char* path, bool inplace, bool encrypt) {
    AesKeyInfo* keydb = NULL;
    const char* path_out = (inplace) ? path : OUTPUT_PATH "/" KEYDB_NAME;
    
    // write permissions
    if (!CheckWritePermissions(path_out))
        return 1;
    
    if (!inplace) {
        // ensure the output dir exists
        if (fvx_rmkdir(OUTPUT_PATH) != FR_OK)
            return 1;
    }
    
    // check key database size
    u32 fsize = fvx_qsize(path);
    if (!fsize || (fsize % sizeof(AesKeyInfo)) || (fsize > MAX_KEYDB_SIZE))
        return 1;
    
    keydb = (AesKeyInfo*) malloc(fsize);
    if (!keydb) return 1;
    
    // load key database
    if (fvx_qread(path, keydb, 0, fsize, NULL) != FR_OK) {
        free(keydb);
        return 1;
    }
    
    // en-/decrypt keys
    u32 n_keys = fsize / sizeof(AesKeyInfo);
    for (u32 i = 0; i < n_keys; i++) {
        if ((bool) keydb[i].isEncrypted == !encrypt)
            CryptAesKeyInfo(&(keydb[i]));
    }
    
    // dump key database
    if (!inplace) f_unlink(path_out);
    if (fvx_qwrite(path_out, keydb, 0, fsize, NULL) != FR_OK) {
        free(keydb);
        return 1;
    }
    
    free(keydb);
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
    static AesKeyInfo* key_info = NULL;
    const char* path_out = OUTPUT_PATH "/" KEYDB_NAME;
    const char* path_in = path;
    
    // write permissions
    if (!CheckWritePermissions(path_out))
        return 1;
    
    if (!path_in && !dump) { // no input path given - initialize
        if (!key_info) key_info = (AesKeyInfo*) malloc(STD_BUFFER_SIZE);
        if (!key_info) return 1;
        memset(key_info, 0xFF, sizeof(AesKeyInfo));
        
        AddKeyToDb(key_info, NULL);
        if ((fvx_stat(path_out, NULL) == FR_OK) &&
            (ShowPrompt(true, "%s\nOutput file already exists.\nUpdate this?", path_out)))
            path_in = path_out;
        else return 0;
    }
    
    // key info has to be allocated at this point
    if (!key_info) return 1;
    
    u64 filetype = path_in ? IdentifyFileType(path_in) : 0;
    if (filetype & BIN_KEYDB) { // AES key database
        u32 fsize = fvx_qsize(path_in);
        if ((fsize % sizeof(AesKeyInfo)) || (fsize > MAX_KEYDB_SIZE))
            return 1;
        
        u32 n_keys = fsize / sizeof(AesKeyInfo);
        u32 merged_keys = 0;
    
        AesKeyInfo* key_info_merge = (AesKeyInfo*) malloc(fsize);
        if (fvx_qread(path_in, key_info_merge, 0, fsize, NULL) == FR_OK) {
            for (u32 i = 0; i < n_keys; i++) {
                if (key_info_merge[i].isEncrypted) // build an unencrypted db
                    CryptAesKeyInfo(&(key_info_merge[i]));
                if (AddKeyToDb(key_info, key_info_merge + i) != 0) break;
                merged_keys++;
            }
        }
        
        free(key_info_merge);
        if (merged_keys < n_keys) return 1;
    } else if (filetype & BIN_LEGKEY) { // legacy key file
        AesKeyInfo key;
        unsigned int keyslot = 0xFF;
        char typestr[32] = { 0 };
        char* name_in = strrchr(path_in, '/');
        
        memset(&key, 0, sizeof(AesKeyInfo));
        key.type = 'N';
        if (!name_in || (strnlen(++name_in, 32) > 24)) return 1; // safety
        if ((sscanf(name_in, "slot0x%02XKey%31s", &keyslot, typestr) != 2) &&
            (sscanf(name_in, "slot0x%02Xkey%31s", &keyslot, typestr) != 2)) return 1;
            
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
        if (fvx_qread(path_in, key.key, 0, 16, NULL) != FR_OK) return 1;
        if (AddKeyToDb(key_info, &key) != 0) return 1;
    }
    
    if (dump) {
        u32 dump_size = 0;
        for (AesKeyInfo* key = key_info; key->slot <= 0x40; key++) {
            u32 dump_size_next = dump_size + sizeof(AesKeyInfo);
            if (dump_size_next > MAX_KEYDB_SIZE) break;
            dump_size = dump_size_next;
        }
        
        if (dump_size) {
            if (fvx_rmkdir(OUTPUT_PATH) != FR_OK) // ensure the output dir exists
                return 1;
            f_unlink(path_out);
            if (fvx_qwrite(path_out, key_info, 0, dump_size, NULL) != FR_OK)
                return 1;
        }
        
        free(key_info);
        key_info = NULL;
    }
    
    return 0;
}
