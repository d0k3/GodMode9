#pragma once
#include "gm9lua.h"
#include "ui.h"
#include "fs.h"
#include "png.h"

#define GM9LUA_UILIBNAME "UI"

void ShiftOutputBufferUp(void);
void ClearOutputBuffer(void);
void RenderOutputBuffer(void);
void WriteToOutputBuffer(char* text);
int gm9lua_open_UI(lua_State* L);
