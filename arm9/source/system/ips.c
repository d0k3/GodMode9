// C port of Alcaro's libips.cpp, which was released under GPLv3
// https://github.com/Alcaro/Flips/blob/master/libips.cpp
// Ported by Hyarion for use with VirtualFatFS

#include "ips.h"
#include "common.h"
#include "fsperm.h"
#include "ui.h"
#include "vff.h"

typedef enum {
	IPS_OK,
	IPS_NOTTHIS,
	IPS_THISOUT,
	IPS_SCRAMBLED,
	IPS_INVALID,
	IPS_16MB,
    IPS_INVALID_FILE_PATH,
    IPS_CANCELED
} IPSERROR;

FIL patchFile, inFile, outFile;
size_t patchSize;
char errName[256];

uint8_t buf[3];
unsigned int br;

int ret(int errcode) {
    fvx_close(&patchFile);
    fvx_close(&inFile);
    fvx_close(&outFile);
    switch(errcode) {
        case IPS_NOTTHIS:
            ShowPrompt(false, "%s\nThe patch is most likely not intended for this file.", errName); break;
        case IPS_THISOUT:
            ShowPrompt(false, "%s\nYou most likely applied the patch on the output file.", errName); break;
        case IPS_SCRAMBLED:
            ShowPrompt(false, "%s\nThe patch is technically valid,\nbut seems scrambled or malformed.", errName); break;
        case IPS_INVALID:
            ShowPrompt(false, "%s\nThe patch is invalid.", errName); break;
        case IPS_16MB:
            ShowPrompt(false, "%s\nOne or both files is bigger than 16MB.\nThe IPS format doesn't support that.", errName); break;
        case IPS_INVALID_FILE_PATH:
            ShowPrompt(false, "%s\nThe requested file path was invalid.", errName); break;
        case IPS_CANCELED:
            ShowPrompt(false, "%s\nPatching canceled.", errName); break;
    }
    return errcode;
}

uint8_t read8() {
    if (fvx_tell(&patchFile) >= patchSize) return 0;
    fvx_read(&patchFile, &buf, 1, &br);
    return buf[0];
}

unsigned int read16() {
    if (fvx_tell(&patchFile)+1 >= patchSize) return 0;
    fvx_read(&patchFile, &buf, 2, &br);
    return (buf[0] << 8) | buf[1];
}

unsigned int read24() {
    if (fvx_tell(&patchFile)+2 >= patchSize) return 0;
    fvx_read(&patchFile, &buf, 3, &br);
    return (buf[0] << 16) | (buf[1] << 8) | buf[2];
}

int ApplyIPSPatch(const char* patchName, const char* inName, const char* outName) {
    int error = IPS_INVALID;
    unsigned int outlen_min, outlen_max, outlen_min_mem;
    snprintf(errName, 256, "%s", patchName);
    
    if (fvx_open(&patchFile, patchName, FA_READ) != FR_OK) return ret(IPS_INVALID_FILE_PATH);
    patchSize = fvx_size(&patchFile);
    ShowProgress(0, patchSize, patchName);
    
    // Check validity of patch
    if (patchSize < 8) return ret(IPS_INVALID);
    if (read8() != 'P' ||
        read8() != 'A' ||
        read8() != 'T' ||
        read8() != 'C' ||
        read8() != 'H')
    {
        return ret(IPS_INVALID);
    }
    
    unsigned int offset = read24();
    unsigned int outlen = 0;
    unsigned int thisout = 0;
    unsigned int lastoffset = 0;
    bool w_scrambled = false;
    while (offset != 0x454F46) // 454F46=EOF
    {
        if (!ShowProgress(fvx_tell(&patchFile), patchSize, patchName) &&
            ShowPrompt(true, "%s\nB button detected. Cancel?", patchName))
            return ret(IPS_CANCELED);
        unsigned int size = read16();
        if (size == 0)
        {
            size = read16();
            if (!size) return ret(IPS_INVALID);
            thisout = offset + size;
            read8();
        }
        else
        {
            thisout = offset + size;
            fvx_lseek(&patchFile, fvx_tell(&patchFile) + size);
        }
        if (offset < lastoffset) w_scrambled = true;
        lastoffset = offset;
        if (thisout > outlen) outlen = thisout;
        if (fvx_tell(&patchFile) >= patchSize) return ret(IPS_INVALID);
        offset = read24();
    }
    outlen_min_mem = outlen;
    outlen_max = 0xFFFFFFFF;
    if (fvx_tell(&patchFile)+3 == patchSize)
    {
        unsigned int truncate = read24();
        outlen_max = truncate;
        if (outlen > truncate)
        {
            outlen = truncate;
            w_scrambled = true;
        }
    }
    if (fvx_tell(&patchFile) != patchSize) return ret(IPS_INVALID);
    outlen_min = outlen;
    error = IPS_OK;
    if (w_scrambled) error = IPS_SCRAMBLED;
    
    // start applying patch
    bool inPlace = false;
    if (!CheckWritePermissions(outName)) return ret(IPS_INVALID_FILE_PATH);
    if (strncasecmp(inName, outName, 256) == 0)
    {
        if (fvx_open(&outFile, outName, FA_WRITE | FA_READ) != FR_OK) return ret(IPS_INVALID_FILE_PATH);
        inFile = outFile;
        inPlace = true;
    }
    else if ((fvx_open(&inFile, inName, FA_READ) != FR_OK) ||
            (fvx_open(&outFile, outName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK))
            return ret(IPS_INVALID_FILE_PATH);
    
    size_t inSize = fvx_size(&inFile);
    outlen = max(outlen_min, min(inSize, outlen_max));
    fvx_lseek(&outFile, max(outlen, outlen_min_mem));
    fvx_lseek(&outFile, 0);
    size_t outSize = outlen;
    ShowProgress(0, outSize, outName);
    
    if (!inPlace) {
        fvx_lseek(&inFile, 0);
        uint8_t buffer;
        for(size_t n = 0; n < min(inSize, outlen); n++) {
            fvx_read(&inFile, &buffer, 1, &br);
            fvx_write(&outFile, &buffer, 1, &br);
        }
    }
    if (outSize > inSize) {
        fvx_lseek(&outFile, inSize);
        for(size_t n = inSize; n < outSize; n++) fvx_write(&outFile, 0, 1, &br);
    }
    
    fvx_lseek(&patchFile, 5);
    offset = read24();
    while (offset != 0x454F46)
    {
        if (!ShowProgress(offset, outSize, outName) &&
            ShowPrompt(true, "%s\nB button detected. Cancel?", patchName))
            return ret(IPS_CANCELED);
        
        fvx_lseek(&outFile, offset);
        unsigned int size = read16();
        if (size == 0)
        {
            size = read16();
            uint8_t b = read8();
            for(size_t n = 0; n < size; n++) fvx_write(&outFile, &b, 1, &br);
        }
        else
        {
            uint8_t buffer;
            for(size_t n = 0; n < size; n++) {
                fvx_read(&patchFile, &buffer, 1, &br);
                fvx_write(&outFile, &buffer, 1, &br);
            }
        }
        offset = read24();
    }
    
    fvx_lseek(&outFile, outSize);
    f_truncate(&outFile);
    return ret(error);
}
