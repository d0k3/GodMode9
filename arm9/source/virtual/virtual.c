#include "virtual.h"
#include "vnand.h"
#include "vmem.h"
#include "vgame.h"
#include "vbdri.h"
#include "vkeydb.h"
#include "vcart.h"
#include "vvram.h"
#include "vdisadiff.h"
#include "ff.h"

typedef struct {
    char drv_letter;
    u32 virtual_src;
} PACKED_STRUCT VirtualDrive;

static const VirtualDrive virtualDrives[] = { VRT_DRIVES };

u32 GetVirtualSource(const char* path) {
    // check path validity
    if ((strnlen(path, 16) < 2) || (path[1] != ':') || ((path[2] != '/') && (path[2] != '\0')))
        return 0;
    // search for virtual source
    for (u32 i = 0; i < (sizeof(virtualDrives) / sizeof(VirtualDrive)); i++)
        if (*path == virtualDrives[i].drv_letter) return virtualDrives[i].virtual_src;
    return 0;
}

void DeinitVirtualImageDrive(void) {
    DeinitVGameDrive();
    DeinitVBDRIDrive();
    DeinitVKeyDbDrive();
    DeinitVDisaDiffDrive();
}

bool InitVirtualImageDrive(void) {
    DeinitVirtualImageDrive();
    return InitVGameDrive() || InitVDisaDiffDrive() || InitVKeyDbDrive();
}

bool CheckVirtualDrive(const char* path) {
    u32 virtual_src = GetVirtualSource(path);
    if (virtual_src & (VRT_EMUNAND|VRT_IMGNAND))
        return CheckVNandDrive(virtual_src); // check virtual NAND drive for EmuNAND / ImgNAND
    else if (virtual_src & VRT_VRAM)
        return CheckVVramDrive();
    else if (virtual_src & VRT_GAME)
        return CheckVGameDrive();
    else if (virtual_src & VRT_BDRI)
        return CheckVBDRIDrive();
    else if (virtual_src & VRT_KEYDB)
        return CheckVKeyDbDrive();
    else if (virtual_src & VRT_DISADIFF)
        return CheckVDisaDiffDrive();
    return virtual_src; // this is safe for SysNAND & memory
}

bool ReadVirtualDir(VirtualFile* vfile, VirtualDir* vdir) {
    u32 virtual_src = vdir->flags & VRT_SOURCE;
    bool ret = false;
    if (virtual_src & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND|VRT_XORPAD)) {
        ret = ReadVNandDir(vfile, vdir);
    } else if (virtual_src & VRT_MEMORY) {
        ret = ReadVMemDir(vfile, vdir);
    } else if (virtual_src & VRT_GAME) {
        ret = ReadVGameDir(vfile, vdir);
    } else if (virtual_src & VRT_BDRI) {
        ret = ReadVBDRIDir(vfile, vdir);
    } else if (virtual_src & VRT_KEYDB) {
        ret = ReadVKeyDbDir(vfile, vdir);
    } else if (virtual_src & VRT_CART) {
        ret = ReadVCartDir(vfile, vdir);
    } else if (virtual_src & VRT_VRAM) {
        ret = ReadVVramDir(vfile, vdir);
    } else if (virtual_src & VRT_DISADIFF) {
        ret = ReadVDisaDiffDir(vfile, vdir);
    }
    vfile->flags |= virtual_src; // add source flag
    return ret;
}

bool OpenVirtualRoot(VirtualDir* vdir, u32 virtual_src) {
    if (virtual_src & VRT_GAME) {
        if (!OpenVGameDir(vdir, NULL)) return false;
    } else { // generic vdir object
        vdir->offset = 0;
        vdir->size = 0;
        vdir->flags = 0;
    }
    vdir->index = -1;
    vdir->flags |= VFLAG_DIR|virtual_src;
    return true;
}

bool OpenVirtualDir(VirtualDir* vdir, VirtualFile* ventry) {
    u32 virtual_src = ventry->flags & VRT_SOURCE;
    if (ventry->flags & VFLAG_ROOT)
        return OpenVirtualRoot(vdir, virtual_src);
    if (virtual_src & VRT_GAME) {
        if (!OpenVGameDir(vdir, ventry)) return false;
    } else {
        vdir->index = -1;
        vdir->offset = ventry->offset;
        vdir->size = ventry->size;
        vdir->flags = ventry->flags;
    }
    vdir->flags |= virtual_src;
    return true;
}

