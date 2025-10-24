#ifndef NO_LUA
#include "gm9i2c.h"
#include "system/i2c.h"

// Helper function to validate I2C device ID against enum values
static bool is_valid_i2c_device(int devId) {
    switch (devId) {
        case I2C_DEV_POWER:
        case I2C_DEV_CAMERA:
        case I2C_DEV_CAMERA2:
        case I2C_DEV_MCU:
        case I2C_DEV_GYRO:
        case I2C_DEV_DEBUG_PAD:
        case I2C_DEV_IR:
        case I2C_DEV_EEPROM:
        case I2C_DEV_NFC:
        case I2C_DEV_QTM:
        case I2C_DEV_N3DS_HID:
            return true;
        default:
            return false;
    }
}

static int i2c_read(lua_State *L) {
    // Get parameters from Lua: device ID, register address, length
    int devId = luaL_checkinteger(L, 1);
    int regAddr = luaL_checkinteger(L, 2);
    int length = luaL_checkinteger(L, 3);

    // Validate device ID against I2cDevice enum
    if (!is_valid_i2c_device(devId)) {
        return luaL_error(L, "Invalid device ID: %d (valid IDs: 0-3, 10, 12-17)", devId);
    }

    // Validate parameters
    if (length <= 0 || length > 1024) {
        return luaL_error(L, "Invalid length: %d (must be 1-1024)", length);
    }

    if (regAddr < 0 || regAddr > 255) {
        return luaL_error(L, "Invalid register address: %d (must be 0-255)", regAddr);
    }

    // Create a buffer for the read data
    u8 *buffer = malloc(length);
    if (!buffer) {
        return luaL_error(L, "Memory allocation failed");
    }
    bool success = I2C_readRegBuf((I2cDevice)devId, (u8)regAddr, buffer, (u32)length);

    if (success) {
        // Create a Lua table to hold the byte values
        lua_createtable(L, length, 0);

        // Fill the table with byte values as integers
        for (int i = 0; i < length; i++) {
            lua_pushinteger(L, buffer[i]);  // Push byte value as integer
            lua_rawseti(L, -2, i + 1);     // Set table[i+1] = byte
        }

        free(buffer);
        return 1;
    } else {
        free(buffer);
        lua_pushnil(L);
        lua_pushstring(L, "I2C read failed");
        return 2;
    }
}

static int i2c_write(lua_State *L) {
    lua_pushstring(L, "TBA");
    return 1;
}

static const luaL_Reg i2c[] = {
    {"read", i2c_read},
    {"write", i2c_write},
    {NULL, NULL}
};

int gm9lua_open_i2c(lua_State* L) {
    luaL_newlib(L, i2c);
    return 1;
}
#endif
