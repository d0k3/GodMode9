#pragma once

#include "common.h"
#include "dir.h"

// primary drive types
#define DRV_UNKNOWN     (0<<0)
#define DRV_FAT         (1<<0)
#define DRV_VIRTUAL     (1<<1)
// secondary drive types
#define DRV_SDCARD      (1<<3)
#define DRV_SYSNAND     (1<<4)
#define DRV_EMUNAND     (1<<5)
#define DRV_IMAGE       (1<<6)
#define DRV_RAMDRIVE    (1<<7)
#define DRV_MEMORY      (1<<8)
#define DRV_ALIAS       (1<<9)
#define DRV_SEARCH      (1<<10)
#define DRV_STDFAT      (1<<11) // standard FAT drive without limitations

// permission types
#define PERM_SDCARD     (1<<0)
#define PERM_RAMDRIVE   (1<<1)
#define PERM_EMUNAND    (1<<2)
#define PERM_SYSNAND    (1<<3)
#define PERM_IMAGE      (1<<4)
#define PERM_MEMORY     (1<<5)
#define PERM_A9LH       ((1<<6) | PERM_SYSNAND)
#define PERM_SDDATA     ((1<<7) | PERM_SDCARD)
#define PERM_BASE       (PERM_SDCARD | PERM_RAMDRIVE)
#define PERM_ALL        (PERM_SDCARD | PERM_RAMDRIVE | PERM_EMUNAND | PERM_SYSNAND | PERM_IMAGE | PERM_MEMORY | PERM_SDDATA)

// move / copy flags
#define ASK_ALL         (1<<0)
#define SKIP_ALL        (1<<1)
#define OVERWRITE_ALL   (1<<2)

bool InitSDCardFS();
bool InitExtFS();
void DeinitExtFS();
void DeinitSDCardFS();

/** Set search pattern / path for special Z: drive **/
void SetFSSearch(const char* pattern, const char* path);

/** Return total size of SD card **/
uint64_t GetSDCardSize();

/** Format the SD card **/
bool FormatSDCard(u64 hidden_mb, u32 cluster_size);

/** Check if writing to this path is allowed **/
bool CheckWritePermissions(const char* path);

/** Set new write permissions */
bool SetWritePermissions(u32 perm, bool add_perm);

/** Get write permissions */
u32 GetWritePermissions();

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

/** Search under a given path **/
void SearchDirContents(DirStruct* contents, const char* path, const char* pattern, bool recursive);

/** Get directory content under a given path **/
void GetDirContents(DirStruct* contents, const char* path);

/** Gets remaining space in filesystem in bytes */
uint64_t GetFreeSpace(const char* path);

/** Gets total spacein filesystem in bytes */
uint64_t GetTotalSpace(const char* path);

/** Return the offset - in sectors - of the FAT partition on the drive **/
uint64_t GetPartitionOffsetSector(const char* path);

/** Function to identify the type of a drive **/
int DriveType(const char* path);

/** Check for special search drive **/
bool IsSearchDrive(const char* path);
