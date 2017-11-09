/*
    card_access.cpp
    Copyright (C) 2010 yellow wood goblin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

// modifyed by osilloscopion (2 Jul 2016)

#include "protocol_ntr.h"
#include "secure_ntr.h"
#include "card_ntr.h"
// #include "draw.h"
#include "timer.h"
#include "delay.h"


#define BSWAP32(val) ((((val >> 24) & 0xFF)) | (((val >> 16) & 0xFF) << 8) | (((val >> 8) & 0xFF) << 16) | ((val & 0xFF) << 24))

extern u32 ReadDataFlags;

void NTR_CryptUp (u32* pCardHash, u32* aPtr)
{
    u32 x = aPtr[1];
    u32 y = aPtr[0];
    u32 z;

    for(int ii=0;ii<0x10;++ii)
    {
        z = pCardHash[ii] ^ x;
        x = pCardHash[0x012 + ((z >> 24) & 0xff)];
        x = pCardHash[0x112 + ((z >> 16) & 0xff)] + x;
        x = pCardHash[0x212 + ((z >> 8) & 0xff)] ^ x;
        x = pCardHash[0x312 + ((z >> 0) & 0xff)] + x;
        x = y ^ x;
        y = z;
    }
    aPtr[0] = x ^ pCardHash[0x10];
    aPtr[1] = y ^ pCardHash[0x11];
}

void NTR_CryptDown(u32* pCardHash, u32* aPtr)
{
    u32 x = aPtr[1];
    u32 y = aPtr[0];
    u32 z;

    for(int ii=0x11;ii>0x01;--ii)
    {
        z = pCardHash[ii] ^ x;
        x = pCardHash[0x012 + ((z >> 24) & 0xff)];
        x = pCardHash[0x112 + ((z >> 16) & 0xff)] + x;
        x = pCardHash[0x212 + ((z >> 8) & 0xff)] ^ x;
        x = pCardHash[0x312 + ((z >> 0) & 0xff)] + x;
        x = y ^ x;
        y = z;
    }
    aPtr[0] = x ^ pCardHash[0x01];
    aPtr[1] = y ^ pCardHash[0x00];
}

// chosen by fair dice roll.
// guaranteed to be random.
#define getRandomNumber() (rand())

void NTR_InitKey1 (u8* aCmdData, IKEY1* pKey1, int iCardDevice)
{
    pKey1->iii = getRandomNumber() & 0x00000fff;
    pKey1->jjj = getRandomNumber() & 0x00000fff;
    pKey1->kkkkk = getRandomNumber() & 0x000fffff;
    pKey1->llll = getRandomNumber() & 0x0000ffff;
    pKey1->mmm = getRandomNumber() & 0x00000fff;
    pKey1->nnn = getRandomNumber() & 0x00000fff;

    if(iCardDevice) //DSi
      aCmdData[7]=NTRCARD_CMD_ACTIVATE_BF2;  //0x3D
    else
      aCmdData[7]=NTRCARD_CMD_ACTIVATE_BF;

    aCmdData[6] = (u8)(pKey1->iii >> 4);
    aCmdData[5] = (u8)((pKey1->iii << 4) | (pKey1->jjj >> 8));
    aCmdData[4] = (u8)pKey1->jjj;
    aCmdData[3] = (u8)(pKey1->kkkkk >> 16);
    aCmdData[2] = (u8)(pKey1->kkkkk >> 8);
    aCmdData[1] = (u8)pKey1->kkkkk;
    aCmdData[0] = (u8)getRandomNumber();
}

void NTR_ApplyKey (u32* pCardHash, int nCardHash, u32* pKeyCode)
{
    u32 scratch[2];

    NTR_CryptUp (pCardHash, &pKeyCode[1]);
    NTR_CryptUp (pCardHash, &pKeyCode[0]);
    memset(scratch, 0, sizeof (scratch));

    for(int ii=0;ii<0x12;++ii)
    {
        pCardHash[ii] = pCardHash[ii] ^ BSWAP32 (pKeyCode[ii%2]);
    }

    for(int ii=0;ii<nCardHash;ii+=2)
    {
        NTR_CryptUp (pCardHash, scratch);
        pCardHash[ii] = scratch[1];
        pCardHash[ii+1] = scratch[0];
    }
}

void NTR_InitKey (u32 aGameCode, u32* pCardHash, int nCardHash, u32* pKeyCode, int level, int iCardDevice)
{
	if(iCardDevice)
	{
		const u8* BlowfishTwl = (const u8*)0x01FFD3E0;
		memcpy (pCardHash, BlowfishTwl, 0x1048);
	}
	else
    {
		const u8* BlowfishNtr = (const u8*)0x01FFE428;
		memcpy (pCardHash, BlowfishNtr, 0x1048);
	}
	
    pKeyCode[0] = aGameCode;
    pKeyCode[1] = aGameCode/2;
    pKeyCode[2] = aGameCode*2;

	if (level >= 1) NTR_ApplyKey (pCardHash, nCardHash, pKeyCode);
    if (level >= 2) NTR_ApplyKey (pCardHash, nCardHash, pKeyCode);

    pKeyCode[1] = pKeyCode[1]*2;
    pKeyCode[2] = pKeyCode[2]/2;

    if (level >= 3) NTR_ApplyKey (pCardHash, nCardHash, pKeyCode);
}

void NTR_CreateEncryptedCommand (u8 aCommand, u32* pCardHash, u8* aCmdData, IKEY1* pKey1, u32 aBlock)
{
    u32 iii,jjj;
    if(aCommand!=NTRCARD_CMD_SECURE_READ) aBlock=pKey1->llll;
    if(aCommand==NTRCARD_CMD_ACTIVATE_SEC)
    {
        iii=pKey1->mmm;
        jjj=pKey1->nnn;
    }
    else
    {
        iii=pKey1->iii;
        jjj=pKey1->jjj;
    }
    aCmdData[7]=(u8)(aCommand|(aBlock>>12));
    aCmdData[6]=(u8)(aBlock>>4);
    aCmdData[5]=(u8)((aBlock<<4)|(iii>>8));
    aCmdData[4]=(u8)iii;
    aCmdData[3]=(u8)(jjj>>4);
    aCmdData[2]=(u8)((jjj<<4)|(pKey1->kkkkk>>16));
    aCmdData[1]=(u8)(pKey1->kkkkk>>8);
    aCmdData[0]=(u8)pKey1->kkkkk;

    NTR_CryptUp(pCardHash, (u32*)(void*)aCmdData);

    pKey1->kkkkk+=1;
}

void NTR_DecryptSecureArea (u32 aGameCode, u32* pCardHash, int nCardHash, u32* pKeyCode, u32* pSecureArea, int iCardDevice)
{
    NTR_InitKey (aGameCode, pCardHash, nCardHash, pKeyCode, 2, iCardDevice);
    NTR_CryptDown(pCardHash, pSecureArea);
    NTR_InitKey(aGameCode, pCardHash, nCardHash, pKeyCode, 3, iCardDevice);
    for(int ii=0;ii<0x200;ii+=2) NTR_CryptDown (pCardHash, pSecureArea + ii);
}

// Count timeout at (33.514 / 1024) Mhz.
void NTR_SecureDelay(u16 aTimeout)
{
    u64 tTimeout = (((u64)(aTimeout&0x3FFF)+3)<<10);
    u64 timer = timer_start();
    while (timer_ticks(timer) < tTimeout);
}

/*// Causes the timer to count at (33.514 / 1024) Mhz.
#define TIMER_DIV_1024  (3)
#define TIMER0_DATA    (*(vu16*)0x10003000)
#define TIMER0_CR   (*(vu16*)0x10003002)
#define TIMER_ENABLE    (1<<7)
void NTR_SecureDelay(u16 aTimeout)
{
  // Using a while loop to check the timeout,
  // so we have to wait until one before overflow.
  // This also requires an extra 1 for the timer data.
  // See GBATek for the normal formula used for card timeout.
  TIMER0_DATA=0x10000-(((aTimeout&0x3FFF)+3));
  //TIMER0_CR=TIMER_DIV_256|TIMER_ENABLE;
  TIMER0_CR=TIMER_DIV_1024|TIMER_ENABLE;
  while(TIMER0_DATA!=0xFFFF);
  
  // Clear out the timer registers
  TIMER0_CR=0;
  TIMER0_DATA=0;
}*/

