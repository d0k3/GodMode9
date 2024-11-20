#ifndef NO_LUA
#include "gm9ui.h"

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

static int ui_echo(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.echo");
    const char* text = lua_tostring(L, 1);

    ShowPrompt(false, "%s", text);
    return 0;
}

static int ui_ask(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.ask");
    const char* text = lua_tostring(L, 1);

    bool ret = ShowPrompt(true, "%s", text);
    lua_pushboolean(L, ret);
    return 1;
}

static int ui_ask_hex(lua_State* L) {
    CheckLuaArgCount(L, 3, "ui.ask_hex");
    const char* text = lua_tostring(L, 1);
    u64 initial_hex = lua_tonumber(L, 2);
    u32 n_digits = lua_tonumber(L, 3);

    u64 ret = ShowHexPrompt(initial_hex, n_digits, "%s", text);
    if (ret == (u64) -1) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, ret);
    }
    return 1;
}

static int ui_ask_number(lua_State* L) {
    CheckLuaArgCount(L, 2, "ui.ask_number");
    const char* text = lua_tostring(L, 1);
    u64 initial_num = lua_tonumber(L, 2);

    u64 ret = ShowNumberPrompt(initial_num, "%s", text);
    if (ret == (u64) -1) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, ret);
    }
    return 1;

}

static int ui_ask_text(lua_State* L) {
    CheckLuaArgCount(L, 3, "ui.ask_text");
    const char* prompt = lua_tostring(L, 1);
    const char* _initial_text = lua_tostring(L, 2);
    u32 initial_text_size = strlen(_initial_text)+1;
    char initial_text[initial_text_size];
    snprintf(initial_text, initial_text_size, "%s", _initial_text);
    u32 max_size = lua_tonumber(L, 3);
    bool result = ShowKeyboardOrPrompt(initial_text, max_size, "%s", prompt);
    if (result)
        lua_pushstring(L, initial_text);
    else
        lua_pushnil(L);
    return 1;
}

static int ui_show_png(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.show_png");
    const char* path = lua_tostring(L, 1);
    u16 *screen = ALT_SCREEN;
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
        if (!bitmap) {
            return luaL_error(L, "PNG too large");
        } else if ((SCREEN_WIDTH(screen) < bitmap_width) || (SCREEN_HEIGHT < bitmap_height)) {
            free(bitmap);
            return luaL_error(L, "PNG too large");
        } else {
            DrawBitmap(
                screen,        // screen
                -1,       // x coordinate from argument
                -1,       // y coordinate from argument
                bitmap_width,  // width
                bitmap_height, // height
                bitmap         // bitmap
                );
            free(bitmap);
        }
    }
    return 0;
}

static int ui_show_text(lua_State* L) {
    CheckLuaArgCount(L, 1, "ui.show_text");
    const char* text = lua_tostring(L, 1);

    ShowStringF(ALT_SCREEN, "%s", text);
    return 0;
}

static int ui_ask_selection(lua_State* L) {
    CheckLuaArgCount(L, 2, "ui.ask_selection");
    const char* text = lua_tostring(L, 1);
    char* options[MAXOPTIONS];
    const char* tmpstr;
    size_t len;
    int i;
    
    luaL_argcheck(L, lua_istable(L, 2), 2, "table expected");

    lua_Integer opttablesize = luaL_len(L, 2);
    luaL_argcheck(L, opttablesize <= MAXOPTIONS, 2, "more than " MAXOPTIONS_STR " options given");
    for (i = 0; i < opttablesize; i++) {
        lua_geti(L, 2, i + 1);
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

static int ui_global_print(lua_State* L) {
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

static const luaL_Reg ui_lib[] = {
    {"echo", ui_echo},
    {"ask_hex", ui_ask_hex},
    {"ask_number", ui_ask_number},
    {"ask_text", ui_ask_text},
    {"ask", ui_ask},
    {"show_png", ui_show_png},
    {"show_text", ui_show_text},
    {"ask_selection", ui_ask_selection},
    {NULL, NULL}
};

static const luaL_Reg ui_global_lib[] = {
    {"print", ui_global_print},
    {NULL, NULL}
};

int gm9lua_open_ui(lua_State* L) {
    luaL_newlib(L, ui_lib);
    lua_pushglobaltable(L); // push global table to stack
    luaL_setfuncs(L, ui_global_lib, 0); // set global funcs
    lua_pop(L, 1); // pop global table from stack
    return 1;
}
#endif
