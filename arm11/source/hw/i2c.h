/*
 *   This file is part of fastboot 3DS
 *   Copyright (C) 2017 derrek, profi200
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>
#include <types.h>


#define I2C_STOP          (1u)
#define I2C_START         (1u<<1)
#define I2C_ERROR         (1u<<2)
#define I2C_ACK           (1u<<4)
#define I2C_DIRE_WRITE    (0u)
#define I2C_DIRE_READ     (1u<<5)
#define I2C_IRQ_ENABLE    (1u<<6)
#define I2C_ENABLE        (1u<<7)

#define I2C_GET_ACK(reg)  ((bool)((reg)>>4 & 1u))


/**
 * @brief      Initializes the I2C buses. Call this only once.
 */
void I2C_init(void);

/**
 * @brief      Reads data from a I2C register to a buffer.
 *
 * @param[in]  devId    The device ID. Use the enum above.
 * @param[in]  regAddr  The register address.
 * @param      out      The output buffer pointer.
 * @param[in]  size     The read size.
 *
 * @return     Returns true on success and false on failure.
 */
bool I2C_readRegBuf(int devId, u8 regAddr, u8 *out, u32 size);

/**
 * @brief      Writes a buffer to a I2C register.
 *
 * @param[in]  devId    The device ID. Use the enum above.
 * @param[in]  regAddr  The register address.
 * @param[in]  in       The input buffer pointer.
 * @param[in]  size     The write size.
 *
 * @return     Returns true on success and false on failure.
 */
bool I2C_writeRegBuf(int devId, u8 regAddr, const u8 *in, u32 size);
