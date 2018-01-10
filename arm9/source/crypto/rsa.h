#pragma once

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

#include "common.h"



//////////////////////////////////
//             RSA              //
//////////////////////////////////

// REG_RSA_CNT
#define RSA_ENABLE          (1u)
#define RSA_UNK_BIT1        (1u<<1)
#define RSA_KEYSLOT(k)      ((k)<<4)
#define RSA_GET_KEYSLOT     ((REG_RSA_CNT & RSA_KEYSLOT(0xFu))>>4)
#define RSA_INPUT_BIG       (1u<<8)
#define RSA_INPUT_LITTLE    (0u)
#define RSA_INPUT_NORMAL    (1u<<9)
#define RSA_INPUT_REVERSED  (0u)

// RSA_SLOTCNT
#define RSA_KEY_STAT_SET    (1u)
#define RSA_KEY_WR_PROT     (1u<<1)
#define RSA_KEY_UNK_BIT31   (1u<<31)

// RSA_SLOTSIZE
#define RSA_SLOTSIZE_2048   (0x40u)


/**
 * @brief      Initializes the RSA hardware.
 */
void RSA_init(void);

/**
 * @brief      Selects the given keyslot for all following RSA operations.
 *
 * @param[in]  keyslot  The keyslot to select.
 */
void RSA_selectKeyslot(u8 keyslot);

/**
 * @brief      Sets a RSA modulus + exponent in the specified keyslot.
 *
 * @param[in]  keyslot  The keyslot this key will be set for.
 * @param[in]  mod      Pointer to 2048-bit RSA modulus data.
 * @param[in]  exp      The exponent to set.
 *
 * @return     Returns true on success, false otherwise.
 */
bool RSA_setKey2048(u8 keyslot, const u8 *const mod, u32 exp);

/**
 * @brief      Decrypts a RSA 2048 signature.
 *
 * @param      decSig  Pointer to decrypted destination signature.
 * @param[in]  encSig  Pointer to encrypted source signature.
 *
 * @return     Returns true on success, false otherwise.
 */
bool RSA_decrypt2048(void *const decSig, const void *const encSig);

/**
 * @brief      Verifies a RSA 2048 SHA 256 signature.
 * @brief      Note: This function skips the ASN.1 data and is therefore not safe.
 *
 * @param[in]  encSig  Pointer to encrypted source signature.
 * @param[in]  data    Pointer to the data to hash.
 * @param[in]  size    The hash data size.
 *
 * @return     Returns true if the signature is valid, false otherwise.
 */
bool RSA_verify2048(const u32 *const encSig, const u32 *const data, u32 size);
