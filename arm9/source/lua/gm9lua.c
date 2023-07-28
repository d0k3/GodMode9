#include "gm9lua.h"
#include "ui.h"
#include "ff.h"
#include "vff.h"
#include "fsutil.h"
#include "gm9ui.h"
#include "gm9fs.h"
#include "gm9loader.h"

#define DEBUGSP ShowPrompt

typedef struct GM9LuaLoadF {
    int n; // pre-read characters
    FIL f;
    FRESULT res;
    char buff[BUFSIZ];
} GM9LuaLoadF;

// similar to "getF" in lauxlib.c
static const char* GetF(lua_State* L, void* ud, size_t* size) {
    GM9LuaLoadF* lf = (GM9LuaLoadF*)ud;
    UINT br = 0;
    (void)L; // unused
    if (lf->n > 0) { // check for pre-read characters
        *size = lf->n; // return those
        lf->n = 0;
    } else {
        if (fvx_eof(&lf->f)) return NULL;
        lf->res = fvx_read(&lf->f, lf->buff, BUFSIZ, &br);
        *size = (size_t)br;
        if (lf->res != FR_OK) return NULL;
    }
    return lf->buff;
}

// similar to "errfile" in lauxlib.c
static int ErrFile(lua_State* L, const char* what, int fnameindex, FRESULT res) {
    const char* filename = lua_tostring(L, fnameindex) + 1;
    lua_pushfstring(L, "cannot %s %s:\nfatfs error %d", what, filename, res);
    lua_remove(L, fnameindex);
    return LUA_ERRFILE;
}

int LoadLuaFile(lua_State* L, const char* filename) {
    GM9LuaLoadF lf;
    lf.n = 0;
    int status;
    int fnameindex = lua_gettop(L) + 1; // index of filename on the stack
    lua_pushfstring(L, "@%s", filename);
    lf.res = fvx_open(&lf.f, filename, FA_READ | FA_OPEN_EXISTING);
    if (lf.res != FR_OK) return ErrFile(L, "open", fnameindex, lf.res);
    
    status = lua_load(L, GetF, &lf, lua_tostring(L, -1), NULL);
    fvx_close(&lf.f);
    if (lf.res != FR_OK) {
        lua_settop(L, fnameindex);
        return ErrFile(L, "read", fnameindex, lf.res);
    }
    lua_remove(L, fnameindex);
    return status;
}

static const luaL_Reg gm9lualibs[] = {
    // built-ins
    {LUA_GNAME, luaopen_base},
    {LUA_LOADLIBNAME, luaopen_package},
    {LUA_COLIBNAME, luaopen_coroutine},
    {LUA_TABLIBNAME, luaopen_table},
    //{LUA_IOLIBNAME, luaopen_io},
    //{LUA_OSLIBNAME, luaopen_os},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {LUA_UTF8LIBNAME, luaopen_utf8},
    {LUA_DBLIBNAME, luaopen_debug},

    // gm9 custom
    {GM9LUA_UILIBNAME, gm9lua_open_UI},
    {GM9LUA_FSLIBNAME, gm9lua_open_FS},

    {NULL, NULL}
};

static int LuaShowPrompt(lua_State* L) {
    bool ask = lua_toboolean(L, 1);
    const char* text = lua_tostring(L, 2);

    bool ret = ShowPrompt(ask, "%s", text);
    lua_pushboolean(L, ret);
    return 1;
}

static void loadlibs(lua_State* L) {
    const luaL_Reg* lib;
    for (lib = gm9lualibs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1); // remove lib from stack
    }
}

bool ExecuteLuaScript(const char* path_script) {
    lua_State* L = luaL_newstate();
    loadlibs(L);

    ResetPackageSearchersAndPath(L);
    ClearOutputBuffer();

    //int result = luaL_loadbuffer(L, script_buffer, script_size, path_script);
    int result = LoadLuaFile(L, path_script);
    //free(script_buffer);
    if (result != LUA_OK) {
        char errstr[BUFSIZ] = {0};
        strlcpy(errstr, lua_tostring(L, -1), BUFSIZ);
        WordWrapString(errstr, 0);
        ShowPrompt(false, "Error during loading:\n%s", errstr);
        lua_close(L);
        return false;
    }

    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        char errstr[BUFSIZ] = {0};
        strlcpy(errstr, lua_tostring(L, -1), BUFSIZ);
        WordWrapString(errstr, 0);
        ShowPrompt(false, "Error during execution:\n%s", errstr);
        lua_close(L);
        return false;
    }

    lua_close(L);
    return true;
}
