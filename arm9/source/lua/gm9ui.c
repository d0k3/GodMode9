#ifndef NO_LUA
#include "gm9ui.h"
#include "gm9enum.h"

#define MAXOPTIONS     256
#define MAXOPTIONS_STR "256"

#define OUTPUTMAXLINES 24
#define OUTPUTMAXCHARSPERLINE 51 // make sure this includes space for '\0'

// this output buffer stuff is especially a test, it needs to take into account newlines and fonts that are not 8x10

char output_buffer[OUTPUTMAXLINES][OUTPUTMAXCHARSPERLINE]; // hold 24 lines

void ShiftOutputBufferUp(void) {
    for (int i = 0; i < OUTPUTMAXLINES - 1; i++) {
        memcpy(output_buffer[i], output_buffer[i + 1], OUTPUTMAXCHARSPERLINE);
    }
}

void ClearOutputBuffer(void) {
    memset(output_buffer, 0, sizeof(output_buffer));
}

void WriteToOutputBuffer(char* text) {
    strlcpy(output_buffer[OUTPUTMAXLINES - 1], text, OUTPUTMAXCHARSPERLINE);
}

void RenderOutputBuffer(void) {
    ClearScreenF(false, true, COLOR_STD_BG);
    for (int i = 0; i < OUTPUTMAXLINES; i++) {
        DrawString(ALT_SCREEN, output_buffer[i], 0, i * 10, COLOR_STD_FONT, COLOR_TRANSPARENT);
    }
}

static u16* GetScreenFromIndex(int index) {
    switch (index) {
        case 0: return MAIN_SCREEN;
        case 1: return ALT_SCREEN;
        case 2: return TOP_SCREEN;
        case 3: return BOT_SCREEN;
        default: return MAIN_SCREEN;
    }
}

static int UI_ShowPNG(lua_State* L) {
    CheckLuaArgCount(L, 2, "ShowPNG");
    lua_Integer which_screen = luaL_checknumber(L, 1);
    const char* path = lua_tostring(L, 2);
    u16* screen = GetScreenFromIndex(which_screen);
    u16 *bitmap = NULL;
    u8* png = (u8*) malloc(SCREEN_SIZE(screen));
    u32 bitmap_width, bitmap_height;
    if (png) {
        u32 png_size = FileGetData(path, png, SCREEN_SIZE(screen), 0);
        if (!png_size) {
            free(png);
            return luaL_error(L, "Could not read %s", path);
        }
        if (png_size && png_size < SCREEN_SIZE(screen)) {
            bitmap = PNG_Decompress(png, png_size, &bitmap_width, &bitmap_height);
            if (!bitmap) {
                free(png);
                return luaL_error(L, "Invalid PNG file");
            }
        }
        free(png);
        if (bitmap) {
            DrawBitmap(
                screen,                                 // screen
                (SCREEN_WIDTH(screen)-bitmap_width)/2,  // x coordinate calculated to be centered
                (SCREEN_HEIGHT-bitmap_height)/2,        // y coordinate calculated to be centered
                bitmap_width,                           // width
                bitmap_height,                          // height
                bitmap                                  // bitmap
                );
            free(bitmap);
        } else {
            return luaL_error(L, "PNG too large for console screen");
        }
    }
    return 0;
}

static int UI_ShowPrompt(lua_State* L) {
    CheckLuaArgCount(L, 1, "ShowPrompt");
    const char* text = lua_tostring(L, 1);

    ShowPrompt(false, "%s", text);
    return 0;
}

static int UI_AskPrompt(lua_State* L) {
    CheckLuaArgCount(L, 2, "AskPrompt");
    const char* text = lua_tostring(L, 2);

    bool ret = ShowPrompt(true, "%s", text);
    lua_pushboolean(L, ret);
    return 1;
}

