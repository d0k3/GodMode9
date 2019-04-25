// C port of byuu's \nall\beat\patch.hpp and \multi.hpp, which were released under GPLv3
// https://github.com/eai04191/beat/blob/master/nall/beat/patch.hpp
// https://github.com/eai04191/beat/blob/master/nall/beat/multi.hpp
// Ported by Hyarion for use with VirtualFatFS

#pragma once

int ApplyBPSPatch(const char* modifyName, const char* sourceName, const char* targetName);
int ApplyBPMPatch(const char* patchName, const char* sourcePath, const char* targetPath);
