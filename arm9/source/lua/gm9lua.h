#pragma once
#include "common.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "scripting.h"

#define LUASCRIPT_EXT      "lua"
#define LUASCRIPT_MAX_SIZE STD_BUFFER_SIZE

#ifndef NO_LUA
static inline void CheckLuaArgCount(lua_State* L, int argcount, const char* cmd) {
    int args = lua_gettop(L);
    if (args != argcount) {
        luaL_error(L, "bad number of arguments passed to '%s' (expected %d, got %d)", cmd, argcount, args);
    }
}

int LoadLuaFile(lua_State* L, const char* filename);
#endif
bool ExecuteLuaScript(const char* path_script);
