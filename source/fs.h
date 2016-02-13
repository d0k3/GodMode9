#pragma once

#include "common.h"

bool InitFS();
void DeinitFS();

/** Opens existing files */
bool FileOpen(const char* path);
bool DebugFileOpen(const char* path);

/** Opens new files (and creates them if they don't already exist) */
bool FileCreate(const char* path, bool truncate);
bool DebugFileCreate(const char* path, bool truncate);

/** Copies currently opened file to destination (must provide buffer) */
size_t FileCopyTo(const char* dest, void* buf, size_t bufsize);

/** Reads contents of the opened file */
size_t FileRead(void* buf, size_t size, size_t foffset);
bool DebugFileRead(void* buf, size_t size, size_t foffset);

/** Writes to the opened file */
size_t FileWrite(void* buf, size_t size, size_t foffset);
bool DebugFileWrite(void* buf, size_t size, size_t foffset);

/** Gets the size of the opened file */
size_t FileGetSize();

/** Creates a directory */
bool DirMake(const char* path);
bool DebugDirMake(const char* path);

/** Opens an existing directory */
bool DirOpen(const char* path);
bool DebugDirOpen(const char* path);

/** Reads next file name to fname from opened directory,
    returns false if all files in directory are processed.
    fname needs to be allocated to fsize bytes minimum. */
bool DirRead(char* fname, int fsize);

/** Get list of files under a given path **/
bool GetFileList(const char* path, char* list, int lsize, bool recursive, bool inc_files, bool inc_dirs);

/** Writes text to a constantly open log file **/
size_t LogWrite(const char* text);

/** Gets remaining space on SD card in bytes */
uint64_t RemainingStorageSpace();

/** Gets total space on SD card in bytes */
uint64_t TotalStorageSpace();

void FileClose();

void DirClose();
