#ifndef NO_LUA
#include "gm9sys.h"
#include "bootfirm.h"
#include "fs.h"
#include "pxi.h"
#include "game.h"
#include "power.h"

static int sys_boot(lua_State* L) {
    CheckLuaArgCount(L, 1, "sys.boot");
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

static int sys_reboot(lua_State* L) {
    CheckLuaArgCount(L, 0, "sys.reboot");
    DeinitExtFS();
    DeinitSDCardFS();
    Reboot();
    return 0;
}

static int sys_power_off(lua_State* L) {
    CheckLuaArgCount(L, 0, "sys.power_off");
    DeinitExtFS();
    DeinitSDCardFS();
    PowerOff();
    return 0;
}

static const luaL_Reg sys_lib[] = {
    {"boot", sys_boot},
    {"reboot", sys_reboot},
    {"power_off", sys_power_off},
    {NULL, NULL}
};

int gm9lua_open_sys(lua_State* L) {
    luaL_newlib(L, sys_lib);
    return 1;
}
#endif
