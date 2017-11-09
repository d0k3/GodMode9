// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "protocol_ntr.h"
#ifdef VERBOSE_COMMANDS
#include "draw.h"
#endif

void NTR_SendCommand(const u32 command[2], u32 pageSize, u32 latency, void* buffer)
{
#ifdef VERBOSE_COMMANDS
    Debug("N> %08X %08X", command[0], command[1]);
#endif

    REG_NTRCARDMCNT = NTRCARD_CR1_ENABLE;

    for( u32 i=0; i<2; ++i )
    {
        REG_NTRCARDCMD[i*4+0] = command[i]>>24;
        REG_NTRCARDCMD[i*4+1] = command[i]>>16;
        REG_NTRCARDCMD[i*4+2] = command[i]>>8;
        REG_NTRCARDCMD[i*4+3] = command[i]>>0;
    }

    pageSize -= pageSize & 3; // align to 4 byte

    u32 pageParam = NTRCARD_PAGESIZE_4K;
    u32 transferLength = 4096;

    // make zero read and 4 byte read a little special for timing optimization(and 512 too)
    switch (pageSize) {
        case 0:
            transferLength = 0;
            pageParam = NTRCARD_PAGESIZE_0;
            break;
        case 4:
            transferLength = 4;
            pageParam = NTRCARD_PAGESIZE_4;
            break;
        case 512:
            transferLength = 512;
            pageParam = NTRCARD_PAGESIZE_512;
            break;
        case 8192:
            transferLength = 8192;
            pageParam = NTRCARD_PAGESIZE_8K;
            break;
        case 16384:
            transferLength = 16384;
            pageParam = NTRCARD_PAGESIZE_16K;
            break;
	default:
	    break; //Using 4K pagesize and transfer length by default
    }

    // go
    REG_NTRCARDROMCNT = 0x10000000;
    REG_NTRCARDROMCNT = NTRKEY_PARAM | NTRCARD_ACTIVATE | NTRCARD_nRESET | pageParam | latency;

    u8 * pbuf = (u8 *)buffer;
    u32 * pbuf32 = (u32 * )buffer;
    bool useBuf = ( NULL != pbuf );
    bool useBuf32 = (useBuf && (0 == (3 & ((u32)buffer))));

    u32 count = 0;
    u32 cardCtrl = REG_NTRCARDROMCNT;

    if(useBuf32)
    {
        while( (cardCtrl & NTRCARD_BUSY) && count < pageSize)
        {
            cardCtrl = REG_NTRCARDROMCNT;
            if( cardCtrl & NTRCARD_DATA_READY  ) {
                u32 data = REG_NTRCARDFIFO;
                *pbuf32++ = data;
                count += 4;
            }
        }
    }
    else if(useBuf)
    {
        while( (cardCtrl & NTRCARD_BUSY) && count < pageSize)
        {
            cardCtrl = REG_NTRCARDROMCNT;
            if( cardCtrl & NTRCARD_DATA_READY  ) {
                u32 data = REG_NTRCARDFIFO;
                pbuf[0] = (unsigned char) (data >>  0);
                pbuf[1] = (unsigned char) (data >>  8);
                pbuf[2] = (unsigned char) (data >> 16);
                pbuf[3] = (unsigned char) (data >> 24);
                pbuf += sizeof (unsigned int);
                count += 4;
            }
        }
    }
    else
    {
        while( (cardCtrl & NTRCARD_BUSY) && count < pageSize)
        {
            cardCtrl = REG_NTRCARDROMCNT;
            if( cardCtrl & NTRCARD_DATA_READY  ) {
                u32 data = REG_NTRCARDFIFO;
                (void)data;
                count += 4;
            }
        }
    }

    // if read is not finished, ds will not pull ROM CS to high, we pull it high manually
    if( count != transferLength ) {
        // MUST wait for next data ready,
        // if ds pull ROM CS to high during 4 byte data transfer, something will mess up
        // so we have to wait next data ready
        do { cardCtrl = REG_NTRCARDROMCNT; } while(!(cardCtrl & NTRCARD_DATA_READY));
        // and this tiny delay is necessary
        //ioAK2Delay(33);
        // pull ROM CS high
        REG_NTRCARDROMCNT = 0x10000000;
        REG_NTRCARDROMCNT = NTRKEY_PARAM | NTRCARD_ACTIVATE | NTRCARD_nRESET/* | 0 | 0x0000*/;
    }
    // wait rom cs high
    do { cardCtrl = REG_NTRCARDROMCNT; } while( cardCtrl & NTRCARD_BUSY );
    //lastCmd[0] = command[0];lastCmd[1] = command[1];

#ifdef VERBOSE_COMMANDS
    if (!useBuf) {
        Debug("N< NULL");
    } else if (!useBuf32) {
        Debug("N< non32");
    } else {
        u32* p = (u32*)buffer;
        int transferWords = count / 4;
        for (int i = 0; i < transferWords && i < 4*4; i += 4) {
            switch (transferWords - i) {
            case 0:
                break;
            case 1:
                Debug("N< %08X", p[i+0]);
                break;
            case 2:
                Debug("N< %08X %08X", p[i+0], p[i+1]);
                break;
            case 3:
                Debug("N< %08X %08X %08X", p[i+0], p[i+1], p[i+2]);
                break;
            default:
                Debug("N< %08X %08X %08X %08X", p[i+0], p[i+1], p[i+2], p[i+3]);
                break;
            }
        }
    }
#endif
}
