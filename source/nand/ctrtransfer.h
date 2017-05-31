#pragma once

#include "common.h"

u32 CheckTransferableMbr(void* data);
u32 TransferCtrNandImage(const char* path_img, const char* drv);
