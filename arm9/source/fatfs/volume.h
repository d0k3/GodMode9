#pragma once

#define FAT_VOLUME_COUNT     11
#define FAT_IMAGE_FIRST      7
#define FAT_IMAGE_COUNT      3
#define FAT_IMAGE_END        (FAT_IMAGE_FIRST + FAT_IMAGE_COUNT)
#define FAT_RAM_VOLUME       9
#define FAT_FLASHCART_VOLUME 10

#define FLASHCART_DRIVE "H:"
#define FAT_VOLUME_STRS "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "H"

const char* GetFatDrivePath(unsigned int volume);
int GetFatDriveIndex(const char* path);
