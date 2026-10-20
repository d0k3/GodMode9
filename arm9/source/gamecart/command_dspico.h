#pragma once

#include "common.h"

// DSpico flashcart SD card access, protocol based on the open DSpico firmware.
// The cart must have completed a normal NTR init (InitCartRead) before entering.
// While active, normal ROM reads do not work. Run InitCartRead again to leave.

bool DSpico_CheckGameCode(const u8* header); // NTR header game code, as used by GodMode9i
bool DSpico_Enter(void);
void DSpico_Reset(void); // card was re-initialized into ROM mode
bool DSpico_IsReady(void);
bool DSpico_IsDirty(void);
int DSpico_ReadSectors(void* buffer, u32 sector, u32 count);
int DSpico_WriteSectors(const void* buffer, u32 sector, u32 count);
u64 DSpico_GetSdSize(void);
