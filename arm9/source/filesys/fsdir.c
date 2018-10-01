#include "fsdir.h"

void DirEntryCpy(DirEntry* dest, const DirEntry* orig) {
    memcpy(dest, orig, sizeof(DirEntry));
    dest->name = dest->path + dest->p_name;
}

int compDirEntry(const void* e1, const void* e2) {
    const DirEntry* entry1 = (const DirEntry*) e1;
    const DirEntry* entry2 = (const DirEntry*) e2;
    if (entry1->type == T_DOTDOT) return -1;
    if (entry2->type == T_DOTDOT) return 1;
    if (entry1->type != entry2->type)
        return entry1->type - entry2->type;
    return strncasecmp(entry1->path, entry2->path, 256);
}

void SortDirStruct(DirStruct* contents) {
    qsort(contents->entry, contents->n_entries, sizeof(DirEntry), compDirEntry);
    // fix entry->names after qsort
    for (int i = 0; i < (int)contents->n_entries; i++) {
        DirEntry* entry = &(contents->entry[i]);
        entry->name = entry->path + entry->p_name;
    }
}
