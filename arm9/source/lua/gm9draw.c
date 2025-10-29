#ifndef NO_LUA

#include "gm9draw.h"
#include "common/ui.h"
#include "common/colors.h"

static int draw_pixel(lua_State *L) {
    CheckLuaArgCount(L, 4, "draw.pixel");
    u16* screen;
    
    int screenID = lua_tonumber(L, 1);
    if (screenID == 1) {
        screen = TOP_SCREEN;
    }
    else if (screenID == 2) {
        screen = BOT_SCREEN;
    }

    int x = lua_tonumber(L, 2);
    int y = lua_tonumber(L, 3);
    u32 color = lua_tonumber(L, 4);

    DrawPixel(screen, x, y, color);
    return 0;
}

static int draw_box(lua_State *L) {
    CheckLuaArgCount(L, 6, "draw.box");
    u16* screen;
    
    int screenID = lua_tonumber(L, 1);
    if (screenID == 1) {
        screen = TOP_SCREEN;
    }
    else if (screenID == 2) {
        screen = BOT_SCREEN;
    }

    int x = lua_tonumber(L, 2);
    int y = lua_tonumber(L, 3);
    u32 w = lua_tonumber(L, 4);
    u32 h = lua_tonumber(L, 5);
    u32 color = lua_tonumber(L, 6);

    DrawRectangle(screen, x, y, w, h, color);
    return 0;
}

static int draw_text(lua_State *L) {
    CheckLuaArgCount(L, 5, "draw.text");
    u16* screen;
    
    int screenID = lua_tonumber(L, 1);
    if (screenID == 1) {
        screen = TOP_SCREEN;
    }
    else if (screenID == 2) {
        screen = BOT_SCREEN;
    }

    const char* stringToDraw = lua_tostring(L, 2);
    int x = lua_tonumber(L, 3);
    int y = lua_tonumber(L, 4);
    u32 color = lua_tonumber(L, 5);

    DrawString(screen, stringToDraw, x, y, color, COLOR_STD_BG);
    return 0;
}

static int draw_rgb(lua_State *L) {
    CheckLuaArgCount(L, 3, "draw.rgb");

    u32 r = lua_tonumber(L, 1);
    u32 g = lua_tonumber(L, 2);
    u32 b = lua_tonumber(L, 3);

    return (int)RGB(r,g,b);
}

static const luaL_Reg draw[] = {
    {"pixel", draw_pixel},
    {"box", draw_box},
    {"text", draw_text},
    {"rgb", draw_rgb},
    {NULL, NULL}
};

int gm9lua_open_draw(lua_State* L) {
    luaL_newlib(L, draw);
    return 1;
}

#endif
