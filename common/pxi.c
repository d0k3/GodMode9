/*
 *   This file is part of GodMode9
 *   Copyright (C) 2019 d0k3, Wolfvak
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

#include <types.h>
#include <pxi.h>

void PXI_Barrier(u8 barrier_id)
{
	PXI_SetRemote(barrier_id);
	PXI_WaitRemote(barrier_id);
}

void PXI_Reset(void)
{
	*PXI_SYNC_IRQ = 0;
	*PXI_CNT = PXI_CNT_SEND_FIFO_FLUSH | PXI_CNT_ENABLE_FIFO;
	for (int i = 0; i < PXI_FIFO_LEN; i++)
		*PXI_RECV;

	*PXI_CNT = 0;
	*PXI_CNT = PXI_CNT_RECV_FIFO_AVAIL_IRQ | PXI_CNT_ENABLE_FIFO |
				PXI_CNT_ACKNOWLEDGE_ERROR;

	PXI_SetRemote(0xFF);
}

void PXI_SendArray(const u32 *w, u32 c)
{
	while(c--)
		PXI_Send(*(w++));
}

void PXI_RecvArray(u32 *w, u32 c)
{
	while(c--)
		*(w++) = PXI_Recv();
}

u32 PXI_DoCMD(u32 cmd, const u32 *args, u32 argc)
{
	PXI_Send((argc << 16) | cmd);
	PXI_SendArray(args, argc);
	return PXI_Recv();
}
