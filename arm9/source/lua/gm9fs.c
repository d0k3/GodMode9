#include "gm9fs.h"

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

static const luaL_Reg FSlib[] = {
    {"InitImgFS", FS_InitImgFS},
    {NULL, NULL}
};

int gm9lua_open_FS(lua_State* L) {
    luaL_newlib(L, FSlib);
    return 1;
}
