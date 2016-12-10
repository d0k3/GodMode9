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

// inspired by http://www.geeksforgeeks.org/wildcard-character-matching/
bool MatchName(const char *pattern, const char *path) {
    // handling non asterisk chars
    for (; *pattern != '*'; pattern++, path++) {
        if ((*pattern == '\0') && (*path == '\0')) {
            return true; // end reached simultaneously, match found
        } else if ((*pattern == '\0') || (*path == '\0')) {
            return false; // end reached on only one, failure
        } else if ((*pattern != '?') && (tolower(*pattern) != tolower(*path))) {
            return false; // chars don't match, failure
        }
    }
    // handling the asterisk (matches one or more chars in path)
    if ((*(pattern+1) == '?') || (*(pattern+1) == '*')) {
        return false; // stupid user shenanigans, failure
    } else if (*path == '\0') {
        return false; // asterisk, but end reached on path, failure
    } else if (*(pattern+1) == '\0') {
        return true; // nothing after the asterisk, match found
    } else { // we couldn't really go without recursion here
        for (path++; *path != '\0'; path++) {
            if (MatchName(pattern + 1, path)) return true;
        }
    }
    
    return false;
}
