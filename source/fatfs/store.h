#pragma once

#include "fs.h"

bool IsStoredDrive(const char* path);
void StoreDirContents(DirStruct* contents);
void GetStoredDirContents(DirStruct* contents);
