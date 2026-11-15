// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// modifyed by osilloscopion (2 Jul 2016)
//

#include <arm.h>

#include "command_ntr.h"
#include "protocol_ntr.h"
#include "card_ntr.h"
#include "timer.h"

u32 ReadDataFlags = 0;

bool NTR_CmdReset(void)
{
    if (!cardReset()) return false;
    ARM_WaitCycles(0xF000 * 4);
    return true;
}

u32 NTR_CmdGetCartId(void)
{
    return cardReadID (0);
}

void NTR_CmdEnter16ByteMode(void)
{
    static const u32 enter16bytemode_cmd[2] = { 0x3E000000, 0x00000000 };
    NTR_SendCommand(enter16bytemode_cmd, 0x0, 0, NULL);
}

static bool WaitCardIdle(void) {
    u64 begin = timer_start();
    while (REG_NTRCARDROMCNT & NTRCARD_BUSY)
        if (timer_ticks(begin) >= TICKS_PER_SEC * 2) return false;
    return true;
}

bool NTR_CmdReadHeader (u8* buffer)
{
    REG_NTRCARDROMCNT=0;
    REG_NTRCARDMCNT=0;
    ARM_WaitCycles(167550 * 4);
    REG_NTRCARDMCNT=NTRCARD_CR1_ENABLE|NTRCARD_CR1_IRQ;
    REG_NTRCARDROMCNT=NTRCARD_nRESET|NTRCARD_SEC_SEED;
    if (!WaitCardIdle() || !cardReset() || !WaitCardIdle()) return false;
    u32 iCardId=cardReadID(NTRCARD_CLK_SLOW);
    if ((iCardId == UINT32_MAX) || !WaitCardIdle()) return false;

    u32 iCheapCard=iCardId&0x80000000;

    if(iCheapCard)
    {
        //this is magic of wood goblins
        for (size_t ii = 0; ii < 8; ++ii) {
            if (!cardParamCommand(NTRCARD_CMD_HEADER_READ, ii * 0x200,
                NTRCARD_ACTIVATE | NTRCARD_nRESET | NTRCARD_CLK_SLOW |
                NTRCARD_BLK_SIZE(1) | NTRCARD_DELAY1(0x1FFF) | NTRCARD_DELAY2(0x3F),
                (u32*)(void*)(buffer + ii * 0x200), 0x200 / sizeof(u32)))
                return false;
        }
    }
    else
    {
      //0xac3f1fff
        if (!cardParamCommand(NTRCARD_CMD_HEADER_READ, 0,
            NTRCARD_ACTIVATE | NTRCARD_nRESET | NTRCARD_CLK_SLOW |
            NTRCARD_BLK_SIZE(4) | NTRCARD_DELAY1(0x1FFF) | NTRCARD_DELAY2(0x3F),
            (u32*)(void*)buffer, 0x1000 / sizeof(u32)))
            return false;
    }
    //cardReadHeader (buffer);
    return true;
}

bool NTR_CmdReadData (u32 offset, void* buffer)
{
    return cardParamCommand (NTRCARD_CMD_DATA_READ, offset, ReadDataFlags | NTRCARD_ACTIVATE | NTRCARD_nRESET | NTRCARD_BLK_SIZE(1), (u32*)buffer, 0x200 / 4);
}
