#ifndef NO_LUA
#include "gm9internalutil.h"
#include "utf.h"
#include "fs.h"
#include "ui.h"

static int internalutil_utf16_to_utf8(lua_State* L) {
    CheckLuaArgCount(L, 1, "_util.utf16_to_utf8");
    size_t strInLength = 0;
    const u16* strIn = (const u16*)(void*)luaL_checklstring(L, 1, &strInLength);
    u8 buffer[2*strInLength]; //assume a maximum of 4 utf8 codepoints per utf16 codepoint (2 bytes per utf16 codepoint so * 2)
    int code_points = utf16_to_utf8(buffer, strIn, 2*strInLength, strInLength);
    lua_pushlstring(L, (char*)buffer, code_points);
    return 1;
}

static int internalutil_utf8_to_utf16(lua_State* L) {
    CheckLuaArgCount(L, 1, "_util.utf8_to_utf16");
    size_t strInLength = 0;
    const u8* strIn = (u8*)luaL_checklstring(L, 1, &strInLength);
    u16 buffer[strInLength]; //assume a maximum of 1 utf16 codepoint per utf8 codepoint
    int code_points = utf8_to_utf16(buffer, strIn, strInLength, strInLength); 
    lua_pushlstring(L, (char*)buffer, 2*code_points);
    return 1;
}

static const luaL_Reg util[] = {
    {"utf16_to_utf8", internalutil_utf16_to_utf8},
    {"utf8_to_utf16", internalutil_utf8_to_utf16},
    {NULL, NULL}
};

int gm9lua_open_internalutil(lua_State* L) {
    luaL_newlib(L, util);
    return 1;
}

#endif