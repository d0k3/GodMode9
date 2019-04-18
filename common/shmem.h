#pragma once

#include <arm.h>

typedef struct {
	u64 hid_state;
} GlobalSharedMemory;

#ifdef ARM9
#include <pxi.h>

static inline const GlobalSharedMemory *ARM_GetSHMEM(void)
{
	return (const GlobalSharedMemory*)ARM_GetTID();
}

static void ARM_InitSHMEM(void)
{
	ARM_SetTID(PXI_DoCMD(PXI_GET_SHMEM, NULL, 0));
}
#endif
