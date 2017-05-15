#include "fsgame.h"
#include "gameutil.h"

void SetDirGoodNames(DirStruct* contents) {
    char goodname[256];
    for (u32 s = 0; s < contents->n_entries; s++) {
        DirEntry* entry = &(contents->entry[s]);
        u32 plen = strnlen(entry->path, 256);
        if ((GetGoodName(goodname, entry->path, false) != 0) ||
            (plen + 1 + strnlen(goodname, 256) + 1 > 256))
            continue;
        strncpy(entry->path + plen + 1, goodname, 256 - 1 - plen - 1);
        entry->name = entry->path + plen + 1;
    }
}
