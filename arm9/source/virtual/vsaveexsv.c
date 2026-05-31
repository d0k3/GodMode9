#include "disadiff.h"
#include "image.h"
#include "vdisadiff.h"
#include "vsaveexsv.h"
#include "saveexsv.h"
#include "filetype.h"

static SaveExsvFile sav = { 0 };

static char exsv_root_path[256] = { 0 };
static char exsv_file_tmp_path[256] = { 0 };
static DisaDiffRWInfo exsv_cur_file_cache = { 0 };
static u64 last_exsv_file_id = 0;

static int LoadExsvDiffFile(u64 id, u64 uniq) {
    if (last_exsv_file_id == id && exsv_cur_file_cache.unique_id == uniq) {
        return 0;
    }

    u64 dir = id / 126;
    if (dir > 0xFFFFFFFF) // there can be no more than 0xFFFFFFFF dirs (including dir 0)
        return 1;
    u32 file = id % 126;

    snprintf(exsv_file_tmp_path, sizeof(exsv_file_tmp_path), "%s%08lX/%08lX", exsv_root_path, (u32)dir, file);
    if (GetDisaDiffRWInfo(exsv_file_tmp_path, &exsv_cur_file_cache, false) || exsv_cur_file_cache.unique_id != uniq) {
        return 1;
    }

    last_exsv_file_id = id;
    return 0;
}

u64 InitVSaveDrive(void) { // prerequisite: save mounted as virtual disa image
    u64 mount_state = CheckVDisaDiffDrive();
    if (!(mount_state & (SYS_DISA | SYS_DIFF))) return 0;

    DeinitVSaveDrive();

    if (SaveExsvFileInit(&sav) != 0) {
        DeinitVSaveDrive();
        return 0;
    }

    if (sav.is_exsv) {
        // ensure we're initializing extdata from a correct structure
        const char *exsv_root = GetMountPath();
        int pathlen = strlen(exsv_root);

        if (pathlen < 20 || strcmp(&exsv_root[pathlen - 17], "00000000/00000001") != 0) {
            DeinitVSaveDrive();
            return 0;
        }

        strncpy(exsv_root_path, exsv_root, pathlen - 17);
        exsv_root_path[pathlen - 17] = 0;
    }

    return mount_state;
}

u64 CheckVSaveDrive(void) {
    u64 mount_state = CheckVDisaDiffDrive();
    return ((mount_state & (SYS_DISA | SYS_DIFF)) && sav.init_ok) ? mount_state : 0;
}

void DeinitVSaveDrive() {
    SaveExsvFileFree(&sav);
    memset(exsv_root_path, 0, sizeof(exsv_root_path));
    memset(exsv_file_tmp_path, 0, sizeof(exsv_file_tmp_path));
    memset(&exsv_cur_file_cache, 0, sizeof(exsv_cur_file_cache));
    last_exsv_file_id = 0;
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
        if (sav.is_exsv) {
            if (LoadExsvDiffFile(sav.exsv_extra_hdr.sub_file_base_id + vdir->offset, fent->ent.file_unique_id) != 0) {
                vfile->size = 0;
            } else {
                vfile->size = exsv_cur_file_cache.size_ivfc_lvl4;
            }
        } else {
            vfile->size = fent->ent.file_size;
        }
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
    if (sav.is_exsv) {
        SaveFileEntry *fent = &sav.file_entries[vfile->offset];
        if (LoadExsvDiffFile(sav.exsv_extra_hdr.sub_file_base_id + vfile->offset, fent->ent.file_unique_id) != 0)
            return 1;

        return ReadDisaDiffIvfcLvl4(exsv_file_tmp_path, &exsv_cur_file_cache, offset, count, buffer) == count ? 0 : 1;
    } else {
        return SaveExsvReadFatFile(&sav, vfile->offset, buffer, (u32)offset, count);
    }
}

u64 GetVSaveDriveSize(void) {
    if (!CheckVSaveDrive())
        return 0;

    if (sav.is_exsv) {
        return 0; // maybe implement Quota.dat reading for this at some point
    } else {
        return sav.pre_header.fs_image_size_blocks * sav.pre_header.fs_image_blocksize;
    }
}
