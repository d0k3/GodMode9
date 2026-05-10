#include "vdisadiff.h"
#include "filetype.h"
#include "vsave.h"
#include "save.h"
#include "vff.h"
#include "ff.h"

#define PART_A_PATH "D:/partitionA.bin"
#define PART_B_PATH "D:/partitionB.bin"

static FIL part_a = { 0 }, part_b = { 0 };
static SaveFile sav = { 0 };

u64 InitVSaveDrive(void) { // prerequisite: save mounted as virtual disa image
    u64 mount_state = CheckVDisaDiffDrive();
    if (!(mount_state & SYS_DISA)) return 0;

    DeinitVSaveDrive();

    if (SaveFileInit(&sav))
        DeinitVSaveDrive();

    return mount_state;
}

u64 CheckVSaveDrive(void) {
    u64 mount_state = CheckVDisaDiffDrive();
    return ((mount_state & SYS_DISA) && sav.init_ok) ? mount_state : 0;
}

void DeinitVSaveDrive() {
    SaveFileFree(&sav);
    fvx_close(&part_a);
    fvx_close(&part_b);
}

bool ReadVSaveDir(VirtualFile* vfile, VirtualDir* vdir) {
    if (!CheckVSaveDrive())
        return false;
    
    if (vdir->index == -1)
        vdir->index = 0;


    if (vdir->index == 0) {
        u32 dir_offset = vdir->offset == 0 ? 1 : vdir->offset;
        SaveDirectoryEntry *dir = &sav.dir_entries[dir_offset];
        if (!dir->ent.first_file_index) {
            vdir->index = 2;
        } else {
            vdir->index = 1;
            vdir->offset = dir->ent.first_file_index;
        }
    }

    if (vdir->index == 1) {
        SaveFileEntry *fent = &sav.file_entries[vdir->offset];
        vfile->offset = vdir->offset;
        vfile->flags = VFLAG_READONLY;
        vfile->size = fent->ent.file_size;
        memset(vfile->name, 0, sizeof(vfile->name));
        memcpy(vfile->name, fent->ent.key.name, sizeof(fent->ent.key.name));

        if (fent->ent.next_sibling_index) {
            vdir->offset = fent->ent.next_sibling_index;
        } else {
            vdir->offset = fent->ent.key.parent_dir_idx;
            vdir->index = 2;
        }

        return true;
    }

    if (vdir->index == 2) {
        SaveDirectoryEntry *dir = &sav.dir_entries[vdir->offset];
        if (!dir->ent.first_subdir_index)
            return false;
        else {
            vdir->index = 3;
            vdir->offset = dir->ent.first_subdir_index;
        }
    }

    if (vdir->index == 3) {
        SaveDirectoryEntry *dir = &sav.dir_entries[vdir->offset];
        vfile->offset = vdir->offset;
        vfile->flags = VFLAG_READONLY | VFLAG_DIR;
        memset(vfile->name, 0, sizeof(vfile->name));
        memcpy(vfile->name, dir->ent.key.name, sizeof(dir->ent.key.name));

        if (dir->ent.next_sibling_dir_index) {
            vdir->offset = dir->ent.next_sibling_dir_index;
        } else {
            vdir->index = 4;
        }

        return true;
    }
    
    return false;
}

int ReadVSaveFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    if (vfile->flags & VFLAG_DIR)
        return 1;
    return SaveReadFile(&sav, vfile->offset, buffer, (u32)offset, count);
}

u64 GetVSaveDriveSize(void) {
    if (!CheckVSaveDrive())
        return 0;

    return sav.header.fs_image_size_blocks * sav.header.fs_image_blocksize;
}