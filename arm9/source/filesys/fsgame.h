#pragma once

#include "common.h"
#include "fsdir.h"

void SetupTitleManager(DirStruct* contents);
bool GoodRenamer(DirEntry* entry, bool ask);
