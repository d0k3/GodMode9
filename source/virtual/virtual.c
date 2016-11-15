#include "virtual.h"
#include "vnand.h"
#include "vmem.h"

typedef struct {
    char drv_letter;
    u32 virtual_src;
} __attribute__((packed)) VirtualDrive;

static const VirtualDrive virtualDrives[] = { {'S', VRT_SYSNAND}, {'E', VRT_EMUNAND}, {'I', VRT_IMGNAND}, {'M', VRT_MEMORY} };

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
    return virtual_src; // this is safe for SysNAND & memory
}

bool ReadVirtualDir(VirtualFile* vfile, u32 virtual_src) {
    if (virtual_src & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND)) {
        return ReadVNandDir(vfile, virtual_src);
    } else if (virtual_src & VRT_MEMORY) {
        return ReadVMemDir(vfile);
    }
    return false;
}

bool FindVirtualFile(VirtualFile* vfile, const char* path, u32 size) {
    // get / fix the name
    char* fname = strchr(path, '/');
    if (!fname) return false;
    fname++;
    
    // check path validity / get virtual source
    u32 virtual_src = 0;
    virtual_src = GetVirtualSource(path);
    if (!virtual_src || (fname - path != 3))
        return false;
    
    // read virtual dir, match the path / size
    ReadVirtualDir(NULL, virtual_src); // reset dir reader
    while (ReadVirtualDir(vfile, virtual_src)) {
        vfile->flags |= virtual_src; // add source flag
        if (((strncasecmp(fname, vfile->name, 32) == 0) ||
            (size && (vfile->size == size)))) // search by size should be a last resort solution
            return true; // file found
    }
    
    // failed if arriving
    return false;
}

bool GetVirtualDirContents(DirStruct* contents, const char* path, const char* pattern) {
    u32 virtual_src = GetVirtualSource(path);
    if (!virtual_src) return false; // not a virtual path
    if (strchr(path, '/')) return false; // only top level paths
    
    VirtualFile vfile;
    ReadVirtualDir(NULL, virtual_src); // reset dir reader
    while ((contents->n_entries < MAX_DIR_ENTRIES) && (ReadVirtualDir(&vfile, virtual_src))) {
        DirEntry* entry = &(contents->entry[contents->n_entries]);
        if (pattern && !MatchName(pattern, vfile.name)) continue;
        snprintf(entry->path, 256, "%s/%s", path, vfile.name);
        entry->name = entry->path + strnlen(path, 256) + 1;
        entry->size = vfile.size;
        entry->type = T_FILE;
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
    }
    
    return -1;
}
