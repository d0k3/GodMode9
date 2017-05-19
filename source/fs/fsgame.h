#pragma once

#include "common.h"
#include "fsdir.h"

void SetDirGoodNames(DirStruct* contents);
bool GoodRenamer(DirEntry* entry, bool ask);
