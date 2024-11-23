#ifndef NO_LUA
#include "gm9fs.h"
#include "fs.h"
#include "ui.h"
#include "utils.h"
#include "sha.h"
#include "nand.h"
#include "language.h"
#include "hid.h"

static u8 no_data_hash_256[32] = { SHA256_EMPTY_HASH };
static u8 no_data_hash_1[32] = { SHA1_EMPTY_HASH };

static bool PathIsDirectory(const char* path) {
    DIR fdir;
    if (fvx_opendir(&fdir, path) == FR_OK) {
        fvx_closedir(&fdir);
        return true;
    } else {
        return false;
    }
}

static void CreateStatTable(lua_State* L, FILINFO* fno) {
    lua_createtable(L, 0, 4); // create nested table
    lua_pushstring(L, fno->fname);
    lua_setfield(L, -2, "name");
    lua_pushstring(L, (fno->fattrib & AM_DIR) ? "dir" : "file");
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, fno->fsize);
    lua_setfield(L, -2, "size");
    lua_pushboolean(L, fno->fattrib & AM_RDO);
    lua_setfield(L, -2, "read_only");
    // ... and leave this table on the stack for the caller to deal with
}

static u32 GetFlagsFromTable(lua_State* L, int pos, u32 flags_ext_starter, u32 allowed_flags) {
    char types[FLAGS_COUNT][14] = { FLAGS_STR };
    int types_int[FLAGS_COUNT] = { FLAGS_CONSTS };
    u32 flags_ext = flags_ext_starter;

    for (int i = 0; i < FLAGS_COUNT; i++) {
        if (!(allowed_flags & types_int[i])) continue;
        lua_getfield(L, pos, types[i]);
        if (lua_toboolean(L, -1)) flags_ext |= types_int[i];
        lua_pop(L, 1);
    }

    return flags_ext;
}

static int fs_move(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 2, "fs.rename");
    const char* path_src = luaL_checkstring(L, 1);
    const char* path_dst = luaL_checkstring(L, 2);
    FILINFO fno;

    u32 flags = BUILD_PATH;
    if (extra) {
        flags = GetFlagsFromTable(L, 3, flags, NO_CANCEL | SILENT | OVERWRITE_ALL | SKIP_ALL);
    }

    if (!(flags & OVERWRITE_ALL) && (fvx_stat(path_dst, &fno) == FR_OK)) {
        return luaL_error(L, "destination already exists on %s -> %s and {overwrite_all=true} was not used", path_src, path_dst);
    }

    if (!(PathMoveCopy(path_dst, path_src, &flags, true))) {
        return luaL_error(L, "PathMoveCopy failed on %s -> %s", path_src, path_dst);
    }
    return 0;
}

static int fs_remove(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "fs.remove");
    const char* path = luaL_checkstring(L, 1);

    bool allowed = CheckWritePermissions(path);
    if (!allowed) {
        return luaL_error(L, "writing not allowed: %s", path);
    }

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, flags, RECURSIVE);
    }

    if (!(flags & RECURSIVE)) {
        if (PathIsDirectory(path)) {
            return luaL_error(L, "requested directory remove without {recursive=true} on %s", path);
        }
    }

    if (!(PathDelete(path))) {
        return luaL_error(L, "PathDelete failed on %s", path);
    }
    
    return 0;
}

static int fs_copy(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 2, "fs.copy");
    const char* path_src = luaL_checkstring(L, 1);
    const char* path_dst = luaL_checkstring(L, 2);
    FILINFO fno;

    bool allowed = CheckWritePermissions(path_dst);
    if (!allowed) {
        return luaL_error(L, "writing not allowed: %s", path_dst);
    }

    u32 flags = BUILD_PATH;
    if (extra) {
        flags = GetFlagsFromTable(L, 3, flags, CALC_SHA | USE_SHA1 | NO_CANCEL | SILENT | OVERWRITE_ALL | SKIP_ALL | APPEND_ALL | RECURSIVE);
    }

    if (!(flags & RECURSIVE)) {
        if (PathIsDirectory(path_src)) {
            return luaL_error(L, "requested directory copy without {recursive=true} on %s -> %s", path_src, path_dst);
        }
    }

    if (!(flags & OVERWRITE_ALL) && (fvx_stat(path_dst, &fno) == FR_OK)) {
        return luaL_error(L, "destination already exists on %s -> %s and {overwrite_all=true} was not used", path_src, path_dst);
    }

    if (!(PathMoveCopy(path_dst, path_src, &flags, false))) {
        return luaL_error(L, "PathMoveCopy failed on %s -> %s", path_src, path_dst);
    }

    return 0;
}