u32 NTR_GetIDSafe (u32 flags, const u8* command)
{
    u32 data = 0;
    cardWriteCommand(command);
    REG_NTRCARDROMCNT = flags | NTRCARD_BLK_SIZE(7);

    do
    {
        if (REG_NTRCARDROMCNT & NTRCARD_DATA_READY)
        {
            data = REG_NTRCARDFIFO;
        }
    }
    while(REG_NTRCARDROMCNT & NTRCARD_BUSY);

    return data;
}

void NTR_CmdSecure (u32 flags, void* buffer, u32 length, u8* pcmd)
{
    cardPolledTransfer (flags, buffer, length, pcmd);
}

bool NTR_Secure_Init (u8* header, u32 CartID, int iCardDevice)
{
	u32 iGameCode;
    u32 iCardHash[0x412] = {0};
    u32 iKeyCode[3] = {0};
    u32* secureArea=(u32*)(void*)(header + 0x4000);
    u8 cmdData[8] __attribute__((aligned(32)));
    const u8 cardSeedBytes[]={0xE8,0x4D,0x5A,0xB1,0x17,0x8F,0x99,0xD5};
    IKEY1 iKey1 ={0};
    bool iCheapCard = (CartID & 0x80000000) != 0;
    u32 cardControl13 = *((vu32*)(void*)&header[0x60]);
    u32 cardControlBF = *((vu32*)(void*)&header[0x64]);
	u16 readTimeout = *((vu16*)(void*)&header[0x6E]);
    u32 nds9Offset = *((vu32*)(void*)&header[0x20]);
	u8 deviceType = header[0x13];
	int nCardHash = sizeof (iCardHash) / sizeof (iCardHash[0]);
    u32 flagsKey1=NTRCARD_ACTIVATE|NTRCARD_nRESET|(cardControl13&(NTRCARD_WR|NTRCARD_CLK_SLOW))|((cardControlBF&(NTRCARD_CLK_SLOW|NTRCARD_DELAY1(0x1FFF)))+((cardControlBF&NTRCARD_DELAY2(0x3F))>>16));
    u32 flagsSec=(cardControlBF&(NTRCARD_CLK_SLOW|NTRCARD_DELAY1(0x1FFF)|NTRCARD_DELAY2(0x3F)))|NTRCARD_ACTIVATE|NTRCARD_nRESET|NTRCARD_SEC_EN|NTRCARD_SEC_DAT;

    iGameCode = *((vu32*)(void*)&header[0x0C]);
    ReadDataFlags = cardControl13 & ~ NTRCARD_BLK_SIZE(7);
    NTR_InitKey (iGameCode, iCardHash, nCardHash, iKeyCode, iCardDevice?1:2, iCardDevice);

    if(!iCheapCard) flagsKey1 |= NTRCARD_SEC_LARGE;
    //Debug("iCheapCard=%d, readTimeout=%d", iCheapCard, readTimeout);

	NTR_InitKey1 (cmdData, &iKey1, iCardDevice);
    //Debug("cmdData=%02X %02X %02X %02X %02X %02X %02X %02X ", cmdData[0], cmdData[1], cmdData[2], cmdData[3], cmdData[4], cmdData[5], cmdData[6], cmdData[7]);
    //Debug("iKey1=%08X %08X %08X", iKey1.iii, iKey1. jjj, iKey1. kkkkk);
    //Debug("iKey1=%08X %08X %08X", iKey1. llll, iKey1. mmm, iKey1. nnn);

    NTR_CmdSecure ((cardControl13&(NTRCARD_CLK_SLOW|NTRCARD_DELAY2(0x3f)|NTRCARD_DELAY1(0x1fff)))|NTRCARD_ACTIVATE|NTRCARD_nRESET, NULL, 0, cmdData);

    NTR_CreateEncryptedCommand (NTRCARD_CMD_ACTIVATE_SEC, iCardHash, cmdData, &iKey1, 0);
    //Debug("cmdData=%02X %02X %02X %02X %02X %02X %02X %02X ", cmdData[0], cmdData[1], cmdData[2], cmdData[3], cmdData[4], cmdData[5], cmdData[6], cmdData[7]);
    if(iCheapCard)
    {
        NTR_CmdSecure (flagsKey1, NULL, 0, cmdData);
		NTR_SecureDelay(readTimeout);
    }
    NTR_CmdSecure (flagsKey1, NULL, 0, cmdData);

    REG_NTRCARDROMCNT = 0;
    REG_NTRCARDSEEDX_L = cardSeedBytes[deviceType & 0x07] | (iKey1.nnn << 15) | (iKey1.mmm << 27) | 0x6000;
    REG_NTRCARDSEEDY_L = 0x879b9b05;
    REG_NTRCARDSEEDX_H = iKey1.mmm >> 5;
    REG_NTRCARDSEEDY_H = 0x5c;
    REG_NTRCARDROMCNT = NTRCARD_nRESET | NTRCARD_SEC_SEED | NTRCARD_SEC_EN | NTRCARD_SEC_DAT;

    flagsKey1 |= NTRCARD_SEC_EN | NTRCARD_SEC_DAT;
    NTR_CreateEncryptedCommand(NTRCARD_CMD_SECURE_CHIPID, iCardHash, cmdData, &iKey1, 0);
    //Debug("cmdData=%02X %02X %02X %02X %02X %02X %02X %02X ", cmdData[0], cmdData[1], cmdData[2], cmdData[3], cmdData[4], cmdData[5], cmdData[6], cmdData[7]);
    u32 SecureCartID = 0;
    if(iCheapCard)
    {
        NTR_CmdSecure (flagsKey1, NULL, 0, cmdData);
		NTR_SecureDelay(readTimeout);
    }

    //NTR_CmdSecure (flagsKey1, &SecureCartID, sizeof (SecureCartID), cmdData);
    SecureCartID = NTR_GetIDSafe (flagsKey1, cmdData);

    if (SecureCartID != CartID)
    {
        // Debug("Invalid SecureCartID\n(%08X != %08X)", SecureCartID, CartID);
        return false;
    }

    int secureAreaOffset = 0;
    for(int secureBlockNumber=4;secureBlockNumber<8;++secureBlockNumber)
    {
        NTR_CreateEncryptedCommand (NTRCARD_CMD_SECURE_READ, iCardHash, cmdData, &iKey1, secureBlockNumber);
        if (iCheapCard)
        {
            NTR_CmdSecure (flagsSec, NULL, 0, cmdData);
			NTR_SecureDelay(readTimeout);
            for(int ii=8;ii>0;--ii)
            {
                NTR_CmdSecure (flagsSec | NTRCARD_BLK_SIZE(1), secureArea + secureAreaOffset, 0x200, cmdData);
                secureAreaOffset += 0x200 / sizeof (u32);
            }
        }
        else
        {
            NTR_CmdSecure (flagsSec | NTRCARD_BLK_SIZE(4) | NTRCARD_SEC_LARGE, secureArea + secureAreaOffset, 0x1000, cmdData);
            secureAreaOffset += 0x1000 / sizeof (u32);
        }
    }

    NTR_CreateEncryptedCommand (NTRCARD_CMD_DATA_MODE, iCardHash, cmdData, &iKey1, 0);
    if(iCheapCard)
    {
        NTR_CmdSecure (flagsKey1, NULL, 0, cmdData);
		NTR_SecureDelay(readTimeout);
    }
    NTR_CmdSecure (flagsKey1, NULL, 0, cmdData);

    //CycloDS doesn't like the dsi secure area being decrypted
    if(!iCardDevice && ((nds9Offset != 0x4000) || secureArea[0] || secureArea[1]))
    {
		NTR_DecryptSecureArea (iGameCode, iCardHash, nCardHash, iKeyCode, secureArea, iCardDevice);
	}

    //Debug("secure area %08X %08X", secureArea[0], secureArea[1]);
    if(secureArea[0] == 0x72636e65/*'encr'*/ && secureArea[1] == 0x6a624f79/*'yObj'*/)
    {
        secureArea[0] = 0xe7ffdeff;
        secureArea[1] = 0xe7ffdeff;
    }
    else
    {
        //Debug("Invalid secure area (%08X %08X)", secureArea[0], secureArea[1]);
        //dragon quest 5 has invalid secure area. really.
		//return false;
    }

    return true;
}
