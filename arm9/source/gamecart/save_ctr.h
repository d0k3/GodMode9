#pragma once

#include "gamecart.h"

u32 InitCtrCardSave(CartData *);
u32 ReadDecryptedCtrCardSave(u8* buffer, u64 offset, u64 count, CartData* cdata);