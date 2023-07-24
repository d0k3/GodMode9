#pragma once
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "common.h"
#include "scripting.h"

#define LUASCRIPT_EXT      "lua"
#define LUASCRIPT_MAX_SIZE STD_BUFFER_SIZE

void CheckLuaArgCount(lua_State* L, int argcount, const char* cmd);
int LoadLuaFile(lua_State* L, const char *filename);
bool ExecuteLuaScript(const char* path_script);
