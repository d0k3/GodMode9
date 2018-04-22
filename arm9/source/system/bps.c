// C port of byuu's \nall\beat\patch.hpp and \multi.hpp, which were released under GPLv3
// https://github.com/eai04191/beat/blob/master/nall/beat/patch.hpp
// https://github.com/eai04191/beat/blob/master/nall/beat/multi.hpp
// Ported by Hyarion for use with VirtualFatFS

#include "bps.h"
#include "common.h"
#include "crc32.h"
#include "fsperm.h"
#include "ui.h"
#include "vff.h"

#define BPM_FILEMODE_BUFFER 0x200000

typedef enum {
    BEAT_SUCCESS = 0,
    BEAT_PATCH_TOO_SMALL,
    BEAT_PATCH_INVALID_HEADER,
    BEAT_SOURCE_TOO_SMALL,
    BEAT_TARGET_TOO_SMALL,
    BEAT_SOURCE_CHECKSUM_INVALID,
    BEAT_TARGET_CHECKSUM_INVALID,
    BEAT_PATCH_CHECKSUM_INVALID,
    BEAT_BPM_CHECKSUM_INVALID,
    BEAT_INVALID_FILE_PATH,
    BEAT_CANCELED
} BPSRESULT;

typedef enum {
    BEAT_SOURCEREAD = 0,
    BEAT_TARGETREAD,
    BEAT_SOURCECOPY,
    BEAT_TARGETCOPY
} BPSMODE;

typedef enum {
    BEAT_CREATEPATH = 0,
    BEAT_CREATEFILE,
    BEAT_MODIFYFILE,
    BEAT_MIRRORFILE
} BPMACTION;

size_t patchSize;
FIL patchFile;
FIL sourceFile;
FIL targetFile;

uint8_t* sourceData;
uint8_t* targetData;
bool sourceInMemory;
bool targetInMemory;

unsigned int modifyOffset;
unsigned int outputOffset;
uint32_t modifyChecksum;
uint32_t targetChecksum;

bool bpmFileMode;
size_t bpmFileChunks;
size_t bpmLastChunk;
size_t bpmCurrChunk;
bool bpmIsActive;
uint32_t bpmChecksum;

uint8_t buffer;
unsigned int br;

char progressText[256];
unsigned int outputCurrent;
unsigned int outputTotal;

int err(int errcode) {
    if(sourceInMemory) free(sourceData);
    if(targetInMemory) free(targetData);
    fvx_close(&sourceFile);
    fvx_close(&targetFile);
    fvx_close(&patchFile);
    switch(errcode) {
        case BEAT_PATCH_TOO_SMALL:
            ShowPrompt(false, "%s\nThe patch is too small to be a valid BPS file.", progressText); break;
        case BEAT_PATCH_INVALID_HEADER:
            ShowPrompt(false, "%s\nThe patch is not a valid BPS file.", progressText); break;
        case BEAT_SOURCE_TOO_SMALL:
            ShowPrompt(false, "%s\nThe file being patched is smaller than expected.", progressText); break;
        case BEAT_TARGET_TOO_SMALL:
            ShowPrompt(false, "%s\nThere is not enough space for the patched file.", progressText); break;
        case BEAT_SOURCE_CHECKSUM_INVALID:
            ShowPrompt(false, "%s\nThe file being patched does not match its checksum.", progressText); break;
        case BEAT_TARGET_CHECKSUM_INVALID:
            ShowPrompt(false, "%s\nThe patched file does not match its checksum.", progressText); break;
        case BEAT_PATCH_CHECKSUM_INVALID:
            ShowPrompt(false, "%s\nThe BPS patch file does not match its checksum.", progressText); break;
        case BEAT_BPM_CHECKSUM_INVALID:
            ShowPrompt(false, "%s\nThe BPM patch file does not match its checksum.", progressText); break;
        case BEAT_INVALID_FILE_PATH:
            ShowPrompt(false, "%s\nThe requested file path was invalid.", progressText); break;
        case BEAT_CANCELED:
            ShowPrompt(false, "%s\nPatching canceled.", progressText); break;
    }
    return errcode;
}