bool GetVirtualFile(VirtualFile* vfile, const char* path, u8 mode) {
    char lpath[256];
    strncpy(lpath, path, 256);
    lpath[255] = '\0';

    // get virtual source / root dir object
    u32 virtual_src = 0;
    virtual_src = GetVirtualSource(path);
    if (!virtual_src) return false;

    // set vfile as root object
    memset(vfile, 0, sizeof(VirtualDir));
    vfile->flags = VFLAG_ROOT|virtual_src;
    if (strnlen(lpath, 256) <= 3) return true;

    // tokenize / parse path
    char* name;
    VirtualDir vdir;
    if (!OpenVirtualRoot(&vdir, virtual_src)) return false;
    for (name = strtok(lpath + 3, "/"); name && vdir.flags; name = strtok(NULL, "/")) {
        if (!(vdir.flags & VFLAG_LV3)) { // standard method
            while (true) {
                if (!ReadVirtualDir(vfile, &vdir))
                    return ((mode & FA_WRITE) && (vdir.flags & VRT_BDRI) && GetNewVBDRIFile(vfile, &vdir, path));
                if ((!(vfile->flags & (VRT_GAME|VRT_VRAM)) && (strncasecmp(name, vfile->name, 32) == 0)) ||
                    ((vfile->flags & VRT_GAME) && MatchVGameFilename(name, vfile, 256)) ||
                    ((vfile->flags & VRT_VRAM) && MatchVVramFilename(name, vfile)))
                    break; // entry found
            }
        } else { // use lv3 hashes for quicker search
            if (!FindVirtualFileInLv3Dir(vfile, &vdir, name))
                return false;
        }
        if (!OpenVirtualDir(&vdir, vfile))
            vdir.flags = 0;
    }

    return (name == NULL); // if name is NULL, this succeeded
}

bool GetVirtualDir(VirtualDir* vdir, const char* path) {
    VirtualFile vfile;
    return GetVirtualFile(&vfile, path, 0) && OpenVirtualDir(vdir, &vfile);
}

bool GetVirtualFilename(char* name, const VirtualFile* vfile, u32 n_chars) {
    if (vfile->flags & VRT_GAME) return GetVGameFilename(name, vfile, n_chars);
    else if (vfile->flags & VRT_VRAM) return GetVVramFilename(name, vfile);

    strncpy(name, vfile->name, n_chars);
    return true;
}

int ReadVirtualFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count, u32* bytes_read) {
    // basic check of offset / count
    if (offset >= vfile->size)
        return 0;
    else if ((offset + count) > vfile->size)
        count = vfile->size - offset;
    if (bytes_read) *bytes_read = count;

    if (vfile->flags & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND|VRT_XORPAD)) {
        return ReadVNandFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_MEMORY) {
        return ReadVMemFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_GAME) {
        return ReadVGameFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_BDRI) {
        return ReadVBDRIFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_KEYDB) {
        return ReadVKeyDbFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_CART) {
        return ReadVCartFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_VRAM) {
        return ReadVVramFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_DISADIFF) {
        return ReadVDisaDiffFile(vfile, buffer, offset, count);
    }

    return -1;
}

int WriteVirtualFile(VirtualFile* vfile, const void* buffer, u64 offset, u64 count, u32* bytes_written) {
    // basic check of offset / count
    if (offset >= vfile->size)
        return 0;
    else if (!(vfile->flags & VRT_BDRI) && ((offset + count) > vfile->size))
        count = vfile->size - offset;
    if (bytes_written) *bytes_written = count;

    if (vfile->flags & VFLAG_READONLY) {
        return -1;
    } else if (vfile->flags & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND)) {
        return WriteVNandFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_MEMORY) {
        return WriteVMemFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_DISADIFF) {
        return WriteVDisaDiffFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_CART) {
        return WriteVCartFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_BDRI) {
        return WriteVBDRIFile(vfile, buffer, offset, count);
    } // no write support for virtual game / keydb / vram files

    return -1;
}

int DeleteVirtualFile(const VirtualFile* vfile) {
    if (!(vfile->flags & VFLAG_DELETABLE)) return -1;

    // Special handling for deleting BDRI entries
    if (vfile->flags & VRT_BDRI)
        return DeleteVBDRIFile(vfile);

    // For anything else, "deleting" is just filling with 0s
    u32 zeroes_size = STD_BUFFER_SIZE;
    u8* zeroes = (u8*) malloc(zeroes_size);
    if (!zeroes) return -1;
    memset(zeroes, 0x00, zeroes_size);

    int result = 0;
    for (u64 pos = 0; pos < vfile->size; pos += zeroes_size) {
        u64 wipe_bytes = min(zeroes_size, vfile->size - pos);
        result = WriteVirtualFile((VirtualFile*)vfile, zeroes, pos, wipe_bytes, NULL);
        if (result != 0) break;
    }

    free(zeroes);
    return result;
}

u64 GetVirtualDriveSize(const char* path) {
    u32 virtual_src = GetVirtualSource(path);
    if (virtual_src & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND))
        return GetVNandDriveSize(virtual_src);
    else if (virtual_src & VRT_GAME)
        return GetVGameDriveSize();
    else if (virtual_src & VRT_BDRI)
        return GetVBDRIDriveSize();
    else if (virtual_src & VRT_KEYDB)
        return GetVKeyDbDriveSize();
    else if (virtual_src & VRT_CART)
        return GetVCartDriveSize();
    else if (virtual_src & VRT_VRAM)
        return GetVVramDriveSize();
    return 0;
}
