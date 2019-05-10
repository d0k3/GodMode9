/*
 *   This file is part of GodMode9
 *   Copyright (C) 2017-2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
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
#include <types.h>

#ifdef ARM9
#define PXI_BASE	(0x10008000)
#define PXI_RX_INTERRUPT	(14)
#else
#define PXI_BASE	(0x10163000)
#define PXI_RX_INTERRUPT	(83)
#endif

enum {
	PXI_LEGACY_MODE = 0,
	PXI_GET_SHMEM,
	PXI_SET_VMODE,

	PXI_I2C_READ,
	PXI_I2C_WRITE,

	PXI_NVRAM_ONLINE,
	PXI_NVRAM_READ,

	PXI_NOTIFY_LED,
};

/*
 * These should be somewhat "unusual"
 * IDs and shouldnt be similar to
 * those used by any other software
 */
enum {
	ARM11_READY_BARRIER = 19,
};

#define PXI_FIFO_LEN	(16)
#define PXI_MAX_ARGS	(64)

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
#define PXI_CNT_ENABLE_FIFO           (BIT(15))

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

static inline void PXI_Reset(void)
{
	*PXI_SYNC_IRQ = 0;
	*PXI_CNT = PXI_CNT_SEND_FIFO_FLUSH | PXI_CNT_ENABLE_FIFO;
	for (int i = 0; i < PXI_FIFO_LEN; i++)
		*PXI_RECV;

	*PXI_CNT = 0;
	*PXI_CNT = PXI_CNT_RECV_FIFO_AVAIL_IRQ | PXI_CNT_ENABLE_FIFO;

	PXI_SetRemote(0xFF);
}

static inline void PXI_Barrier(u8 barrier_id)
{
	PXI_SetRemote(barrier_id);
	PXI_WaitRemote(barrier_id);
}

static inline void PXI_Send(u32 w)
{
	while(*PXI_CNT & PXI_CNT_SEND_FIFO_FULL);
	*PXI_SEND = w;
}

static inline u32 PXI_Recv(void)
{
	while(*PXI_CNT & PXI_CNT_RECV_FIFO_EMPTY);
	return *PXI_RECV;
}

static inline void PXI_SendArray(const u32 *w, u32 c)
{
	while(c--) PXI_Send(*(w++));
}

static inline void PXI_RecvArray(u32 *w, u32 c)
{
	while(c--) *(w++) = PXI_Recv();
}

static inline u32 PXI_DoCMD(u32 cmd, const u32 *args, u32 argc)
{
	PXI_Send((argc << 16) | cmd);
	PXI_SendArray(args, argc);
	return PXI_Recv();
}