static int fs_mkdir(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.mkdir");
    const char* path = luaL_checkstring(L, 1);

    bool allowed = CheckWritePermissions(path);
    if (!allowed) {
        return luaL_error(L, "writing not allowed: %s", path);
    }

    FRESULT res = fvx_rmkdir(path);
    if (res != FR_OK) {
        return luaL_error(L, "could not mkdir (%d)", path, res);
    }

    return 0;
}

static int fs_list_dir(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.list_dir");
    const char* path = luaL_checkstring(L, 1);
    lua_newtable(L);

    DIR dir;
    FILINFO fno;

    FRESULT res = fvx_opendir(&dir, path);
    if (res != FR_OK) {
        lua_pop(L, 1); // remove final table from stack
        return luaL_error(L, "could not opendir %s (%d)", path, res);
    }

    for (int i = 1; true; i++) {
        res = fvx_readdir(&dir, &fno);
        if (res != FR_OK) {
            lua_pop(L, 1); // remove final table from stack
            return luaL_error(L, "could not readdir %s (%d)", path, res);
        }
        if (fno.fname[0] == 0) break;
        CreateStatTable(L, &fno);
        lua_seti(L, -2, i); // add nested table to final table
    }

    return 1;
}

static int fs_stat(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.stat");
    const char* path = luaL_checkstring(L, 1);
    FILINFO fno;

    FRESULT res = fvx_stat(path, &fno);
    if (res != FR_OK) {
        return luaL_error(L, "could not stat %s (%d)", path, res);
    }
    CreateStatTable(L, &fno);
    return 1;
}

static int fs_fix_cmacs(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.fix_cmacs");
    const char* path = luaL_checkstring(L, 1);

    ShowString("%s", STR_FIXING_CMACS_PLEASE_WAIT);
    if (RecursiveFixFileCmac(path) != 0) {
        return luaL_error(L, "%s", STR_SCRIPTERR_FIXCMAC_FAILED);
    }

    return 0;
}

static int fs_stat_fs(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.stat_fs");
    const char* path = luaL_checkstring(L, 1);

    u64 freespace = GetFreeSpace(path);
    u64 totalspace = GetTotalSpace(path);
    u64 usedspace = totalspace - freespace;

    lua_createtable(L, 0, 3);
    lua_pushinteger(L, freespace);
    lua_setfield(L, -2, "free");
    lua_pushinteger(L, totalspace);
    lua_setfield(L, -2, "total");
    lua_pushinteger(L, usedspace);
    lua_setfield(L, -2, "used");

    return 1;
}

static int fs_dir_info(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.stat_fs");
    const char* path = luaL_checkstring(L, 1);

    u64 tsize = 0;
    u32 tdirs = 0;
    u32 tfiles = 0;
    if (!DirInfo(path, &tsize, &tdirs, &tfiles)) {
        return luaL_error(L, "error when running DirInfo");
    }

    lua_createtable(L, 0, 3);
    lua_pushinteger(L, tsize);
    lua_setfield(L, -2, "size");
    lua_pushinteger(L, tdirs);
    lua_setfield(L, -2, "dirs");
    lua_pushinteger(L, tfiles);
    lua_setfield(L, -2, "files");

    return 1;
}

static int fs_exists(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.exists");
    const char* path = luaL_checkstring(L, 1);
    FILINFO fno;

    lua_pushboolean(L, (fvx_stat(path, &fno) == FR_OK));
    return 1;
}

static int fs_is_dir(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.is_dir");
    const char* path = luaL_checkstring(L, 1);

    lua_pushboolean(L, PathIsDirectory(path));
    return 1;
}

static int fs_is_file(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.is_file");
    const char* path = luaL_checkstring(L, 1);
    FILINFO fno;

    FRESULT res = fvx_stat(path, &fno);
    if (res != FR_OK) {
        lua_pushboolean(L, false);
    } else {
        lua_pushboolean(L, !(fno.fattrib & AM_DIR));
    }
    return 1;
}

