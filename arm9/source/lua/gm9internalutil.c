#ifndef NO_LUA
#include "gm9internalutil.h"
#include "utf.h"
#include "fs.h"
#include "ui.h"

static int internalutil_utf16_to_utf8(lua_State* L) {
    CheckLuaArgCount(L, 1, "_util.utf16_to_utf8");
    size_t strInLength = 0;
    luaL_Buffer buffer;
    luaL_buffinit(L, &buffer);
    const u16* strIn = (const u16*)(void*)luaL_checklstring(L, 1, &strInLength);
    char* u8buf = luaL_prepbuffsize(&buffer, 2*strInLength); //assume a maximum of 4 utf8 codepoints per utf16 codepoint (2 bytes per utf16 codepoint so * 2)
    memset(u8buf, 0, 2*strInLength);
    int code_points = utf16_to_utf8((u8*)u8buf, strIn, 2*strInLength, strInLength / sizeof(u16)); //length is in bytes, prevent reading garbage by stopping properly because len_in is in codepoints
    luaL_addsize(&buffer, code_points*sizeof(u8));
    luaL_pushresult(&buffer);
    return 1;
}

static int internalutil_utf8_to_utf16(lua_State* L) {
    CheckLuaArgCount(L, 1, "_util.utf8_to_utf16");
    size_t strInLength = 0;
    luaL_Buffer buffer;
    luaL_buffinit(L, &buffer);
    const u8* strIn = (u8*)luaL_checklstring(L, 1, &strInLength);
    char* utf16buf = luaL_prepbuffsize(&buffer, sizeof(u16)*strInLength); //assume a maximum of 1 utf16 codepoint per utf8 codepoint
    memset(utf16buf, 0, sizeof(u16)*strInLength);
    int code_points = utf8_to_utf16((u16*)(void*)utf16buf, strIn, strInLength, strInLength); //strinlength is proper here because codepoint size is 1
    luaL_addsize(&buffer, code_points*sizeof(u16));
    luaL_pushresult(&buffer);
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