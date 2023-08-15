#include "gm9lua.h"

#define GM9LUA_ENUMLIBNAME "Enum"
#define GLENUMITEM(what) {#what, what}

typedef struct EnumItem {
    const char *name;
    lua_Unsigned value;
} EnumItem;

void AddLuaEnumItems(lua_State* L, const char* name, const EnumItem* items);

int gm9lua_open_Enum(lua_State* L);
