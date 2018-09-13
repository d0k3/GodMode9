#include "fsdir.h"

void DirEntryCpy(DirEntry* dest, const DirEntry* orig) {
    memcpy(dest, orig, sizeof(DirEntry));
    dest->name = dest->path + (orig->name - orig->path);
}

int compDirEntry(const void* e1, const void* e2) {
    const DirEntry* entry1 = *((const DirEntry**) e1);
    const DirEntry* entry2 = *((const DirEntry**) e2);
    if (entry1->type == T_DOTDOT) return -1;
    if (entry2->type == T_DOTDOT) return 1;
    if (entry1->type != entry2->type)
        return entry1->type - entry2->type;
    return strncasecmp(entry1->name, entry2->name, 256);
}

void SortDirStruct(DirStruct* contents) {
    DirEntry* entry_ptr[MAX_DIR_ENTRIES];
    for (int i = 0; i < (int)contents->n_entries; i++)
        entry_ptr[i] = &(contents->entry[i]);
    qsort(entry_ptr, contents->n_entries, sizeof(DirEntry*), compDirEntry);
    DirEntry tmp[MAX_DIR_ENTRIES];
    for (int i = 0; i < (int)contents->n_entries; i++)
        DirEntryCpy(&tmp[i], entry_ptr[i]);
    for (int i = 0; i < (int)contents->n_entries; i++)
        DirEntryCpy(&(contents->entry[i]), &tmp[i]);
}
