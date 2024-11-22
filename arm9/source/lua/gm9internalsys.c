#ifndef NO_LUA
#include "gm9internalsys.h"
#include "bootfirm.h"
#include "fs.h"
#include "pxi.h"
#include "game.h"
#include "power.h"
#include "sha.h"

static int internalsys_boot(lua_State* L) {
    CheckLuaArgCount(L, 1, "_sys.boot");
    const char* path = lua_tostring(L, 1);

    u8* firm = (u8*) malloc(FIRM_MAX_SIZE);
    if (!firm) {
        return luaL_error(L, STR_SCRIPTERR_OUT_OF_MEMORY);
    }

    size_t firm_size = FileGetData(path, firm, FIRM_MAX_SIZE, 0);
    if (!(firm_size && IsBootableFirm(firm, firm_size))) {
        return luaL_error(L, STR_SCRIPTERR_NOT_A_BOOTABLE_FIRM);
    }

    char fixpath[256] = { 0 };
    if ((*path == '0') || (*path == '1'))
        snprintf(fixpath, sizeof(fixpath), "%s%s", (*path == '0') ? "sdmc" : "nand", path + 1);
    else strncpy(fixpath, path, 256);
    fixpath[255] = '\0';
    DeinitExtFS();
    DeinitSDCardFS();
    PXI_DoCMD(PXICMD_LEGACY_BOOT, NULL, 0);
    PXI_Barrier(PXI_FIRMLAUNCH_BARRIER);
    BootFirm((FirmHeader*)(void*)firm, fixpath);
    while(1);
}

static int internalsys_reboot(lua_State* L) {
    CheckLuaArgCount(L, 0, "_sys.reboot");
    DeinitExtFS();
    DeinitSDCardFS();
    Reboot();
    return 0;
}

static int internalsys_power_off(lua_State* L) {
    CheckLuaArgCount(L, 0, "_sys.power_off");
    DeinitExtFS();
    DeinitSDCardFS();
    PowerOff();
    return 0;
}

static int internalsys_get_id0(lua_State* L) {
    CheckLuaArgCount(L, 1, "_sys.get_id0");
    const char* path = lua_tostring(L, 1);

    char env_id0[32+1];
    u8 sd_keyy[0x10] __attribute__((aligned(4)));
    if (FileGetData(path, sd_keyy, 0x10, 0x110) == 0x10) {
        u32 sha256sum[8];
        sha_quick(sha256sum, sd_keyy, 0x10, SHA256_MODE);
        snprintf(env_id0, sizeof(env_id0), "%08lx%08lx%08lx%08lx",
            sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
        lua_pushstring(L, env_id0);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static const luaL_Reg internalsys_lib[] = {
    {"boot", internalsys_boot},
    {"reboot", internalsys_reboot},
    {"power_off", internalsys_power_off},
    {"get_id0", internalsys_get_id0},
    {NULL, NULL}
};

int gm9lua_open_internalsys(lua_State* L) {
    luaL_newlib(L, internalsys_lib);
    return 1;
}
#endif
