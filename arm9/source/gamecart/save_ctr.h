#pragma once

#include "gamecart.h"

int InitCtrCardSave(CartData *);
u32 ReadDecryptedCtrCardSave(u8* buffer, u64 offset, u64 count, CartData* cdata);