/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

#pragma once
#include <types.h>

#ifdef ARM9
#define PXI_BASE (0x10008000)
#define IRQ_PXI_SYNC (12)
#else
#define PXI_BASE (0x10163000)
#define IRQ_PXI_SYNC (80)
#endif

enum {
    PXI_NOCMD = 0,
    PXI_SETBRIGHTNESS = 1
};

#define PXI_SYNC_RECV ((volatile uint8_t*)(PXI_BASE + 0x00))
#define PXI_SYNC_SEND ((volatile uint8_t*)(PXI_BASE + 0x01))
#define PXI_SYNC_IRQ  ((volatile uint8_t*)(PXI_BASE + 0x03))
#define PXI_CNT       ((volatile uint16_t*)(PXI_BASE + 0x04))
#define PXI_SEND      ((volatile uint32_t*)(PXI_BASE + 0x08))
#define PXI_RECV      ((volatile uint32_t*)(PXI_BASE + 0x0C))

#define PXI_CNT_SEND_FIFO_EMPTY       (1<<0)
#define PXI_CNT_SEND_FIFO_FULL        (1<<1)
#define PXI_CNT_SEND_FIFO_EMPTY_IRQ   (1<<2)
#define PXI_CNT_SEND_FIFO_FLUSH       (1<<3)
#define PXI_CNT_RECV_FIFO_EMPTY       (1<<8)
#define PXI_CNT_RECV_FIFO_FULL        (1<<9)
#define PXI_CNT_RECV_FIFO_NEMPTY_IRQ  (1<<10)
#define PXI_CNT_ERROR_ACK             (1<<14)
#define PXI_CNT_ENABLE_FIFO           (1<<15)

#define PXI_SYNC_TRIGGER_MPCORE (1<<5)
#define PXI_SYNC_TRIGGER_OLDARM (1<<6)
#define PXI_SYNC_ENABLE_IRQ     (1<<7)

static inline void PXI_SetRemote(u8 msg)
{
    *PXI_SYNC_SEND = msg;
}

static inline u8 PXI_GetRemote(void)
{
    return *PXI_SYNC_RECV;
}

static inline void PXI_EnableIRQ(void)
{
    *PXI_SYNC_IRQ = PXI_SYNC_ENABLE_IRQ;
}

static inline void PXI_DisableIRQ(void)
{
    *PXI_SYNC_IRQ = 0;
}

static inline void PXI_Wait(void)
{
    while(PXI_GetRemote() != PXI_NOCMD);
}

static inline void PXI_Sync(void)
{
    #ifdef ARM9
    *PXI_SYNC_IRQ |= PXI_SYNC_TRIGGER_MPCORE;
    #else
    *PXI_SYNC_IRQ |= PXI_SYNC_TRIGGER_OLDARM;
    #endif
}

static inline void PXI_Reset(void)
{
    *PXI_SYNC_SEND = 0;
    *PXI_SYNC_IRQ = 0;
    *PXI_CNT = PXI_CNT_SEND_FIFO_FLUSH | PXI_CNT_ENABLE_FIFO;
}

static inline void PXI_Send(u32 w)
{
    while(*PXI_CNT & PXI_CNT_SEND_FIFO_FULL);
    do {
        *PXI_SEND = w;
    } while(*PXI_CNT & PXI_CNT_ERROR_ACK);
    return;
}

static inline u32 PXI_Recv(void)
{
    u32 ret;
    while(*PXI_CNT & PXI_CNT_RECV_FIFO_EMPTY);
    do {
        ret = *PXI_RECV;
    } while(*PXI_CNT & PXI_CNT_ERROR_ACK);
    return ret;
}
