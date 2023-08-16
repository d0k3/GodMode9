#ifndef NO_LUA
#include "gm9enum.h"

void AddLuaEnumItems(lua_State* L, const char* name, const EnumItem* items) {
    lua_getglobal(L, GM9LUA_ENUMLIBNAME); // stack: 1
    // this should probably use mua_createtable to pre-allocate the size
    lua_newtable(L); // stack: 2
    const EnumItem* e;
    for (e = items; e->name; e++) {
        lua_pushinteger(L, e->value); // stack: 3
        lua_setfield(L, -2, e->name); // stack: 2
    }
    lua_setfield(L, -2, name); // stack: 1
    lua_pop(L, 1); // stack: 0
}

int gm9lua_open_Enum(lua_State* L) {
    lua_newtable(L);
    return 1;
}
#endif
