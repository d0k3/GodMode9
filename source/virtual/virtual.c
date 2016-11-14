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

bool FindVirtualFile(VirtualFile* vfile, const char* path, u32 size)
{
    // get / fix the name
    char* fname = strchr(path, '/');
    if (!fname) return false;
    fname++;
    
    // check path validity / get virtual source
    u32 virtual_src = 0;
    virtual_src = GetVirtualSource(path);
    if (!virtual_src || (fname - path != 3))
        return false;
    
    // get virtual file struct from appropriate function
    if (virtual_src & (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND)) {
        if (!FindVNandFile(vfile, virtual_src, fname, size)) return false;
    } else if (virtual_src & VRT_MEMORY) {
        if (!FindVMemFile(vfile, fname, size)) return false;
    } else return false;
    
    // add the virtual source to the virtual file flags
    vfile->flags |= virtual_src;
    
    return true;
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
