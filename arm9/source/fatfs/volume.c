#include "volume.h"
#include <stddef.h>

static const char* const drive_paths[FAT_VOLUME_COUNT] = {
    "0:", "1:", "2:", "3:", "4:", "5:", "6:", "7:", "8:", "9:", FLASHCART_DRIVE
};

const char* GetFatDrivePath(unsigned int volume) {
    return (volume < FAT_VOLUME_COUNT) ? drive_paths[volume] : NULL;
}

int GetFatDriveIndex(const char* path) {
    if (!path || !path[0] || (path[1] != ':')) return -1;
    char name = path[0];
    if ((name >= 'a') && (name <= 'z')) name -= 'a' - 'A';
    for (unsigned int i = 0; i < FAT_VOLUME_COUNT; i++)
        if (name == drive_paths[i][0]) return i;
    return -1;
}
