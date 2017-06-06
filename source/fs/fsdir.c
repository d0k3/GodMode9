#include "fsdir.h"

void DirEntryCpy(DirEntry* dest, const DirEntry* orig) {
    memcpy(dest, orig, sizeof(DirEntry));
    dest->name = dest->path + (orig->name - orig->path);
}

void SortDirStruct(DirStruct* contents) {
    for (u32 s = 0; s < contents->n_entries; s++) {
        DirEntry* cmp0 = &(contents->entry[s]);
        DirEntry* min0 = cmp0;
        if (cmp0->type == T_DOTDOT) continue;
        for (u32 c = s + 1; c < contents->n_entries; c++) {
            DirEntry* cmp1 = &(contents->entry[c]);
            if (min0->type != cmp1->type) {
                if (min0->type > cmp1->type)
                    min0 = cmp1;
                continue;
            }
            if (strncasecmp(min0->name, cmp1->name, 256) > 0)
                min0 = cmp1;
        }
        if (min0 != cmp0) {
            DirEntry swap; // swap entries and fix names
            DirEntryCpy(&swap, cmp0);
            DirEntryCpy(cmp0, min0);
            DirEntryCpy(min0, &swap);
        }
    }
}
