#include "common.h"
#include "virtual.h"

bool CheckVVramDrive(void);

bool ReadVVramDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVVramFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);

bool GetVVramFilename(char* name, const VirtualFile* vfile);
bool MatchVVramFilename(const char* name, const VirtualFile* vfile);

u64 GetVVramDriveSize(void);