static int UI_ShowString(lua_State* L) {
    CheckLuaArgCount(L, 2, "ShowString");
    lua_Integer screen = luaL_checknumber(L, 1);
    const char* text = lua_tostring(L, 2);

    ShowStringF(GetScreenFromIndex(screen), "%s", text);
    return 0;
}

static int UI_WordWrapString(lua_State* L) {
    size_t len;
    int isnum;
    int llen;
    int top = lua_gettop(L);
    if (top == 1) {
        llen = 0; // WordWrapString will automatically wrap it for the main screen
    } else if (top == 2) {
        llen = lua_tointegerx(L, 2, &isnum);
        if (llen == -1) {
            // special case for "word wrap for alt screen"
            llen = (SCREEN_WIDTH_ALT / GetFontWidth());
        }
    } else {
        return luaL_error(L, "bad number of arguments passed to WordWrapString (expected 1 or 2, got %d", top);
    }
    const char* text = lua_tolstring(L, 1, &len);
    char* buf = malloc(len + 1);
    strlcpy(buf, text, len + 1);
    WordWrapString(buf, llen);
    lua_pushlstring(L, buf, len);
    free(buf);
    return 1;
}

static int UI_ClearScreen(lua_State* L) {
    bool which_screen = luaL_checknumber(L, 1);
    u32 color = lua_tointeger(L, 2);
    ClearScreen(GetScreenFromIndex(which_screen), color);
    return 0;
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

static int UI_DrawString(lua_State* L) {
    int which_screen = lua_tointeger(L, 1);
    const char* text = lua_tostring(L, 2);
    int x = lua_tointeger(L, 3);
    int y = lua_tointeger(L, 4);
    u32 color = lua_tointeger(L, 5);
    u32 bgcolor = lua_tointeger(L, 6);
    u16* screen = GetScreenFromIndex(which_screen);
    DrawString(screen, text, x, y, color, bgcolor);
    return 0;
}

static int UIGlobal_Print(lua_State* L) {
    //const char* text = lua_tostring(L, 1);
    char buf[OUTPUTMAXCHARSPERLINE] = {0};
    int argcount = lua_gettop(L);
    for (int i = 0; i < lua_gettop(L); i++) {
        const char* str = luaL_tolstring(L, i+1, NULL);
        if (str) {
            strlcat(buf, str, OUTPUTMAXCHARSPERLINE);
            lua_pop(L, 1);
        } else {
            // idk
            strlcat(buf, "(unknown)", OUTPUTMAXCHARSPERLINE);
        }
        if (i < argcount) strlcat(buf, " ", OUTPUTMAXCHARSPERLINE);
    }
    ShiftOutputBufferUp();
    WriteToOutputBuffer((char*)buf);
    RenderOutputBuffer();
    return 0;
}

static const luaL_Reg UIlib[] = {
    {"ShowPrompt", UI_ShowPrompt},
    {"AskPrompt", UI_AskPrompt},
    {"ShowString", UI_ShowString},
    {"WordWrapString", UI_WordWrapString},
    {"ClearScreen", UI_ClearScreen},
    {"ShowSelectPrompt", UI_ShowSelectPrompt},
    {"ShowProgress", UI_ShowProgress},
    {"DrawString", UI_DrawString},
    {"DrawPNG", UI_ShowPNG},
    {NULL, NULL}
};

static const luaL_Reg UIGlobalLib[] = {
    {"print", UIGlobal_Print},
    {NULL, NULL}
};

static const EnumItem Enum_UI[] = {
    {"MainScreen", 0},
    {"AltScreen", 1},
    {"TopScreen", 2},
    {"BottomScreen", 3},
    {NULL, 0}
};

int gm9lua_open_UI(lua_State* L) {
    luaL_newlib(L, UIlib);
    AddLuaEnumItems(L, "UI", Enum_UI);
    lua_pushglobaltable(L); // push global table to stack
    luaL_setfuncs(L, UIGlobalLib, 0); // set global funcs
    lua_pop(L, 1); // pop global table from stack
    return 1;
}
#endif
