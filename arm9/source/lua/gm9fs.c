#ifndef NO_LUA
#include "gm9fs.h"
#include "fs.h"
#include "ui.h"
#include "utils.h"

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
    char types[8][14] = {"no_cancel", "silent", "hash", "sha1", "skip", "overwrite", "append_all", "all"};
    int types_int[8] = {NO_CANCEL, SILENT, CALC_SHA, USE_SHA1, SKIP_ALL, OVERWRITE_ALL, APPEND_ALL, ASK_ALL};
    u32 flags_ext = flags_ext_starter;

    for (int i = 0; i < 8; i++) {
        if (!(allowed_flags & types_int[i])) continue;
        lua_getfield(L, pos, types[i]);
        if (lua_toboolean(L, -1)) flags_ext |= types_int[i];
        lua_pop(L, 1);
    }

    return flags_ext;
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

    FRESULT res = fvx_stat(path, &fno);
    lua_pushboolean(L, res == FR_OK);
    return 1;
}

static int fs_is_dir(lua_State* L) {
    CheckLuaArgCount(L, 1, "fs.is_dir");
    const char* path = luaL_checkstring(L, 1);
    FILINFO fno;

    FRESULT res = fvx_stat(path, &fno);
    if (res != FR_OK) {
        lua_pushboolean(L, false);
    } else {
        lua_pushboolean(L, fno.fattrib & AM_DIR);
    }
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

static int fs_allow(lua_State* L) {
    int extra = CheckLuaArgCountPlusExtra(L, 1, "fs.img_mount");
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
    bool res = true;

    u64 filetype = IdentifyFileType(path);
    if (filetype & IMG_NAND) res = (ValidateNandDump(path) == 0);
    else res = (VerifyGameFile(path) == 0);

    lua_pushboolean(L, res);
    return 1;
}

static const luaL_Reg fs_lib[] = {
    {"list_dir", fs_list_dir},
    {"stat", fs_stat},
    {"stat_fs", fs_stat_fs},
    {"dir_info", fs_dir_info},
    {"exists", fs_exists},
    {"is_dir", fs_is_dir},
    {"is_file", fs_is_file},
    {"read_file", fs_read_file},
    {"write_file", fs_write_file},
    {"img_mount", fs_img_mount},
    {"img_umount", fs_img_umount},
    {"get_img_mount", fs_get_img_mount},
    {"allow", fs_allow},
    {NULL, NULL}
};

int gm9lua_open_fs(lua_State* L) {
    luaL_newlib(L, fs_lib);
    return 1;
}
#endif
