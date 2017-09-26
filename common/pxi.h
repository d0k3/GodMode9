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
    PXI_NONE = 0,
    PXI_READY,
    PXI_BUSY,
    PXI_SCREENINIT,
    PXI_BRIGHTNESS
};

#define PXI_MAXBUFLEN (2048)
#define PXI_FIFO_LEN  (16)

#define PXI_SYNC_RECV ((vu8*)(PXI_BASE + 0x00))
#define PXI_SYNC_SEND ((vu8*)(PXI_BASE + 0x01))
#define PXI_SYNC_IRQ  ((vu8*)(PXI_BASE + 0x03))
#define PXI_SYNC      ((vu32*)(PXI_BASE + 0x00))
#define PXI_CNT       ((vu16*)(PXI_BASE + 0x04))
#define PXI_SEND      ((vu32*)(PXI_BASE + 0x08))
#define PXI_RECV      ((vu32*)(PXI_BASE + 0x0C))

#define PXI_CNT_SEND_FIFO_EMPTY       (BIT(0))
#define PXI_CNT_SEND_FIFO_FULL        (BIT(1))
#define PXI_CNT_SEND_FIFO_EMPTY_IRQ   (BIT(2))
#define PXI_CNT_SEND_FIFO_FLUSH       (BIT(3))
#define PXI_CNT_RECV_FIFO_EMPTY       (BIT(8))
#define PXI_CNT_RECV_FIFO_FULL        (BIT(9))
#define PXI_CNT_RECV_FIFO_NEMPTY_IRQ  (BIT(10))
#define PXI_CNT_ERROR_ACK             (BIT(14))
#define PXI_CNT_ENABLE_FIFO           (BIT(15))

#define PXI_SYNC_TRIGGER_MPCORE (BIT(5))
#define PXI_SYNC_TRIGGER_OLDARM (BIT(6))
#define PXI_SYNC_ENABLE_IRQ     (BIT(7))

static inline void PXI_SetRemote(u8 msg)
{
    *PXI_SYNC_SEND = msg;
}

static inline u8 PXI_GetRemote(void)
{
    return *PXI_SYNC_RECV;
}

static inline void PXI_WaitRemote(u8 msg)
{
    while(*PXI_SYNC_RECV != msg);
}

static inline void PXI_EnableIRQ(void)
{
    *PXI_SYNC_IRQ = PXI_SYNC_ENABLE_IRQ;
}

static inline void PXI_DisableIRQ(void)
{
    *PXI_SYNC_IRQ = 0;
}

static inline void PXI_Sync(void)
{
    #ifdef ARM9
    *PXI_SYNC_IRQ |= PXI_SYNC_TRIGGER_MPCORE;
    #else
    *PXI_SYNC_IRQ |= PXI_SYNC_TRIGGER_OLDARM;
    #endif
}

static void PXI_Reset(void)
{
    *PXI_SYNC = 0;
    *PXI_CNT = PXI_CNT_SEND_FIFO_FLUSH;
    for (int i=0; i<16; i++) {
        *PXI_RECV;
    }
    *PXI_CNT = 0;
    *PXI_CNT = PXI_CNT_ENABLE_FIFO;
    return;
}

static void PXI_Send(u32 w)
{
    while(*PXI_CNT & PXI_CNT_SEND_FIFO_FULL);
    do {
        *PXI_SEND = w;
    } while(*PXI_CNT & PXI_CNT_ERROR_ACK);
    return;
}

static u32 PXI_Recv(void)
{
    u32 ret;
    while(*PXI_CNT & PXI_CNT_RECV_FIFO_EMPTY);
    do {
        ret = *PXI_RECV;
    } while(*PXI_CNT & PXI_CNT_ERROR_ACK);
    return ret;
}

static void PXI_SendArray(const u32 *w, u32 c)
{
    if (c>PXI_FIFO_LEN) c=PXI_FIFO_LEN;
    for (u32 i=0; i<c; i++) {
        PXI_Send(w[i]);
    }
    return;
}

static void PXI_RecvArray(u32 *w, u32 c)
{
    if (c>PXI_FIFO_LEN) c=PXI_FIFO_LEN;
    for (u32 i=0; i<c; i++) {
        w[i] = PXI_Recv();
    }
    return;
}

static void PXI_DoCMD(u8 cmd, u32 *args, u32 argc)
{
    PXI_WaitRemote(PXI_READY);
    PXI_SendArray(args, argc);
    PXI_SetRemote(cmd);
    PXI_Sync();
    PXI_WaitRemote(PXI_BUSY);
    return;
}
