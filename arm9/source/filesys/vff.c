#include "sddata.h"
#include "virtual.h"
#include "ffconf.h"
#include "vff.h"

#if FF_USE_LFN != 0
#define _MAX_FN_LEN (FF_MAX_LFN)
#else
#define _MAX_FN_LEN (8+3)
#endif
    
#define _VFIL_ENABLED    (!_FS_TINY)
#define _VDIR_ENABLED    ((sizeof(DIR) - sizeof(FFOBJID) >= sizeof(VirtualDir)) && (FF_USE_LFN != 0))

#define VFIL(fp) ((VirtualFile*) (void*) fp->buf)
#define VDIR(dp) ((VirtualDir*) (void*) &(dp->dptr))

FRESULT fvx_open (FIL* fp, const TCHAR* path, BYTE mode) {
    #if _VFIL_ENABLED
    VirtualFile* vfile = VFIL(fp);
    memset(fp, 0, sizeof(FIL));
    if (GetVirtualFile(vfile, path)) {
        fp->obj.fs = NULL;
        fp->obj.objsize = vfile->size;
        fp->fptr = 0;
        return FR_OK;
    }
    #endif
    return fx_open ( fp, path, mode );
}

FRESULT fvx_read (FIL* fp, void* buff, UINT btr, UINT* br) {
    #if _VFIL_ENABLED
    if (fp->obj.fs == NULL) {
        VirtualFile* vfile = VFIL(fp);
        int res = ReadVirtualFile(vfile, buff, fp->fptr, btr, (u32*) br);
        fp->fptr += *br;
        return res;
    }
    #endif
    return fx_read ( fp, buff, btr, br );
}

FRESULT fvx_write (FIL* fp, const void* buff, UINT btw, UINT* bw) {
    #if _VFIL_ENABLED
    if (fp->obj.fs == NULL) {
        VirtualFile* vfile = VFIL(fp);
        int res = WriteVirtualFile(vfile, buff, fp->fptr, btw, (u32*) bw);
        fp->fptr += *bw;
        return res;
    }
    #endif
    return fx_write ( fp, buff, btw, bw );
}

FRESULT fvx_close (FIL* fp) {
    #if _VFIL_ENABLED
    if (fp->obj.fs == NULL) return FR_OK;
    #endif
    return fx_close( fp );
}

FRESULT fvx_lseek (FIL* fp, FSIZE_t ofs) {
    #if _VFIL_ENABLED
    if (fp->obj.fs == NULL) {
        if (fvx_size(fp) >= ofs) {
            fp->fptr = ofs;
            return FR_OK;
        } else return FR_DENIED;
    }
    #endif
    return f_lseek( fp, ofs );
}

FRESULT fvx_sync (FIL* fp) {
    #if _VFIL_ENABLED
    if (fp->obj.fs == NULL) return FR_OK;
    #endif
    return f_sync( fp );
}

FRESULT fvx_stat (const TCHAR* path, FILINFO* fno) {
    if (GetVirtualSource(path)) {
        VirtualFile vfile;
        if (!GetVirtualFile(&vfile, path)) return FR_NO_PATH;
        if (fno) {
            fno->fsize = vfile.size;
            fno->fdate = (1<<5)|(1<<0); // 1 for month / day
            fno->ftime = 0;
            fno->fattrib = (vfile.flags & VFLAG_DIR) ? (AM_DIR|AM_VRT) : AM_VRT;
            // could be better...
            if (FF_USE_LFN != 0) GetVirtualFilename(fno->fname, &vfile, FF_MAX_LFN + 1);
        }
        return FR_OK;
    } else return fa_stat( path, fno );
}

FRESULT fvx_rename (const TCHAR* path_old, const TCHAR* path_new) {
    if ((GetVirtualSource(path_old)) || CheckAliasDrive(path_old)) return FR_DENIED;
    return f_rename( path_old, path_new );
}

FRESULT fvx_unlink (const TCHAR* path) {
    if (GetVirtualSource(path)) {
        VirtualFile vfile;
        if (!GetVirtualFile(&vfile, path)) return FR_NO_PATH;
        if (DeleteVirtualFile(&vfile) != 0) return FR_DENIED;
        return FR_OK;
    } else return fa_unlink( path );
}

FRESULT fvx_mkdir (const TCHAR* path) {
    if (GetVirtualSource(path)) return FR_DENIED;
    return fa_mkdir( path );
}

FRESULT fvx_opendir (DIR* dp, const TCHAR* path) {
    if (_VDIR_ENABLED) {
        VirtualDir* vdir = VDIR(dp);
        memset(dp, 0, sizeof(DIR));
        if (GetVirtualDir(vdir, path))
            return FR_OK;
    }
    return fa_opendir( dp, path );
}

