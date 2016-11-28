#include "virtual.h"
#include "vnand.h"
#include "vmem.h"
#include "vgame.h"

typedef struct {
    char drv_letter;
    u32 virtual_src;
} __attribute__((packed)) VirtualDrive;

static const VirtualDrive virtualDrives[] =
    { {'S', VRT_SYSNAND}, {'E', VRT_EMUNAND}, {'I', VRT_IMGNAND}, {'M', VRT_MEMORY}, {'G', VRT_GAME} };

u32 GetVirtualSource(const char* path) {
    // check path validity
    if ((strnlen(path, 16) < 2) || (path[1] != ':') || ((path[2] != '/') && (path[2] != '\0')))
        return 0;
    // search for virtual source
    for (u32 i = 0; i < (sizeof(virtualDrives) / sizeof(VirtualDrive)); i++)
        if (*path == virtualDrives[i].drv_letter) return virtualDrives[i].virtual_src;
    return 0;
}

bool CheckVirtualDrive(const char* path) {
    u32 virtual_src = GetVirtualSource(path);
    if (virtual_src & (VRT_EMUNAND|VRT_IMGNAND))
        return CheckVNandDrive(virtual_src); // check virtual NAND drive for EmuNAND / ImgNAND
    else if (virtual_src & VRT_GAME)
        return CheckVGameDrive();
    return virtual_src; // this is safe for SysNAND & memory
}

bool ReadVirtualDir(VirtualFile* vfile, VirtualDir* vdir) {
    u32 virtual_src = vdir->virtual_src;
    bool ret = false;
    if (virtual_src & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND)) {
        ret = ReadVNandDir(vfile, vdir);
    } else if (virtual_src & VRT_MEMORY) {
        ret = ReadVMemDir(vfile, vdir);
    } else if (virtual_src & VRT_GAME) {
        ret = ReadVGameDir(vfile, vdir);
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
    vdir->virtual_src = virtual_src;
    return true;
}

bool OpenVirtualDir(VirtualDir* vdir, VirtualFile* ventry) {
    u32 virtual_src = ventry->flags & VRT_SOURCE;
    if (ventry->flags & VFLAG_ROOT)
        return OpenVirtualRoot(vdir, virtual_src);
    if (!(virtual_src & VRT_GAME)) return false; // no subdirs in other virtual sources
    if (!OpenVGameDir(vdir, ventry)) return false;
    vdir->flags |= virtual_src;
    vdir->virtual_src = virtual_src;
    return true;
}

bool GetVirtualFile(VirtualFile* vfile, const char* path) {
    char lpath[256];
    strncpy(lpath, path, 256);
    
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
    for (name = strtok(lpath + 3, "/"); name && vdir.virtual_src; name = strtok(NULL, "/")) {
        while (true) {
            if (!ReadVirtualDir(vfile, &vdir)) return false;
            if (strncasecmp(name, vfile->name, 32) == 0)
                break; // entry found
        }
        if (!OpenVirtualDir(&vdir, vfile))
            vdir.virtual_src = 0;
    }
    
    return (name == NULL); // if name is NULL, this succeeded
}

bool GetVirtualDir(VirtualDir* vdir, const char* path) {
    VirtualFile vfile;
    return GetVirtualFile(&vfile, path) && OpenVirtualDir(vdir, &vfile);
}

// hacky solution, actually ignores path
bool FindVirtualFileBySize(VirtualFile* vfile, const char* path, u32 size) {
    // get virtual source
    u32 virtual_src = 0;
    virtual_src = GetVirtualSource(path);
    if (!virtual_src) return false;
    
    VirtualDir vdir; // read virtual root dir, match size
    OpenVirtualRoot(&vdir, virtual_src); // get dir reader object
    while (ReadVirtualDir(vfile, &vdir)) {
        vfile->flags |= virtual_src; // add source flag
        if (vfile->size == size) // search by size should be a last resort solution
            return true; // file found
    }
    
    // failed if arriving here
    return false;
}

bool GetVirtualDirContents(DirStruct* contents, const char* path, const char* pattern) {
    u32 virtual_src = GetVirtualSource(path);
    if (!virtual_src) return false; // not a virtual path
    
    VirtualDir vdir;
    VirtualFile vfile;
    if (!GetVirtualDir(&vdir, path))
        return false; // get dir reader object
    while ((contents->n_entries < MAX_DIR_ENTRIES) && (ReadVirtualDir(&vfile, &vdir))) {
        DirEntry* entry = &(contents->entry[contents->n_entries]);
        if (pattern && !MatchName(pattern, vfile.name)) continue;
        snprintf(entry->path, 256, "%s/%s", path, vfile.name);
        entry->name = entry->path + strnlen(path, 256) + 1;
        entry->size = vfile.size;
        entry->type = (vfile.flags & VFLAG_DIR) ? T_DIR : T_FILE;
        entry->marked = 0;
        contents->n_entries++;
    }
    
    return true; // not much we can check here
}

int ReadVirtualFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count, u32* bytes_read)
{
    // basic check of offset / count
    if (offset >= vfile->size)
        return 0;
    else if ((offset + count) > vfile->size)
        count = vfile->size - offset;
    if (bytes_read) *bytes_read = count;
    
    if (vfile->flags & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND)) {
        return ReadVNandFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_MEMORY) {
        return ReadVMemFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_GAME) {
        return ReadVGameFile(vfile, buffer, offset, count);
    }
    
    return -1;
}

int WriteVirtualFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count, u32* bytes_written)
{
    // basic check of offset / count
    if (offset >= vfile->size)
        return 0;
    else if ((offset + count) > vfile->size)
        count = vfile->size - offset;
    if (bytes_written) *bytes_written = count;
    
    if (vfile->flags & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND)) {
        return WriteVNandFile(vfile, buffer, offset, count);
    } else if (vfile->flags & VRT_MEMORY) {
        return WriteVMemFile(vfile, buffer, offset, count);
    } // no write support for virtual game files
    
    return -1;
}
