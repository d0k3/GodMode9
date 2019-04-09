/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

#pragma once
#include <types.h>

#ifdef ARM9
#define PXI_BASE (0x10008000)
#define IRQ_PXI_RX  (14)
#else
#define PXI_BASE (0x10163000)
#define IRQ_PXI_RX  (83)
#endif

enum {
	PXI_SCREENINIT = 0,
	PXI_BRIGHTNESS,
	PXI_I2C_READ,
	PXI_I2C_WRITE,
	PXI_LEGACY_BOOT,
};

#define PXI_FIFO_LEN (16)

#define PXI_SYNC_RECV ((vu8*)(PXI_BASE + 0x00))
#define PXI_SYNC_SEND ((vu8*)(PXI_BASE + 0x01))
#define PXI_SYNC_IRQ  ((vu8*)(PXI_BASE + 0x03))
#define PXI_CNT       ((vu16*)(PXI_BASE + 0x04))
#define PXI_SEND      ((vu32*)(PXI_BASE + 0x08))
#define PXI_RECV      ((vu32*)(PXI_BASE + 0x0C))

#define PXI_CNT_SEND_FIFO_EMPTY       (BIT(0))
#define PXI_CNT_SEND_FIFO_FULL        (BIT(1))
#define PXI_CNT_SEND_FIFO_EMPTY_IRQ   (BIT(2))
#define PXI_CNT_SEND_FIFO_FLUSH       (BIT(3))
#define PXI_CNT_RECV_FIFO_EMPTY       (BIT(8))
#define PXI_CNT_RECV_FIFO_FULL        (BIT(9))
#define PXI_CNT_RECV_FIFO_AVAIL_IRQ   (BIT(10))
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
	while(PXI_GetRemote() != msg);
}

static void PXI_Reset(void)
{
	*PXI_SYNC_IRQ = 0;
	*PXI_CNT = PXI_CNT_SEND_FIFO_FLUSH | PXI_CNT_ENABLE_FIFO;
	for (int i = 0; i < PXI_FIFO_LEN; i++)
		*PXI_RECV;

	*PXI_CNT = 0;
	*PXI_CNT = PXI_CNT_RECV_FIFO_AVAIL_IRQ | PXI_CNT_ENABLE_FIFO;
}

static void PXI_Send(u32 w)
{
	while(*PXI_CNT & PXI_CNT_SEND_FIFO_FULL);
	*PXI_SEND = w;
}

static u32 PXI_Recv(void)
{
	while(*PXI_CNT & PXI_CNT_RECV_FIFO_EMPTY);
	return *PXI_RECV;
}

static void PXI_SendArray(const u32 *w, u32 c)
{
	while(c--) PXI_Send(*(w++));
}

static void PXI_RecvArray(u32 *w, u32 c)
{
	while(c--) *(w++) = PXI_Recv();
}

static u32 PXI_DoCMD(u32 cmd, const u32 *args, u32 argc)
{
	PXI_Send((argc << 16) | cmd);
	PXI_SendArray(args, argc);
	return PXI_Recv();
}