FRESULT fvx_closedir (DIR* dp) {
    if (_VDIR_ENABLED) {
        if (dp->obj.fs == NULL) return FR_OK;
    }
    return f_closedir( dp );
}

FRESULT fvx_readdir (DIR* dp, FILINFO* fno) {
    if (_VDIR_ENABLED) {
        if (dp->obj.fs == NULL) {
            VirtualDir* vdir = VDIR(dp);
            VirtualFile vfile;
            if (ReadVirtualDir(&vfile, vdir)) {
                fno->fsize = vfile.size;
                fno->fdate = fno->ftime = 0;
                fno->fattrib = (vfile.flags & VFLAG_DIR) ? (AM_DIR|AM_VRT) : AM_VRT;
                GetVirtualFilename(fno->fname, &vfile, FF_MAX_LFN + 1);
            } else *(fno->fname) = 0;
            return FR_OK;
        }
    }
    return f_readdir( dp, fno );
}

FRESULT fvx_qread (const TCHAR* path, void* buff, FSIZE_t ofs, UINT btr, UINT* br) {
    FIL fp;
    FRESULT res;
    UINT brt = 0;
    
    res = fvx_open(&fp, path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) return res;
    
    res = fvx_lseek(&fp, ofs);
    if (res != FR_OK) {
        fvx_close(&fp);
        return res;
    }
    
    res = fvx_read(&fp, buff, btr, &brt);
    fvx_close(&fp);
    
    if (br) *br = brt;
    else if ((res == FR_OK) && (brt != btr)) res = FR_DENIED;
    
    return res;
}

FRESULT fvx_qwrite (const TCHAR* path, const void* buff, FSIZE_t ofs, UINT btw, UINT* bw) {
    FIL fp;
    FRESULT res;
    UINT bwt = 0;
    
    res = fvx_open(&fp, path, FA_WRITE | FA_OPEN_ALWAYS);
    if (res != FR_OK) return res;
    
    res = fvx_lseek(&fp, ofs);
    if (res != FR_OK) {
        fvx_close(&fp);
        return res;
    }
    
    res = fvx_write(&fp, buff, btw, &bwt);
    fvx_close(&fp);
    
    if (bw) *bw = bwt;
    else if ((res == FR_OK) && (bwt != btw)) res = FR_DENIED;
    
    return res;
}

FSIZE_t fvx_qsize (const TCHAR* path) {
    FILINFO fno;
    return (fvx_stat(path, &fno) == FR_OK) ? fno.fsize : 0;
}

#if !_LFN_UNICODE // this will not work for unicode
FRESULT worker_fvx_rmkdir (TCHAR* tpath) {
    DIR tmp_dir;
    if (fa_opendir(&tmp_dir, tpath) != FR_OK) {
        TCHAR* slash = strrchr(tpath, '/');
        if (!slash) return FR_DENIED;
        *slash = '\0';
        FRESULT res;
        if ((res = worker_fvx_rmkdir(tpath)) != FR_OK) return res;
        *slash = '/';
        return fa_mkdir(tpath);
    } else {
        f_closedir(&tmp_dir);
        return FR_OK;
    }
}
#endif

FRESULT fvx_rmkdir (const TCHAR* path) {
    #if !_LFN_UNICODE // this will not work for unicode
    TCHAR tpath[_MAX_FN_LEN+1];
    strncpy(tpath, path, _MAX_FN_LEN);
    return worker_fvx_rmkdir( tpath );
    #else
    return FR_DENIED;
    #endif
}

FRESULT fvx_rmkpath (const TCHAR* path) {
    #if !_LFN_UNICODE // this will not work for unicode
    TCHAR tpath[_MAX_FN_LEN+1];
    strncpy(tpath, path, _MAX_FN_LEN);
    TCHAR* slash = strrchr(tpath, '/');
    if (!slash) return FR_DENIED;
    *slash = '\0';
    return worker_fvx_rmkdir( tpath );
    #else
    return FR_DENIED;
    #endif
}

#if !_LFN_UNICODE // this will not work for unicode
FRESULT worker_fvx_runlink (TCHAR* tpath) {
    FILINFO fno;
    FRESULT res;
    
    // this code handles directory content deletion
    if ((res = fvx_stat(tpath, &fno)) != FR_OK) return res; // tpath does not exist
    if (fno.fattrib & AM_DIR) { // process folder contents
        DIR pdir;
        TCHAR* fname = tpath + strnlen(tpath, 255);
        
        
        if ((res = fa_opendir(&pdir, tpath)) != FR_OK) return res;
        *(fname++) = '/';
        
        while (fvx_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            strncpy(fname, fno.fname, tpath + 255 - fname);
            if (fno.fname[0] == 0) {
                break;
            } else { // return value won't matter
                worker_fvx_runlink(tpath);
            }
        }
        fvx_closedir(&pdir);
        *(--fname) = '\0';
    }
    
    return fvx_unlink( tpath );
}
#endif