static int fs_read_file(lua_State* L) {
    CheckLuaArgCount(L, 3, "fs.read_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer offset = luaL_checkinteger(L, 2);
    lua_Integer size = luaL_checkinteger(L, 3);

    char *buf = malloc(size);
    if (!buf) {
        return luaL_error(L, "could not allocate memory to read file");
    }
    UINT bytes_read = 0;
    FRESULT res = fvx_qread(path, buf, offset, size, &bytes_read);
    if (res != FR_OK) {
        free(buf);
        return luaL_error(L, "could not read %s (%d)", path, res);
    }
    lua_pushlstring(L, buf, bytes_read);
    free(buf);
    return 1;
}

static int fs_write_file(lua_State* L) {
    CheckLuaArgCount(L, 3, "fs.write_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer offset = luaL_checkinteger(L, 2);
    size_t data_length = 0;
    const char* data = luaL_checklstring(L, 3, &data_length);

    bool allowed = CheckWritePermissions(path);
    if (!allowed) {
        return luaL_error(L, "writing not allowed: %s", path);
    }
    
    UINT bytes_written = 0;
    FRESULT res = fvx_qwrite(path, data, offset, data_length, &bytes_written);
    if (res != FR_OK) {
        return luaL_error(L, "error writing %s (%d)", path, res);
    }

    lua_pushinteger(L, bytes_written);
    return 1;
}

static int fs_truncate(lua_State* L) {
    CheckLuaArgCount(L, 2, "fs.write_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer size = luaL_checkinteger(L, 2);
    FIL fp;
    FRESULT res;

    res = f_open(&fp, path, FA_READ | FA_WRITE);
    if (res != FR_OK) {
        return luaL_error(L, "failed to open %s (note: this only works on FAT filesystems, not virtual)", path);
    }

    // this check is *after* opening so the error happens on virtual filesystems sooner
    bool allowed = CheckWritePermissions(path);
    if (!allowed) {
        f_close(&fp);
        return luaL_error(L, "writing not allowed: %s", path);
    }

    res = f_lseek(&fp, size);
    if (res != FR_OK) {
        f_close(&fp);
        return luaL_error(L, "failed to seek on %s", path);
    }

    res = f_truncate(&fp);
    if (res != FR_OK) {
        f_close(&fp);
        return luaL_error(L, "failed to truncate %s", path);
    }

    f_close(&fp);
    return 0;
}

static int fs_img_mount(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.img_mount");
    const char* path = luaL_checkstring(L, 1);

    bool res = InitImgFS(path);
    if (!res) {
        return luaL_error(L, "failed to mount %s", path);
    }

    return 0;
}

static int fs_img_umount(lua_State* L) {
    CheckLuaArgCount(L, 0, "fs.img_umount");
    
    InitImgFS(NULL);
    
    return 0;
}

static int fs_get_img_mount(lua_State* L) {
    CheckLuaArgCount(L, 0, "fs.img_umount");

    char path[256] = { 0 };
    strncpy(path, GetMountPath(), 256);
    if (path[0] == 0) {
        // since lua treats "" as true, return a nil to make if/else easier
        lua_pushnil(L);
    } else {
        lua_pushstring(L, path);
    }

    return 1;
}

static int fs_hash_file(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 3, "fs.hash_file");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer offset = luaL_checkinteger(L, 2);
    lua_Integer size = luaL_checkinteger(L, 3);
    FRESULT res;
    FILINFO fno;

    if (size == 0) {
        res = fvx_stat(path, &fno);
        if (res != FR_OK) {
            return luaL_error(L, "failed to stat %s", path);
        }

        size = fno.fsize;
    }

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 4, flags, USE_SHA1);
    }

    const u8 hashlen = (flags & USE_SHA1) ? 20 : 32;
    u8 hash_fil[0x20];

    if (size == 0) {
        // shortcut by just returning the hash of empty data
        memcpy(hash_fil, (flags & USE_SHA1) ? no_data_hash_1 : no_data_hash_256, hashlen);
    } else if (!(FileGetSha(path, hash_fil, offset, size, (flags & USE_SHA1)))) {
        return luaL_error(L, "FileGetSha failed on %s", path);
    }

    lua_pushlstring(L, (char*)hash_fil, hashlen);
    return 1;
}