uint8_t BPSread() {
    fvx_read(&patchFile, &buffer, 1, &br);
    modifyChecksum = crc32_adjust(modifyChecksum, buffer);
    if (bpmIsActive) bpmChecksum = crc32_adjust(bpmChecksum, buffer);
    modifyOffset++;
    return buffer;
}

uint64_t BPSdecode() {
    uint64_t data = 0, shift = 1;
    while(true) {
        uint8_t x = BPSread();
        data += (x & 0x7f) * shift;
        if(x & 0x80) break;
        shift <<= 7;
        data += shift;
    }
    return data;
}

void BPSwrite(uint8_t data) {
    if(targetInMemory) targetData[outputOffset] = data;
    else fvx_write(&targetFile, &data, 1, &br);
    targetChecksum = crc32_adjust(targetChecksum, data);
    outputOffset++;
}

int ApplyBeatPatch() {
    uint8_t counter = 0;
    unsigned int sourceRelativeOffset = 0, targetRelativeOffset = 0;
    sourceInMemory = false, targetInMemory = false;
    modifyOffset = 0, outputOffset = 0;
    modifyChecksum = ~0, targetChecksum = ~0;
    if(patchSize < 19) return err(BEAT_PATCH_TOO_SMALL);
    
    if(BPSread() != 'B') return err(BEAT_PATCH_INVALID_HEADER);
    if(BPSread() != 'P') return err(BEAT_PATCH_INVALID_HEADER);
    if(BPSread() != 'S') return err(BEAT_PATCH_INVALID_HEADER);
    if(BPSread() != '1') return err(BEAT_PATCH_INVALID_HEADER);
    
    size_t modifySourceSize = BPSdecode();
    size_t modifyTargetSize = BPSdecode();
    size_t modifyMarkupSize = BPSdecode();
    for(unsigned int n = 0; n < modifyMarkupSize; n++) BPSread(); // metadata, not useful to us

    size_t sourceSize;
    size_t targetSize;
    if(!bpmFileMode) {
        sourceSize = fvx_size(&sourceFile);
        fvx_lseek(&sourceFile, 0);
        fvx_lseek(&targetFile, modifyTargetSize);
        fvx_lseek(&targetFile, 0);
        targetSize = fvx_size(&targetFile);
    } else {
        if (bpmCurrChunk == bpmFileChunks - 1) sourceSize = bpmLastChunk;
        else sourceSize = BPM_FILEMODE_BUFFER;
        fvx_lseek(&sourceFile, bpmCurrChunk * BPM_FILEMODE_BUFFER);
        fvx_lseek(&targetFile, (bpmCurrChunk * BPM_FILEMODE_BUFFER) + modifyTargetSize);
        fvx_lseek(&targetFile, bpmCurrChunk * BPM_FILEMODE_BUFFER);
        targetSize = fvx_size(&targetFile) - (bpmCurrChunk * BPM_FILEMODE_BUFFER);
    }
    
    if (!bpmIsActive) outputTotal = targetSize;
    if(modifySourceSize > sourceSize) return err(BEAT_SOURCE_TOO_SMALL);
    if(modifyTargetSize > targetSize) return err(BEAT_TARGET_TOO_SMALL);
    
    sourceData = (uint8_t*)malloc(sourceSize);
    targetData = (uint8_t*)malloc(targetSize);
    if (sourceData != NULL) {
        sourceInMemory = true;
        fvx_read(&sourceFile, sourceData, sourceSize, &br);
    } else if (bpmFileMode) sourceRelativeOffset = (bpmCurrChunk * BPM_FILEMODE_BUFFER);
    if (targetData != NULL) targetInMemory = true;
    else if (bpmFileMode) targetRelativeOffset = (bpmCurrChunk * BPM_FILEMODE_BUFFER);

    while(modifyOffset < patchSize - 12) {
        if ((counter++ == 0) &&
            !ShowProgress(outputCurrent, outputTotal, progressText) &&
            ShowPrompt(true, "%s\nB button detected. Cancel?", progressText))
            return err(BEAT_CANCELED);
            
        unsigned int length = BPSdecode();
        unsigned int mode = length & 3;
        length = (length >> 2) + 1;
        outputCurrent += length;

        switch(mode) {
            case BEAT_SOURCEREAD:
                if (sourceInMemory) {
                    while(length--) BPSwrite(sourceData[outputOffset]);
                } else {
                    fvx_lseek(&sourceFile, fvx_tell(&targetFile));
                    while(length--) {
                        fvx_read(&sourceFile, &buffer, 1, &br);
                        BPSwrite(buffer);
                    }
                }
                break;
            case BEAT_TARGETREAD:
                while(length--) BPSwrite(BPSread());
                break;
            case BEAT_SOURCECOPY:
            case BEAT_TARGETCOPY:
                ; // intentional null statement
                int offset = BPSdecode();
                bool negative = offset & 1;
                offset >>= 1;
                if(negative) offset = -offset;

                if(mode == BEAT_SOURCECOPY) {
                    sourceRelativeOffset += offset;
                    if(sourceInMemory) {
                        while(length--) BPSwrite(sourceData[sourceRelativeOffset++]);
                    } else {
                        fvx_lseek(&sourceFile, sourceRelativeOffset);
                        while(length--) {
                            fvx_read(&sourceFile, &buffer, 1, &br);
                            BPSwrite(buffer);
                            sourceRelativeOffset++;
                        }
                        fvx_lseek(&sourceFile, fvx_tell(&targetFile));
                    }
                } else {
                    targetRelativeOffset += offset;
                    if(targetInMemory) {
                        while(length--) BPSwrite(targetData[targetRelativeOffset++]);
                    } else {
                        unsigned int targetOffset = fvx_tell(&targetFile);
                        while(length--) {
                            fvx_lseek(&targetFile, targetRelativeOffset);
                            fvx_read(&targetFile, &buffer, 1, &br);
                            fvx_lseek(&targetFile, targetOffset);
                            BPSwrite(buffer);
                            targetRelativeOffset++;
                            targetOffset++;
                        }
                    }
                }
                break;
        }
    }

    uint32_t modifySourceChecksum = 0, modifyTargetChecksum = 0, modifyModifyChecksum = 0;
    for(unsigned int n = 0; n < 32; n += 8) modifySourceChecksum |= BPSread() << n;
    for(unsigned int n = 0; n < 32; n += 8) modifyTargetChecksum |= BPSread() << n;
    uint32_t checksum = ~modifyChecksum;
    for(unsigned int n = 0; n < 32; n += 8) modifyModifyChecksum |= BPSread() << n;

    uint32_t sourceChecksum;
    if(sourceInMemory) sourceChecksum = crc32_calculate(sourceData, modifySourceSize);
    else {
        if (!bpmFileMode) fvx_lseek(&sourceFile, 0);
        else fvx_lseek(&sourceFile, bpmCurrChunk * BPM_FILEMODE_BUFFER);
        sourceChecksum = crc32_calculate_from_file(sourceFile, modifySourceSize);
    }
    targetChecksum = ~targetChecksum;

    if (sourceInMemory) free(sourceData);
    if (targetInMemory) {
        fvx_write(&targetFile, targetData, targetSize, &br);
        free(targetData);
    }
    
    if (!bpmFileMode) fvx_close(&sourceFile);
    if (!bpmFileMode) fvx_close(&targetFile);
    if (!bpmIsActive) fvx_close(&patchFile);

    if(sourceChecksum != modifySourceChecksum) return err(BEAT_SOURCE_CHECKSUM_INVALID);
    if(targetChecksum != modifyTargetChecksum) return err(BEAT_TARGET_CHECKSUM_INVALID);
    if(checksum != modifyModifyChecksum) return err(BEAT_PATCH_CHECKSUM_INVALID);
    
    return BEAT_SUCCESS;
}