FRESULT fvx_runlink (const TCHAR* path) {
    #if !_LFN_UNICODE // this will not work for unicode
    TCHAR tpath[_MAX_FN_LEN+1];
    strncpy(tpath, path, _MAX_FN_LEN);
    return worker_fvx_runlink( tpath );
    #else
    return FR_DENIED;
    #endif
}

// inspired by http://www.geeksforgeeks.org/wildcard-character-matching/
FRESULT fvx_match_name(const TCHAR* path, const TCHAR* pattern) {
    // handling non asterisk chars
    for (; *pattern != '*'; pattern++, path++) {
        if ((*pattern == '\0') && (*path == '\0')) {
            return FR_OK; // end reached simultaneously, match found
        } else if ((*pattern == '\0') || (*path == '\0')) {
            return FR_NO_FILE; // end reached on only one, failure
        } else if ((*pattern != '?') && (tolower(*pattern) != tolower(*path))) {
            return FR_NO_FILE; // chars don't match, failure
        }
    }
    // handling the asterisk (matches one or more chars in path)
    if ((*(pattern+1) == '?') || (*(pattern+1) == '*')) {
        return FR_NO_FILE; // stupid user shenanigans, failure
    } else if (*path == '\0') {
        return FR_NO_FILE; // asterisk, but end reached on path, failure
    } else if (*(pattern+1) == '\0') {
        return FR_OK; // nothing after the asterisk, match found
    } else { // we couldn't really go without recursion here
        for (path++; *path != '\0'; path++) {
            if (fvx_match_name(path, pattern + 1) == FR_OK) return FR_OK;
        }
    }
    
    return FR_NO_FILE;
}

FRESULT fvx_preaddir (DIR* dp, FILINFO* fno, const TCHAR* pattern) {
    FRESULT res;
    while ((res = fvx_readdir(dp, fno)) == FR_OK)
        if (!pattern || !*(fno->fname) || (fvx_match_name(fno->fname, pattern) == FR_OK)) break;
    return res;
}

FRESULT fvx_findpath (TCHAR* path, const TCHAR* pattern, BYTE mode) {
    strncpy(path, pattern, _MAX_FN_LEN);
    TCHAR* fname = strrchr(path, '/');
    if (!fname) return FR_DENIED;
    *fname = '\0';
    
    TCHAR* npattern = strrchr(pattern, '/');
    if (!npattern) return FR_DENIED;
    npattern++;
    
    DIR pdir;
    FILINFO fno;
    FRESULT res;
    if ((res = fvx_opendir(&pdir, path)) != FR_OK) return res;
    
    *(fname++) = '/';
    *fname = '\0';
    
    while ((fvx_preaddir(&pdir, &fno, npattern) == FR_OK) && *(fno.fname)) {
        int cmp = strncmp(fno.fname, fname, _MAX_FN_LEN);
        if (((mode & FN_HIGHEST) && (cmp > 0)) || ((mode & FN_LOWEST) && (cmp < 0)) || !(*fname))
            strncpy(fname, fno.fname, _MAX_FN_LEN - (fname - path));
        if (!(mode & (FN_HIGHEST|FN_LOWEST))) break;
    }
    fvx_closedir( &pdir );
    
    return (*fname) ? FR_OK : FR_NO_PATH;
}

FRESULT fvx_findnopath (TCHAR* path, const TCHAR* pattern) {
    strncpy(path, pattern, _MAX_FN_LEN);
    TCHAR* fname = strrchr(path, '/');
    if (!fname) return FR_DENIED;
    fname++;
    
    TCHAR* rep[16];
    u32 n_rep = 0;
    for (u32 i = 0; fname[i]; i++) {
        if (fname[i] == '?') {
            rep[n_rep] = &(fname[i]);
            *rep[n_rep++] = '0';
        }
        if (n_rep >= 16) return FR_DENIED;
    }
    if (!n_rep) return (fvx_stat(path, NULL) == FR_OK) ? FR_NO_PATH : FR_OK;
    
    while (fvx_stat(path, NULL) == FR_OK) {
        for (INT i = n_rep - 1; (i >= 0); i--) {
            if (*(rep[i]) == '9') {
                if (!i) return FR_NO_PATH;
                *(rep[i]) = '0';
            } else {
                (*(rep[i]))++;
                break;
            }
        }
    }
    
    return FR_OK;
}
