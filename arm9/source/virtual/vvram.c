#include "vvram.h"
#include "vram0.h"


bool SplitTarFName(char* tar_fname, char** dir, char** name) {
    u32 len = strnlen(tar_fname, 100 + 1);
    if (!len || (len == 101)) return false;
    
    // remove trailing slash
    if (tar_fname[len-1] == '/') tar_fname[--len] = '\0';
    
    // find last slash
    char* slash = strrchr(tar_fname, '/');
    
    // relative root dir entry
    if (!slash) {
        *name = tar_fname;
        *dir = NULL;
    } else {
        *slash = '\0';
        *name = slash + 1;
        *dir = tar_fname;
    }
    
    return true;
}


bool CheckVVramDrive(void) {
    return CheckVram0Tar();
}

bool ReadVVramDir(VirtualFile* vfile, VirtualDir* vdir) {
    vfile->name[0] = '\0';
    vfile->flags = VFLAG_READONLY;
    vfile->keyslot = 0xFF;
    
    
    // get current dir name
    char curr_dir[100 + 1];
    if (vdir->offset == (u64) -1) return false; // end of the dir?
    else if (!vdir->offset) *curr_dir = '\0'; // relative root?
    else {
        // vdir->offset is offset of dir entry + 0x200
        TarHeader* tar = (TarHeader*) OffsetVTarEntry(vdir->offset - 0x200);
        strncpy(curr_dir, tar->fname, 100);
        u32 len = strnlen(curr_dir, 100 + 1);
        if (len == 101) return false; // path error
        if (curr_dir[len-1] == '/') curr_dir[len-1] = '\0';
    }
    
    
    // using vdir index to signify the position limits us to 1TiB TARs
    void* tardata = NULL;
    if (vdir->index < 0) tardata = FirstVTarEntry();
    else tardata = NextVTarEntry(OffsetVTarEntry(vdir->index << 9));
    
    if (tardata) do {
        TarHeader* tar = (TarHeader*) tardata;
        char tar_fname[100 + 1];
        char *name, *dir;
        
        strncpy(tar_fname, tar->fname, 100);
        if (!SplitTarFName(tar_fname, &dir, &name)) return false;
        if ((!dir && !*curr_dir) || (dir && (strncmp(dir, curr_dir, 100) == 0))) break;
    } while ((tardata = NextVTarEntry(tardata)));
    
    // match found?
    if (tardata) {
        u64 fsize;
        bool is_dir;
        void* fdata = GetVTarFileInfo(tardata, NULL, &fsize, &is_dir);
        
        vfile->offset = (u32) fdata - VRAM0_OFFSET;
        vfile->size = fsize;
        if (is_dir) vfile->flags |= VFLAG_DIR;
        
        vdir->index = (vfile->offset >> 9) - 1;
    } else { // not found
        vdir->offset = (u64) -1;
        return false;
    }
    
    
    return true;
}

int ReadVVramFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    if (vfile->flags & VFLAG_DIR) return -1;
    void* fdata = (u8*) VRAM0_OFFSET + vfile->offset;
    
    // range checks in virtual.c
    memcpy(buffer, (u8*) fdata + offset, count);
    return 0;
}

bool GetVVramFilename(char* name, const VirtualFile* vfile) {
    void* tardata = OffsetVTarEntry(vfile->offset - 0x200);
    TarHeader* tar = (TarHeader*) tardata;
    char tar_fname[100 + 1];
    char *name_tmp, *dir;
    
    strncpy(tar_fname, tar->fname, 100);
    if (!SplitTarFName(tar_fname, &dir, &name_tmp)) return false;
    strncpy(name, name_tmp, 100);
    
    return true;
}

bool MatchVVramFilename(const char* name, const VirtualFile* vfile) {
    void* tardata = OffsetVTarEntry(vfile->offset - 0x200);
    TarHeader* tar = (TarHeader*) tardata;
    char tar_fname[100 + 1];
    char *name_tmp, *dir;
    
    strncpy(tar_fname, tar->fname, 100);
    if (!SplitTarFName(tar_fname, &dir, &name_tmp)) return false;
    return (strncasecmp(name, name_tmp, 100) == 0);
}

u64 GetVVramDriveSize(void) {
    return VRAM0_LIMIT;
}
