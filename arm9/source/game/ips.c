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
    IPS_CANCELED,
    IPS_MEMORY
} IPSERROR;

static FIL patchFile, inFile, outFile;
static size_t patchSize;
static u8 *patch;
static u32 patchOffset;

char errName[256];

int displayError(int errcode) {
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
        case IPS_MEMORY:
            ShowPrompt(false, "%s\nNot enough memory.", errName); break;
    }
    fvx_close(&patchFile);
    fvx_close(&inFile);
    fvx_close(&outFile);
    return errcode;
}

typedef enum {
    COPY_IN,
    COPY_PATCH,
    COPY_RLE
} COPYMODE;

bool IPScopy(u8 mode, u32 size, u8 rle) {
    bool ret = true;
    if (mode == COPY_PATCH) {
        UINT bytes_written = size;
        if ((fvx_write(&outFile, &patch[patchOffset], size, &bytes_written) != FR_OK) ||
            (size != bytes_written))
            ret = false;
        patchOffset += size;
    } else {
        u32 bufsiz = min(STD_BUFFER_SIZE, size);
        u8* buffer = malloc(bufsiz);
        if (!buffer) return false;
        if (mode == COPY_RLE) memset(buffer, rle, bufsiz);

        for (u64 pos = 0; (pos < size) && ret; pos += bufsiz) {
            UINT read_bytes = min(bufsiz, size - pos);
            UINT bytes_written = read_bytes;
            if (((mode == COPY_IN) && (fvx_read(&inFile, buffer, read_bytes, &bytes_written) != FR_OK)) ||
                ((mode == COPY_PATCH) && (fvx_read(&patchFile, buffer, read_bytes, &bytes_written) != FR_OK)) ||
                (read_bytes != bytes_written))
                ret = false;
            if ((ret && (fvx_write(&outFile, buffer, read_bytes, &bytes_written) != FR_OK)) ||
                (read_bytes != bytes_written))
                ret = false;
        }

        free(buffer);
    }
    return ret;
}

u8 read8() {
    if (patchOffset >= patchSize) return 0;
    return patch[patchOffset++];
}

UINT read16() {
    if (patchOffset+1 >= patchSize) return 0;
    UINT buf = patch[patchOffset++] << 8;
    buf |= patch[patchOffset++];
    return buf;
}

UINT read24() {
    if (patchOffset+2 >= patchSize) return 0;
    UINT buf = patch[patchOffset++] << 16;
    buf |= patch[patchOffset++] << 8;
    buf |= patch[patchOffset++];
    return buf;
}

int ApplyIPSPatch(const char* patchName, const char* inName, const char* outName) {
    int error = IPS_INVALID;
    UINT outlen_min, outlen_max, outlen_min_mem;
    snprintf(errName, 256, "%s", patchName);
    
    if (fvx_open(&patchFile, patchName, FA_READ) != FR_OK) return displayError(IPS_INVALID_FILE_PATH);
    patchSize = fvx_size(&patchFile);
    ShowProgress(0, patchSize, patchName);
    
    patch = malloc(patchSize);
    if (!patch || fvx_read(&patchFile, patch, patchSize, NULL) != FR_OK) return displayError(IPS_MEMORY);
    
    // Check validity of patch
    if (patchSize < 8) return displayError(IPS_INVALID);
    if (read8() != 'P' ||
        read8() != 'A' ||
        read8() != 'T' ||
        read8() != 'C' ||
        read8() != 'H')
    {
        return displayError(IPS_INVALID);
    }
    
    unsigned int offset = read24();
    unsigned int outlen = 0;
    unsigned int thisout = 0;
    unsigned int lastoffset = 0;
    bool w_scrambled = false;
    while (offset != 0x454F46) // 454F46=EOF
    {
        if (!ShowProgress(patchOffset, patchSize, patchName)) {
            if (ShowPrompt(true, "%s\nB button detected. Cancel?", patchName)) return displayError(IPS_CANCELED);
            ShowProgress(0, patchSize, patchName);
            ShowProgress(patchOffset, patchSize, patchName);
        }
        
        unsigned int size = read16();
        if (size == 0)
        {
            size = read16();
            if (!size) return displayError(IPS_INVALID);
            thisout = offset + size;
            read8();
        }
        else
        {
            thisout = offset + size;
            patchOffset += size;
        }
        if (offset < lastoffset) w_scrambled = true;
        lastoffset = offset;
        if (thisout > outlen) outlen = thisout;
        if (patchOffset >= patchSize) return displayError(IPS_INVALID);
        offset = read24();
    }
    outlen_min_mem = outlen;
    outlen_max = 0xFFFFFFFF;
    if (patchOffset+3 == patchSize)
    {
        unsigned int truncate = read24();
        outlen_max = truncate;
        if (outlen > truncate)
        {
            outlen = truncate;
            w_scrambled = true;
        }
    }
    if (patchOffset != patchSize) return displayError(IPS_INVALID);
    outlen_min = outlen;
    error = IPS_OK;
    if (w_scrambled) error = IPS_SCRAMBLED;
    
    // start applying patch
    bool inPlace = false;
    if (!CheckWritePermissions(outName)) return displayError(IPS_INVALID_FILE_PATH);
    if (strncasecmp(inName, outName, 256) == 0)
    {
        if (fvx_open(&outFile, outName, FA_WRITE | FA_READ) != FR_OK) return displayError(IPS_INVALID_FILE_PATH);
        inFile = outFile;
        inPlace = true;
    }
    else if ((fvx_open(&inFile, inName, FA_READ) != FR_OK) ||
            (fvx_open(&outFile, outName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK))
            return displayError(IPS_INVALID_FILE_PATH);
    
    size_t inSize = fvx_size(&inFile);
    outlen = max(outlen_min, min(inSize, outlen_max));
    fvx_lseek(&outFile, max(outlen, outlen_min_mem));
    fvx_lseek(&outFile, 0);
    size_t outSize = outlen;
    ShowProgress(0, outSize, outName);
    
    fvx_lseek(&inFile, 0);
    if (!inPlace && !IPScopy(COPY_IN, min(inSize, outlen), 0)) return displayError(IPS_MEMORY);
    fvx_lseek(&outFile, inSize);
    if (outSize > inSize && !IPScopy(COPY_RLE, outSize - inSize, 0)) return displayError(IPS_MEMORY);
    
    fvx_lseek(&patchFile, 5);
    offset = read24();
    while (offset != 0x454F46)
    {
        if (!ShowProgress(offset, outSize, outName)) {
            if (ShowPrompt(true, "%s\nB button detected. Cancel?", outName)) return displayError(IPS_CANCELED);
            ShowProgress(0, outSize, outName);
            ShowProgress(offset, outSize, outName);
        }
        
        fvx_lseek(&outFile, offset);
        unsigned int size = read16();
        if (size == 0 && !IPScopy(COPY_RLE, read16(), read8())) return displayError(IPS_MEMORY); 
        else if (size != 0 && !IPScopy(COPY_PATCH, size, 0)) return displayError(IPS_MEMORY);
        offset = read24();
    }
    
    fvx_lseek(&outFile, outSize);
    f_truncate(&outFile);
    return displayError(error);
}
