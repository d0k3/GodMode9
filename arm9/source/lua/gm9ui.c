#include "gm9ui.h"

#define MAXOPTIONS     256
#define MAXOPTIONS_STR "256"

static int UI_ShowPrompt(lua_State* L) {
    CheckLuaArgCount(L, 2, "ShowPrompt");
    bool ask = lua_toboolean(L, 1);
    const char* text = lua_tostring(L, 2);

    bool ret = ShowPrompt(ask, "%s", text);
    lua_pushboolean(L, ret);
    return 1;
}

static int UI_ShowString(lua_State* L) {
    CheckLuaArgCount(L, 1, "ShowString");
    const char *text = lua_tostring(L, 1);

    ShowString("%s", text);
    return 0;
}

static int UI_WordWrapString(lua_State* L) {
    size_t len;
    int isnum;
    const char *text = lua_tolstring(L, 1, &len);
    int llen = lua_tointegerx(L, 2, &isnum);
    // i should check arg 2 if it's a number (but only if it was provided at all)
    char* buf = malloc(len + 1);
    strlcpy(buf, text, len + 1);
    WordWrapString(buf, llen);
    lua_pushlstring(L, buf, len);
    free(buf);
    return 1;
}

static int UI_ShowSelectPrompt(lua_State* L) {
    CheckLuaArgCount(L, 2, "ShowSelectPrompt");
    const char* text = lua_tostring(L, 2);
    char* options[MAXOPTIONS];
    const char* tmpstr;
    size_t len;
    int i;
    
    luaL_argcheck(L, lua_istable(L, 1), 1, "table expected");

    lua_Integer opttablesize = luaL_len(L, 1);
    luaL_argcheck(L, opttablesize <= MAXOPTIONS, 1, "more than " MAXOPTIONS_STR " options given");
    for (i = 0; i < opttablesize; i++) {
        lua_geti(L, 1, i + 1);
        tmpstr = lua_tolstring(L, -1, &len);
        options[i] = malloc(len + 1);
        strlcpy(options[i], tmpstr, len + 1);
        lua_pop(L, 1);
    }
    int result = ShowSelectPrompt(opttablesize, (const char**)options, "%s", text);
    for (i = 0; i < opttablesize; i++) free(options[i]);
    // lua only treats "false" and "nil" as false values
    // so to make this easier, return nil and not 0 if no choice was made
    // https://www.lua.org/manual/5.4/manual.html#3.3.4
    if (result)
        lua_pushinteger(L, result);
    else
        lua_pushnil(L);
    return 1;
}

static int UI_ShowProgress(lua_State* L) {
    CheckLuaArgCount(L, 3, "ShowProgress");
    u64 current = lua_tointeger(L, 1);
    u64 total = lua_tointeger(L, 2);
    const char* optstr = lua_tostring(L, 3);

    bool result = ShowProgress(current, total, optstr);
    lua_pushboolean(L, result);
    return 1;
}

static const luaL_Reg UIlib[] = {
    {"ShowPrompt", UI_ShowPrompt},
    {"ShowString", UI_ShowString},
    {"WordWrapString", UI_WordWrapString},
    {"ShowSelectPrompt", UI_ShowSelectPrompt},
    {"ShowProgress", UI_ShowProgress},
    {NULL, NULL}
};

int gm9lua_open_UI(lua_State* L) {
    luaL_newlib(L, UIlib);
    return 1;
}
