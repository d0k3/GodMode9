#include "store.h"
#include "ff.h"

#define STORE_BUFFER ((DirStruct*)0x21300000)

static bool is_stored = false; 

bool IsStoredDrive(const char* path) {
    return is_stored && (strncmp(path, "Z:", 3) == 0);
}

void StoreDirContents(DirStruct* contents) {
    memcpy(STORE_BUFFER, contents, sizeof(DirStruct));
    is_stored = true;
}

void GetStoredDirContents(DirStruct* contents) {
    // warning: this assumes the store buffer is filled with data that makes sense
    DirStruct* stored = STORE_BUFFER;
    u32 skip = 0;
    
    // basic sanity checking
    if (!is_stored || (stored->n_entries > MAX_ENTRIES)) return;
    
    // copy available entries, remove missing from storage
    for (u32 i = 0; i < stored->n_entries; i++) {
        DirEntry* entry = &(stored->entry[i]);
        if (strncmp(entry->name, "..", 3) == 0) continue; // disregard dotdot entry
        if (f_stat(entry->path, NULL) != FR_OK) {
            skip++; // remember this has to be removed from the stored struct
        } else { // entry is valid
            if (skip) { // move remaining entries to the left
                stored->n_entries -= skip;
                memmove(entry - skip, entry, (stored->n_entries - i) * sizeof(DirEntry));
                entry -= skip;
                skip = 0;
            }
            if (contents->n_entries < MAX_ENTRIES)
                memcpy(&(contents->entry[contents->n_entries++]), entry, sizeof(DirEntry));
            else break;
        }
    }
    stored->n_entries -= skip;
}
