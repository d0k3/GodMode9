#include "gm9fs.h"
#include "fs.h"
#include "vff.h"
#include "gm9enum.h"

static int FS_InitImgFS(lua_State* L) {
    CheckLuaArgCount(L, 1, "InitImgFS");

    const char* path;
    if (lua_isnil(L, 1)) {
        path = NULL;
    } else {
        path = lua_tostring(L, 1);
        luaL_argcheck(L, path, 1, "string or nil expected");
    }

    bool ret = InitImgFS(path);
    lua_pushboolean(L, ret);
    return 1;
}

static int FS_FileGetData(lua_State* L) {
    CheckLuaArgCount(L, 3, "FileGetData");
    const char* path = luaL_checkstring(L, 1);
    lua_Integer size = lua_tointeger(L, 2);
    lua_Integer offset = lua_tointeger(L, 3);
    if (size == -1) size = STD_BUFFER_SIZE;
    else if (size == 0) return luaL_error(L, "size cannot be 0");
    else if (size > STD_BUFFER_SIZE) return luaL_error(L, "size cannot be above %I (STD_BUFFER_SIZE)", STD_BUFFER_SIZE);

    void* buf = malloc(size);
    if (!buf) return luaL_error(L, "could not allocate buffer");
    // instead of using FileGetData directly we can use fvx_qread and handle the result
    // and return a nil if it works (and an empty string if it really is empty)
    UINT br = 0;
    FRESULT res = fvx_qread(path, buf, offset, size, &br);
    if (res != FR_OK) {
        luaL_pushfail(L);
        return 1;
    }
    lua_pushlstring(L, buf, br);
    free(buf);
    return 1;
}

static const luaL_Reg FSlib[] = {
    {"InitImgFS", FS_InitImgFS},
    {"FileGetData", FS_FileGetData},
    {NULL, NULL}
};

static const EnumItem Enum_FS[] = {
    GLENUMITEM(T_ROOT),
    GLENUMITEM(T_DIR),
    GLENUMITEM(T_FILE),
    GLENUMITEM(T_DOTDOT),
    GLENUMITEM(PERM_SDCARD),
    GLENUMITEM(PERM_IMAGE),
    GLENUMITEM(PERM_RAMDRIVE),
    GLENUMITEM(PERM_EMU_LVL0),
    GLENUMITEM(PERM_EMU_LVL1),
    GLENUMITEM(PERM_SYS_LVL0),
    GLENUMITEM(PERM_SYS_LVL1),
    GLENUMITEM(PERM_SYS_LVL2),
    GLENUMITEM(PERM_SYS_LVL3),
    GLENUMITEM(PERM_SDDATA),
    GLENUMITEM(PERM_MEMORY),
    GLENUMITEM(PERM_GAME),
    GLENUMITEM(PERM_XORPAD),
    GLENUMITEM(PERM_CART),
    GLENUMITEM(PERM_VRAM),
    GLENUMITEM(PERM_BASE),
    {NULL, 0}
};

int gm9lua_open_FS(lua_State* L) {
    luaL_newlib(L, FSlib);
    AddLuaEnumItems(L, "FS", Enum_FS);
    return 1;
}
