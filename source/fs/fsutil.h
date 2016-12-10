#pragma once

#include "common.h"
// temporary
// #include "fsinit.h"
// #include "fsdrive.h"
// #include "fsperm.h"

// move / copy flags
#define ASK_ALL         (1<<0)
#define SKIP_ALL        (1<<1)
#define OVERWRITE_ALL   (1<<2)

/** Return total size of SD card **/
uint64_t GetSDCardSize();

/** Format the SD card **/
bool FormatSDCard(u64 hidden_mb, u32 cluster_size);

/** True if file exists and is not locked, false otherwise **/
bool FileCheck(const char* path);

/** Create / open file and write the provided data to it **/
bool FileSetData(const char* path, const u8* data, size_t size, size_t foffset, bool create);

/** Read data from file@offset **/
size_t FileGetData(const char* path, u8* data, size_t size, size_t foffset);

/** Get size of file **/
size_t FileGetSize(const char* path);

/** Get SHA-256 of file **/
bool FileGetSha256(const char* path, u8* sha256);

/** Find data in file **/
u32 FileFindData(const char* path, u8* data, u32 size, u32 offset);

/** Inject file into file @offset **/
bool FileInjectFile(const char* dest, const char* orig, u32 offset);

/** Recursively copy a file or directory **/
bool PathCopy(const char* destdir, const char* orig, u32* flags);

/** Recursively move a file or directory **/
bool PathMove(const char* destdir, const char* orig, u32* flags);

/** Recursively delete a file or directory **/
bool PathDelete(const char* path);

/** Rename file / folder in path to new name **/
bool PathRename(const char* path, const char* newname);

/** Create a new directory in cpath **/
bool DirCreate(const char* cpath, const char* dirname);

/** Create a screenshot of the current framebuffer **/
void CreateScreenshot();
