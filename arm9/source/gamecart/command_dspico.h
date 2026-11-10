#pragma once

#include "common.h"

// DSpico SD access, needs a normal NTR init (InitCartRead) first
// ROM reads stop working until InitCartRead runs again

bool DSpico_CheckGameCode(const u8* header); // NTR header game code, as used by GodMode9i
bool DSpico_Enter(void);
void DSpico_Reset(void); // card was re-initialized into ROM mode
bool DSpico_IsReady(void);
bool DSpico_IsDirty(void);
int DSpico_ReadSectors(void* buffer, u32 sector, u32 count);
int DSpico_WriteSectors(const void* buffer, u32 sector, u32 count);
u64 DSpico_GetSdSize(void);