int ApplyBPSPatch(const char* modifyName, const char* sourceName, const char* targetName) {
    bpmFileMode = false;
    bpmIsActive = false;
    outputCurrent = 0;
    outputTotal = 0;
    
    if ((!CheckWritePermissions(targetName)) ||
        (fvx_open(&patchFile, modifyName, FA_READ) != FR_OK) ||
        (fvx_open(&sourceFile, sourceName, FA_READ) != FR_OK) ||
        (fvx_open(&targetFile, targetName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK))
        return err(BEAT_INVALID_FILE_PATH);
    
    patchSize = fvx_size(&patchFile);
    snprintf(progressText, 256, "%s", targetName);
    return ApplyBeatPatch();
}

uint8_t BPMread() {
    fvx_read(&patchFile, &buffer, 1, &br);
    if (bpmIsActive) bpmChecksum = crc32_adjust(bpmChecksum, buffer);
    return buffer;
}

uint64_t BPMreadNumber() {
    uint64_t data = 0, shift = 1;
    while(true) {
        uint8_t x = BPMread();
        data += (x & 0x7f) * shift;
        if(x & 0x80) break;
        shift <<= 7;
        data += shift;
    }
    return data;
}

char* BPMreadString(char text[], unsigned int length) {
    for(unsigned int n = 0; n < length; n++) text[n] = BPMread();
    text[length] = '\0';
    return text;
}

uint32_t BPMreadChecksum() {
    uint32_t checksum = 0;
    checksum |= BPMread() <<  0;
    checksum |= BPMread() <<  8;
    checksum |= BPMread() << 16;
    checksum |= BPMread() << 24;
    return checksum;
}

bool CalculateBPMLength(const char* patchName, const char* sourcePath, const char* targetPath) {
    uint8_t counter = 0;
    bpmIsActive = false;
    snprintf(progressText, 256, "%s", patchName);
    fvx_lseek(&patchFile, BPMreadNumber() + fvx_tell(&patchFile));
    while(fvx_tell(&patchFile) < fvx_size(&patchFile) - 4) {
        uint64_t encoding = BPMreadNumber();
        unsigned int action = encoding & 3;
        char targetName[256];
        BPMreadString(targetName, (encoding >> 2) + 1);
        if ((counter++ % 16 == 0) &&
            !ShowProgress(fvx_tell(&patchFile), fvx_size(&patchFile), progressText) &&
            ShowPrompt(true, "%s\nB button detected. Cancel?", progressText))
            return false;
        if(action == BEAT_CREATEFILE) {
            uint64_t fileSize = BPMreadNumber();
            fvx_lseek(&patchFile, fileSize + 4 + fvx_tell(&patchFile));
            outputTotal += fileSize;
            bpmCurrChunk++;
        } else if(action == BEAT_MODIFYFILE) {
            fvx_lseek(&patchFile, (BPMreadNumber() >> 1) + fvx_tell(&patchFile));
            uint64_t patchSize = BPMreadNumber() + fvx_tell(&patchFile);
            fvx_lseek(&patchFile, 4 + fvx_tell(&patchFile));
            BPMreadNumber();
            outputTotal += BPMreadNumber();
            fvx_lseek(&patchFile, patchSize);
            bpmCurrChunk++;
        } else if(action == BEAT_MIRRORFILE) {
            encoding = BPMreadNumber();
            char originPath[256], sourceName[256], oldPath[256];
            if (encoding & 1) snprintf(originPath, 256, "%s", targetPath);
            else snprintf(originPath, 256, "%s", sourcePath);
            if ((encoding >> 1) == 0) snprintf(sourceName, 256, "%s", targetName);
            else BPMreadString(sourceName, encoding >> 1);
            snprintf(oldPath, 256, "%s/%s", originPath, sourceName);
            if (!bpmFileMode) {
                FIL oldFile;
                fvx_open(&oldFile, oldPath, FA_READ);
                outputTotal += fvx_size(&oldFile);
                fvx_close(&oldFile);
                fvx_lseek(&patchFile, 4 + fvx_tell(&patchFile));
            } else if (bpmCurrChunk == bpmFileChunks - 1) outputTotal += bpmLastChunk;
            else outputTotal += BPM_FILEMODE_BUFFER;
            bpmCurrChunk++;
        }
    }

    fvx_lseek(&patchFile, 4);
    bpmCurrChunk = 0;
    bpmIsActive = true;
    return true;
}

int ApplyBPMPatch(const char* patchName, const char* sourcePath, const char* targetPath) {
    bpmFileMode = false;
    bpmIsActive = true;
    bpmChecksum = ~0;
    outputCurrent = 0;
    outputTotal = 0;
    
    DIR fdir;
    if (fvx_stat(sourcePath, NULL) != FR_OK) return err(BEAT_INVALID_FILE_PATH);
    else if (fvx_opendir(&fdir, sourcePath) == FR_OK) fvx_closedir(&fdir);
    else {
        bpmFileMode = true;
        if (fvx_open(&sourceFile, sourcePath, FA_READ) != FR_OK) return err(BEAT_INVALID_FILE_PATH);
        bpmFileChunks = fvx_size(&sourceFile) / BPM_FILEMODE_BUFFER;
        bpmLastChunk = fvx_size(&sourceFile) % BPM_FILEMODE_BUFFER;
        bpmCurrChunk = 0;
        if (bpmLastChunk != 0) bpmFileChunks += 1;
    }
    
    if ((!CheckWritePermissions(targetPath)) ||
        (!bpmFileMode && (fvx_stat(targetPath, NULL) != FR_OK) && (fvx_mkdir(targetPath) != FR_OK)) ||
        (bpmFileMode && (fvx_open(&targetFile, targetPath, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK)) ||
        (fvx_open(&patchFile, patchName, FA_READ) != FR_OK))
        return err(BEAT_INVALID_FILE_PATH);
        
    if(BPMread() != 'B') return err(BEAT_PATCH_INVALID_HEADER);
    if(BPMread() != 'P') return err(BEAT_PATCH_INVALID_HEADER);
    if(BPMread() != 'M') return err(BEAT_PATCH_INVALID_HEADER);
    if(BPMread() != '1') return err(BEAT_PATCH_INVALID_HEADER);
    if (!CalculateBPMLength(patchName, sourcePath, targetPath)) return err(BEAT_CANCELED);
    uint64_t metadataLength = BPMreadNumber();
    while(metadataLength--) BPMread();

    while(fvx_tell(&patchFile) < fvx_size(&patchFile) - 4) {
        uint64_t encoding = BPMreadNumber();
        unsigned int action = encoding & 3;
        unsigned int targetLength = (encoding >> 2) + 1;
        char targetName[256];
        BPMreadString(targetName, targetLength);
        snprintf(progressText, 256, "%s", targetName);
        if (!ShowProgress(outputCurrent, outputTotal, progressText) &&
            ShowPrompt(true, "%s\nB button detected. Cancel?", progressText))
            return err(BEAT_CANCELED);

        if(action == BEAT_CREATEPATH && !bpmFileMode) {
            char newPath[256];
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            if ((fvx_stat(newPath, NULL) != FR_OK) && (fvx_mkdir(newPath) != FR_OK)) return err(BEAT_INVALID_FILE_PATH);
        } else if(action == BEAT_CREATEFILE && !bpmFileMode) {
            char newPath[256];
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            FIL newFile;
            if (fvx_open(&newFile, newPath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return err(BEAT_INVALID_FILE_PATH);
            uint64_t fileSize = BPMreadNumber();
            outputCurrent += fileSize;
            fvx_lseek(&newFile, fileSize);
            fvx_lseek(&newFile, 0);
            while(fileSize--) {
                buffer = BPMread();
                fvx_write(&newFile, &buffer, 1, &br);
            }
            BPMreadChecksum();
            fvx_close(&newFile);
        } else if(action == BEAT_CREATEFILE && bpmFileMode) {
            uint64_t fileSize = BPMreadNumber();
            outputCurrent += fileSize;
            fvx_lseek(&targetFile, (bpmCurrChunk * BPM_FILEMODE_BUFFER) + fileSize);
            fvx_lseek(&targetFile, bpmCurrChunk * BPM_FILEMODE_BUFFER);
            while(fileSize--) {
                buffer = BPMread();
                fvx_write(&targetFile, &buffer, 1, &br);
            }
            BPMreadChecksum();
            bpmCurrChunk++;
        } else {
            encoding = BPMreadNumber();
            char originPath[256], sourceName[256], oldPath[256], newPath[256];
            if (encoding & 1) snprintf(originPath, 256, "%s", targetPath);
            else snprintf(originPath, 256, "%s", sourcePath);
            if ((encoding >> 1) == 0) snprintf(sourceName, 256, "%s", targetName);
            else BPMreadString(sourceName, encoding >> 1);
            snprintf(oldPath, 256, "%s/%s", originPath, sourceName);
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            if(action == BEAT_MODIFYFILE) {
                patchSize = BPMreadNumber();
                if (!bpmFileMode && ((fvx_open(&sourceFile, oldPath, FA_READ) != FR_OK) ||
                    (fvx_open(&targetFile, newPath, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK)))
                    return err(BEAT_INVALID_FILE_PATH);
                int result = ApplyBeatPatch();
                if (result != BEAT_SUCCESS) return result;
                bpmCurrChunk++;
            } else if(action == BEAT_MIRRORFILE && !bpmFileMode) {
                FIL oldFile, newFile;
                if ((fvx_open(&oldFile, oldPath, FA_READ) != FR_OK) ||
                    (fvx_open(&newFile, newPath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK))
                    return err(BEAT_INVALID_FILE_PATH);
                uint64_t fileSize = fvx_size(&oldFile);
                outputCurrent += fileSize;
                fvx_lseek(&newFile, fileSize);
                fvx_lseek(&newFile, 0);
                while(fileSize--) {
                    fvx_read(&oldFile, &buffer, 1, &br);
                    fvx_write(&newFile, &buffer, 1, &br);
                }
                BPMreadChecksum();
                fvx_close(&oldFile);
                fvx_close(&newFile);
            } else if(action == BEAT_MIRRORFILE && bpmFileMode) {
                uint64_t fileSize;
                if (bpmCurrChunk == bpmFileChunks - 1) fileSize = bpmLastChunk;
                else fileSize = BPM_FILEMODE_BUFFER;
                outputCurrent += fileSize;
                fvx_lseek(&targetFile, (bpmCurrChunk * BPM_FILEMODE_BUFFER) + fileSize);
                fvx_lseek(&targetFile, bpmCurrChunk * BPM_FILEMODE_BUFFER);
                while(fileSize--) {
                    fvx_read(&sourceFile, &buffer, 1, &br);
                    fvx_write(&targetFile, &buffer, 1, &br);
                }
                BPMreadChecksum();
                bpmCurrChunk++;
            }
        }
    }

    uint32_t cksum = ~bpmChecksum;
    if(BPMread() != (uint8_t)(cksum >>  0)) return err(BEAT_BPM_CHECKSUM_INVALID);
    if(BPMread() != (uint8_t)(cksum >>  8)) return err(BEAT_BPM_CHECKSUM_INVALID);
    if(BPMread() != (uint8_t)(cksum >> 16)) return err(BEAT_BPM_CHECKSUM_INVALID);
    if(BPMread() != (uint8_t)(cksum >> 24)) return err(BEAT_BPM_CHECKSUM_INVALID);

    fvx_close(&patchFile);
    if (bpmFileMode) {
        fvx_close(&sourceFile);
        fvx_close(&targetFile);
    }
    return BEAT_SUCCESS;
}