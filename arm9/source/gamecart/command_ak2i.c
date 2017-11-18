// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// modifyed by osilloscopion (2 Jul 2016)
//

#include "command_ntr.h"
#include "command_ak2i.h"
#include "protocol_ntr.h"
#include "card_ntr.h"
#include "delay.h"

u32 AK2I_CmdGetHardwareVersion(void)
{
    u32 cmd[2] = {0xD1000000, 0x00000000};
    u32 ver = 0;

    NTR_SendCommand(cmd, 4, 0, &ver);
    return ver & 0xFF;
}

void AK2I_CmdReadRom(u32 address, u8 *buffer, u32 length)
{
    length &= ~(0x03);
    u32 cmd[2] = {0xB7000000 | (address >> 8), (address & 0xff) << 24};
    NTR_SendCommand(cmd, length, 2, buffer);
}

void AK2I_CmdReadFlash(u32 address, u8 *buffer, u32 length)
{
    length &= ~(0x03);
    u32 cmd[2] = { 0xB7000000 | (address >> 8), (address & 0xff) << 24 | 0x00100000 };
    NTR_SendCommand(cmd, length, 2, buffer);
}

void AK2I_CmdSetMapTableAddress(u32 tableName, u32 tableInRamAddress)
{
    tableName &= 0x0F;
    u32 cmd[2] = {0xD0000000 | (tableInRamAddress >> 8),
        ((tableInRamAddress & 0xff) << 24) | ((u8)tableName << 16) };

    NTR_SendCommand(cmd, 0, 0, NULL);
}

void AK2I_CmdSetFlash1681_81(void)
{
    u32 cmd[2] = {0xD8000000 , 0x0000c606};
    NTR_SendCommand(cmd, 0, 20, NULL);
}

void AK2I_CmdUnlockFlash(void)
{
    u32 cmd[2] = {0xC2AA55AA, 0x55000000};
    NTR_SendCommand(cmd, 0, 0, NULL);
}

void AK2I_CmdUnlockASIC(void)
{
    u32 cmd[2] = { 0xC2AA5555, 0xAA000000 };
    NTR_SendCommand(cmd, 4, 0, NULL);
}

void AK2i_CmdLockFlash(void) {
    u32 cmd[2] = { 0xC2AAAA55, 0x55000000 };
    NTR_SendCommand(cmd, 0, 0, NULL);
}

void AK2I_CmdActiveFatMap(void)
{
    u32 cmd[2] = {0xC255AA55, 0xAA000000};
    NTR_SendCommand(cmd, 4, 0, NULL);
}

static void waitFlashBusy()
{
    u32 state = 0;
    u32 cmd[2] = {0xC0000000, 0x00000000};
    do {
        //ioAK2Delay( 16 * 10 );
        NTR_SendCommand(cmd, 4, 4, &state);
        state &= 1;
    } while(state != 0);
}

void AK2I_CmdEraseFlashBlock_44(u32 address)
{
    u32 cmd[2] = {0xD4000000 | (address & 0x001fffff), (u32)(1<<16)};
    NTR_SendCommand(cmd, 0, 0, NULL);
    waitFlashBusy();
}

void AK2I_CmdEraseFlashBlock_81(u32 address)
{
    u32 cmd[2] = {0xD4000000 | (address & 0x001fffff), (u32)((0x30<<24) | (0x80<<16) | (0<<8) | (0x35))};
    NTR_SendCommand(cmd, 0, 20, NULL);
    waitFlashBusy();
}

void AK2I_CmdWriteFlashByte_44(u32 address, u8 data)
{
    u32 cmd[2] = {0xD4000000 | (address & 0x001fffff), (u32)((data<<24) | (3<<16))};
    NTR_SendCommand(cmd, 0, 20, NULL);
    waitFlashBusy();
}

void AK2I_CmdWriteFlash_44(u32 address, const void *data, u32 length)
{
    u8 * pbuffer = (u8 *)data;
    for(u32 i = 0; i < length; ++i)
    {
        AK2I_CmdWriteFlashByte_44(address, *(pbuffer + i));
        address++;
    }
}

void AK2I_CmdWriteFlashByte_81(u32 address, u8 data)
{
    u32 cmd[2] = { 0xD4000000 | (address & 0x001fffff), (u32)((data<<24) | (0xa0<<16) | (0<<8) | (0x63)) };
    NTR_SendCommand(cmd, 0, 20, NULL);
    waitFlashBusy();
}

void AK2I_CmdWriteFlash_81(u32 address, const void *data, u32 length)
{
    u8 * pbuffer = (u8 *)data;
    for (u32 i = 0; i < length; ++i)
    {
        AK2I_CmdWriteFlashByte_81(address, *(pbuffer + i));
        address++;
    }
}

bool AK2I_CmdVerifyFlash(void *src, u32 dest, u32 length)
{
    u8 verifyBuffer[512];
    u8 * pSrc = (u8 *)src;
    for (u32 i = 0; i < length; i += 512) {
        u32 toRead = 512;
        if (toRead > length - i)
            toRead = length - i;
        AK2I_CmdReadFlash(dest + i, verifyBuffer, toRead);

        for (u32 j = 0; j < toRead; ++j) {
            if(verifyBuffer[j] != *(pSrc + i + j))
                return false;
        }
    }
    return true;
}

