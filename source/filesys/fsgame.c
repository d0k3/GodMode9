#include "fsgame.h"
#include "fsperm.h"
#include "gameutil.h"
#include "ui.h"
#include "ff.h"

void SetDirGoodNames(DirStruct* contents) {
    char goodname[256];
    ShowProgress(0, 0, "");
    for (u32 s = 0; s < contents->n_entries; s++) {
        DirEntry* entry = &(contents->entry[s]);
        u32 plen = strnlen(entry->path, 256);
        if (!ShowProgress(s+1, contents->n_entries, entry->path)) break;
        if ((GetGoodName(goodname, entry->path, false) != 0) ||
            (plen + 1 + strnlen(goodname, 256) + 1 > 256))
            continue;
        strncpy(entry->path + plen + 1, goodname, 256 - 1 - plen - 1);
        entry->name = entry->path + plen + 1;
    }
}

bool GoodRenamer(DirEntry* entry, bool ask) {
    char goodname[256]; // get goodname
    if ((GetGoodName(goodname, entry->path, false) != 0) ||
        (strncmp(goodname + strnlen(goodname, 256) - 4, ".tmd", 4) == 0)) // no TMD, please
        return false;
    
    if (ask) { // ask for confirmatiom
        char oldname_tr[32+1];
        char newname_ww[256];
        TruncateString(oldname_tr, entry->name, 32, 8);
        strncpy(newname_ww, goodname, 256);
        WordWrapString(newname_ww, 32);
        if (!ShowPrompt(true, "%s\nRename to good name?\n \n%s", oldname_tr, newname_ww))
            return true; // call it a success because user choice
    }
    
    char npath[256]; // get new path
    strncpy(npath, entry->path, 256 - 1);
    char* nname = strrchr(npath, '/');
    if (!nname) return false;
    nname++;
    strncpy(nname, goodname, 256 - 1 - (nname - npath));
    // actual rename
    if (!CheckDirWritePermissions(entry->path)) return false;
    if (f_rename(entry->path, npath) != FR_OK) return false;
    strncpy(entry->path, npath, 256);
    entry->name = entry->path + (nname - npath);
    
    return true;
}