static int fs_hash_data(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "fs.hash_data");
    size_t data_length = 0;
    const char* data = luaL_checklstring(L, 1, &data_length);

    u32 flags = 0;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, flags, USE_SHA1);
    }

    const u8 hashlen = (flags & USE_SHA1) ? 20 : 32;
    u8 hash_fil[0x20];

    if (data_length == 0) {
        // shortcut by just returning the hash of empty data
        memcpy(hash_fil, (flags & USE_SHA1) ? no_data_hash_1 : no_data_hash_256, hashlen);
    } else {
        sha_quick(hash_fil, data, data_length, (flags & USE_SHA1) ? SHA1_MODE : SHA256_MODE);
    }

    lua_pushlstring(L, (char*)hash_fil, hashlen);
    return 1;
}

static int fs_allow(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 1, "fs.img_mount");
    const char* path = luaL_checkstring(L, 1);
    u32 flags = 0;
    bool allowed;
    if (extra) {
        flags = GetFlagsFromTable(L, 2, 0, ASK_ALL);
    }

    if (flags & ASK_ALL) {
        allowed = CheckDirWritePermissions(path);
    } else {
        allowed = CheckWritePermissions(path);
    }
    lua_pushboolean(L, allowed);
    return 1;
};

static int fs_verify(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.verify");
    const char* path = luaL_checkstring(L, 1);
    bool res;

    u64 filetype = IdentifyFileType(path);
    if (filetype & IMG_NAND) res = (ValidateNandDump(path) == 0);
    else res = (VerifyGameFile(path) == 0);

    lua_pushboolean(L, res);
    return 1;
}

static int fs_sd_is_mounted(lua_State* L) {
    CheckLuaArgCount(L, 0, "fs.sd_is_mounted");

    lua_pushboolean(L, CheckSDMountState());
    return 1;
}

static int fs_sd_switch(lua_State* L) {
    bool extra = CheckLuaArgCountPlusExtra(L, 0, "fs.sd_switch");
    const char* message;

    if (extra) {
        message = luaL_checkstring(L, 1);
    } else {
        message = "Please switch the SD card now.";
    }

    bool ret;

    DeinitExtFS();
    if (!(ret = CheckSDMountState())) {
        return luaL_error(L, "%s", STR_SCRIPTERR_SD_NOT_MOUNTED);
    }

    u32 pad_state;
    DeinitSDCardFS();
    ShowString("%s\n \n%s", message, STR_EJECT_SD_CARD);
    while (!((pad_state = InputWait(0)) & (BUTTON_B|SD_EJECT)));
    if (pad_state & SD_EJECT) {
        ShowString("%s\n \n%s", message, STR_INSERT_SD_CARD);
        while (!((pad_state = InputWait(0)) & (BUTTON_B|SD_INSERT)));
    }
    if (pad_state & BUTTON_B) {
        return luaL_error(L, "%s", STR_SCRIPTERR_USER_ABORT);
    }

    InitSDCardFS();
    AutoEmuNandBase(true);
    InitExtFS();

    return 0;
}

static const luaL_Reg fs_lib[] = {
    {"move", fs_move},
    {"remove", fs_remove},
    {"copy", fs_copy},
    {"mkdir", fs_mkdir},
    {"list_dir", fs_list_dir},
    {"stat", fs_stat},
    {"stat_fs", fs_stat_fs},
    {"dir_info", fs_dir_info},
    {"exists", fs_exists},
    {"is_dir", fs_is_dir},
    {"is_file", fs_is_file},
    {"read_file", fs_read_file},
    {"write_file", fs_write_file},
    {"truncate", fs_truncate},
    {"img_mount", fs_img_mount},
    {"img_umount", fs_img_umount},
    {"get_img_mount", fs_get_img_mount},
    {"hash_file", fs_hash_file},
    {"hash_data", fs_hash_data},
    {"verify", fs_verify},
    {"allow", fs_allow},
    {"sd_is_mounted", fs_sd_is_mounted},
    {"sd_switch", fs_sd_switch},
    {"fix_cmacs", fs_fix_cmacs},
    {NULL, NULL}
};

int gm9lua_open_fs(lua_State* L) {
    luaL_newlib(L, fs_lib);
    return 1;
}
#endif
