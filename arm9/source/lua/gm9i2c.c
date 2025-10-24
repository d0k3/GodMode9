#ifndef NO_LUA
#include "gm9i2c.h"
#include "system/i2c.h"

// Helper function to validate I2C device ID against enum values
static bool IsValidI2CDevice(int dev_id) {
    switch (dev_id) {
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
    int dev_id = luaL_checkinteger(L, 1);
    int reg_addr = luaL_checkinteger(L, 2);
    int length = luaL_checkinteger(L, 3);

    // Validate device ID against I2cDevice enum
    if (!IsValidI2CDevice(dev_id)) {
        return luaL_error(L, "Invalid device ID: %d (valid IDs: 0-3, 10, 12-17)", dev_id);
    }

    // Validate parameters
    if (length <= 0 || length > 1024) {
        return luaL_error(L, "Invalid length: %d (must be 1-1024)", length);
    }

    if (reg_addr < 0 || reg_addr > 255) {
        return luaL_error(L, "Invalid register address: %d (must be 0-255)", reg_addr);
    }

    // Create a buffer for the read data
    u8 *buffer = malloc(length);
    if (!buffer) {
        return luaL_error(L, "Memory allocation failed");
    }
    bool success = I2C_readRegBuf((I2cDevice)dev_id, (u8)reg_addr, buffer, (u32)length);

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
    // Temporary check for write permissions
    CheckWritePermissionsLuaError(L, "M:/mcu_3ds_regs.mem");

    int dev_id = luaL_checkinteger(L, 1);
    int reg_addr = luaL_checkinteger(L, 2);

    if (!lua_istable(L, 3)) {
        return luaL_error(L, "Third parameter must be a table of byte values");
    }

    int length = lua_rawlen(L, 3);

    if (!IsValidI2CDevice(dev_id)) {
        return luaL_error(L, "Invalid device ID: %d (valid IDs: 0-3, 10, 12-17)", dev_id);
    }

    if (length <= 0 || length > 1024) {
        return luaL_error(L, "Invalid data length: %d (must be 1-1024)", length);
    }

    if (reg_addr < 0 || reg_addr > 255) {
        return luaL_error(L, "Invalid register address: %d (must be 0-255)", reg_addr);
    }

    // Create a buffer for the write data
    u8 *buffer = malloc(length);
    if (!buffer) {
        return luaL_error(L, "Memory allocation failed");
    }

    // Extract byte values from Lua table
    for (int i = 0; i < length; i++) {
        lua_rawgeti(L, 3, i + 1);  // Get table[i+1]

        if (!lua_isinteger(L, -1)) {
            free(buffer);
            return luaL_error(L, "Table element %d is not an integer", i + 1);
        }

        int value = lua_tointeger(L, -1);
        if (value < 0 || value > 255) {
            free(buffer);
            return luaL_error(L, "Table element %d is out of range: %d (must be 0-255)", i + 1, value);
        }

        buffer[i] = (u8)value;
        lua_pop(L, 1);  // Remove the value from stack
    }

    bool success = I2C_writeRegBuf((I2cDevice)dev_id, (u8)reg_addr, buffer, (u32)length);

    free(buffer);

    if (success) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        return luaL_error(L, "I2C write failed");
    }
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
