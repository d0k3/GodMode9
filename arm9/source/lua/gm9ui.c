#include "gm9ui.h"

static int UI_ShowPrompt(lua_State* L) {
    CheckLuaArgCount(L, 2, "ShowPrompt");
    bool ask = lua_toboolean(L, 1);
    const char* text = lua_tostring(L, 2);

    bool ret = ShowPrompt(ask, "%s", text);
    lua_pushboolean(L, ret);
    return 1;
}

static int UI_ShowSelectPrompt(lua_State* L) {
    CheckLuaArgCount(L, 2, "ShowSelectPrompt");
    const char* text = lua_tostring(L, 2);
    const char* options[16];
    
    luaL_argcheck(L, lua_istable(L, 1), 1, "table expected");

    lua_Integer opttablesize = luaL_len(L, 1);
    luaL_argcheck(L, opttablesize <= 16, 1, "more than 16 options given");
    for (int i = 0; i < opttablesize; i++) {
        lua_geti(L, 1, i + 1);
        options[i] = lua_tostring(L, -1);
    }
    int result = ShowSelectPrompt(opttablesize, options, "%s", text);
    lua_pushinteger(L, result);
    return 1;
}

static const luaL_Reg UIlib[] = {
    {"ShowPrompt", UI_ShowPrompt},
    {"ShowSelectPrompt", UI_ShowSelectPrompt},
    {NULL, NULL}
};

int gm9lua_open_UI(lua_State* L) {
    luaL_newlib(L, UIlib);
    return 1;
}
