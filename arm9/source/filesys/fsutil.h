#pragma once

#include "common.h"

// move / copy flags
#define OVERRIDE_PERM   (1UL<<0)
#define NO_CANCEL       (1UL<<1)
#define SILENT          (1UL<<2)
#define CALC_SHA        (1UL<<3)
#define BUILD_PATH      (1UL<<4)
#define ALLOW_EXPAND    (1UL<<5)
#define ASK_ALL         (1UL<<6)
#define SKIP_ALL        (1UL<<7)
#define OVERWRITE_ALL   (1UL<<8)

// file selector flags
#define NO_DIRS         (1UL<<0)
#define NO_FILES        (1UL<<1)
#define HIDE_EXT        (1UL<<2)
#define SELECT_DIRS     (1UL<<3)


/** Return total size of SD card **/
uint64_t GetSDCardSize();

/** Format the SD card **/
bool FormatSDCard(u64 hidden_mb, u32 cluster_size, const char* label);

/** Format the bonus drive area **/
bool SetupBonusDrive(void);

/** Check for file lock, offer to unlock if possible **/
bool FileUnlock(const char* path);

/** Create / open file and write the provided data to it **/
bool FileSetData(const char* path, const void* data, size_t size, size_t foffset, bool create);

/** Read data from file@offset **/
size_t FileGetData(const char* path, void* data, size_t size, size_t foffset);

/** Get size of file **/
size_t FileGetSize(const char* path);

/** Get SHA-256 of file **/
bool FileGetSha256(const char* path, u8* sha256, u64 offset, u64 size);

/** Find data in file **/
u32 FileFindData(const char* path, u8* data, u32 size_data, u32 offset_file);

/** Inject file into file @offset **/
bool FileInjectFile(const char* dest, const char* orig, u64 off_dest, u64 off_orig, u64 size, u32* flags);

/** Fill (a portion of) a file with a fillbyte **/
bool FileSetByte(const char* dest, u64 offset, u64 size, u8 fillbyte, u32* flags);

/** Create a dummy file at dest **/
bool FileCreateDummy(const char* cpath, const char* filename, u64 size);

/** Create a new directory in cpath **/
bool DirCreate(const char* cpath, const char* dirname);

/** Get # of files, subdirs and total size for directory **/
bool DirInfo(const char* path, u64* tsize, u32* tdirs, u32* tfiles);

/** True if path exists **/
bool PathExist(const char* path);

/** Direct recursive move / copy of files or directories **/
bool PathMoveCopy(const char* dest, const char* orig, u32* flags, bool move);

/** Recursively copy a file or directory **/
bool PathCopy(const char* destdir, const char* orig, u32* flags);

/** Recursively move a file or directory **/
bool PathMove(const char* destdir, const char* orig, u32* flags);

/** Recursively delete a file or directory **/
bool PathDelete(const char* path);

/** Rename file / folder in path to new name **/
bool PathRename(const char* path, const char* newname);

/** Change the attributes on a file/directory **/
bool PathAttr(const char* path, u8 attr, u8 mask);

/** Select a file **/
bool FileSelector(char* result, const char* text, const char* path, const char* pattern, u32 flags);
