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
#include "timer.h"

#define chunkSize STD_BUFFER_SIZE

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
    BEAT_CANCELED,
    BEAT_MEMORY
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

typedef enum {
    BEAT_PATCH = 1,
    BEAT_SOURCE,
    BEAT_TARGET
} BEATFILEID;

typedef struct {
    u8* data;
    u32 size;
    u64 time;
} DataChunk;

typedef struct {
    FIL file;
    u8 id;
    u32 size;
    u32 checksum;
    u32 checksumNeeded;
    u32 currOffset;
    u32 relOffset;
    DataChunk dataChunks[];
} BeatFile;

static BeatFile *patch;
static BeatFile *source;
static BeatFile *target;

static bool bpmIsActive;
static u32 bpsSize;
static u32 bpsChecksum;

static u64 timer;
static u64 timerLastCheck;

static char progressText[256];

BeatFile* initFile(const char *path, u8 id, u64 targetSize) {
    u32 numChunks;
    if (id != BEAT_TARGET) {
        FIL f;
        if (fvx_open(&f, path, FA_READ) != FR_OK) return NULL;
        numChunks = fvx_size(&f) / chunkSize;
        fvx_close(&f);
    } else numChunks = targetSize / chunkSize;
    BeatFile *bf = malloc(offsetof(BeatFile, dataChunks) + ((numChunks + 1) * sizeof(DataChunk)));
    if (id == BEAT_TARGET) {
        if (fvx_open(&bf->file, path, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK) return NULL;
        fvx_lseek(&bf->file, targetSize); fvx_lseek(&bf->file, 0);
    } else if (fvx_open(&bf->file, path, FA_READ) != FR_OK) return NULL;
    bf->id = id; bf->size = fvx_size(&bf->file);
    bf->checksum = ~0; bf->checksumNeeded = 0;
    bf->currOffset = 0; bf->relOffset = 0;
    for (UINT n = 0; n <= numChunks; n++) {
        bf->dataChunks[n].data = NULL;
        bf->dataChunks[n].size = ((n == numChunks) ? (bf->size % chunkSize) : chunkSize);
        bf->dataChunks[n].time = timer;
    }
    return bf;
}

bool checksumChunk(BeatFile *bf, u32 chunkNum) {
    u32 trimLen = 0;
    if (bf->id == BEAT_PATCH) {
        u32 chunkEnd = (chunkNum * chunkSize) + bf->dataChunks[chunkNum].size;
        u32 patchEnd = bf->size - 4;
        if (chunkEnd > patchEnd) trimLen = chunkEnd - patchEnd;
    } else if (bf->id == BEAT_TARGET) {
        fvx_lseek(&bf->file, chunkNum * chunkSize);
        if (fvx_write(&bf->file, bf->dataChunks[chunkNum].data, bf->dataChunks[chunkNum].size, NULL) != FR_OK) return false;
    }
    bf->checksum = crc32_calculate(bf->checksum, bf->dataChunks[chunkNum].data, bf->dataChunks[chunkNum].size - trimLen);
    bf->checksumNeeded++;
    return true;
}

bool freeChunk(BeatFile *bf, u32 chunkNum) {
    if (chunkNum == bf->checksumNeeded && !checksumChunk(bf, chunkNum)) return false;
    free(bf->dataChunks[chunkNum].data);
    bf->dataChunks[chunkNum].data = NULL;
    return true;
}

bool freeOldestChunk() {
    BeatFile *fid; u32 chunkNum; u64 chunkTime = UINT64_MAX;
    for (UINT n = 0; n <= source->size / chunkSize; n++) {
        if (source->dataChunks[n].data && source->dataChunks[n].time < chunkTime) {
            fid = source; chunkNum = n; chunkTime = source->dataChunks[n].time;
        }
    }
    for (UINT n = 0; n <= target->size / chunkSize; n++) {
        if (target->dataChunks[n].data && target->dataChunks[n].time < chunkTime && n != (target->currOffset / chunkSize)) {
            fid = target; chunkNum = n; chunkTime = target->dataChunks[n].time;
        }
    }
    if (chunkTime != UINT64_MAX) return freeChunk(fid, chunkNum);
    else return false;
}

bool readChunk(BeatFile *bf, u32 chunkNum) {
    if (bf->id == BEAT_PATCH) { // the patch is read linearly, so previous chunks can be freed immediately
        for (UINT n = 0; n < chunkNum; n++) {
            if (bf->dataChunks[n].data) freeChunk(bf, n);
        }
    }
    if (bf->dataChunks[chunkNum].size == 0) return true;
    bf->dataChunks[chunkNum].data = malloc(bf->dataChunks[chunkNum].size);
    while (!bf->dataChunks[chunkNum].data) { // free chunks in order from least recently accessed to most recently accessed until we are no longer out of memory
        if (!freeOldestChunk()) return false;
        bf->dataChunks[chunkNum].data = malloc(bf->dataChunks[chunkNum].size);
    }
    fvx_lseek(&bf->file, chunkNum * chunkSize);
    if (fvx_read(&bf->file, bf->dataChunks[chunkNum].data, bf->dataChunks[chunkNum].size, NULL) != FR_OK) return false;
    if (bf->id == BEAT_SOURCE && chunkNum == bf->checksumNeeded && !checksumChunk(bf, chunkNum)) return false; // checksum source as soon as possible
    if (bf->id == BEAT_TARGET && chunkNum == bf->checksumNeeded + 1 && !checksumChunk(bf, chunkNum - 1)) return false; // write and checksum target as soon as possible
    bf->dataChunks[chunkNum].time = timer_ticks(timer);
    return true;
}

u32 closeFile(BeatFile *bf) {
    for (UINT n = 0; n <= bf->size / chunkSize; n++) {
        if (bf->dataChunks[n].data) freeChunk(bf, n);
    }
    fvx_close(&bf->file);
    u32 checksum = ~bf->checksum;
    free(bf);
    return checksum;
}

int fatalError(int errcode) {
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
        case BEAT_MEMORY:
            ShowPrompt(false, "%s\nNot enough memory.", progressText); break;
    }
    if (patch) { closeFile(patch); patch = NULL; }
    if (source) { closeFile(source); source = NULL; }
    if (target) { closeFile(target); target = NULL; }
    return errcode;
}

bool beatCopy(const char* inName, const char* outName, u32 offset, u32 length) {
    FIL inFile, outFile;
    if (fvx_open(&inFile, inName, FA_READ) != FR_OK ||
        fvx_open(&outFile, outName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) != FR_OK)
        return false;
    fvx_lseek(&inFile, offset);
    if (length == 0) length = fvx_size(&inFile);
    u32 bufsiz = min(STD_BUFFER_SIZE, length);
    u8* buffer = malloc(bufsiz);
    if (!buffer) return false;
    
    bool ret = true;
    for (u64 pos = 0; (pos < length) && ret; pos += bufsiz) {
        UINT read_bytes = min(bufsiz, length - pos);
        UINT bytes_read = read_bytes;
        if ((fvx_read(&inFile, buffer, read_bytes, &bytes_read) != FR_OK) || (read_bytes != bytes_read)) ret = false;
        if (ret && fvx_write(&outFile, buffer, read_bytes, &bytes_read) != FR_OK) ret = false;
    }
    
    fvx_close(&inFile);
    fvx_close(&outFile);
    free(buffer);
    return ret;
}

u8 beatRead() {
    if (!patch->dataChunks[patch->currOffset / chunkSize].data) readChunk(patch, patch->currOffset / chunkSize);
    u8 buf = patch->dataChunks[patch->currOffset / chunkSize].data[patch->currOffset % chunkSize];
    bpsChecksum = crc32_adjust(bpsChecksum, buf);
    patch->currOffset++; patch->relOffset++;
    return buf;
}

u64 beatReadNumber() {
    u64 data = 0, shift = 1;
    while(true) {
        u8 x = beatRead();
        data += (x & 0x7f) * shift;
        if(x & 0x80) break;
        shift <<= 7;
        data += shift;
    }
    return data;
}

u32 beatReadChecksum() {
    u32 checksum = 0;
    checksum |= beatRead() <<  0;
    checksum |= beatRead() <<  8;
    checksum |= beatRead() << 16;
    checksum |= beatRead() << 24;
    return checksum;
}

bool beatReadString(u32 length, char text[]) {
    char strBuf[256];
    for(u32 i = 0; i < length; i++) { strBuf[min(i, 255)] = beatRead(); }
    strBuf[min(length, 255)] = '\0';
    snprintf(text, length+1, "%s", strBuf);
    return true;
}

void findNewOffset(BeatFile *bf) {
    int offset = beatReadNumber();
    bool negative = offset & 1;
    offset >>= 1;
    if (negative) offset = -offset;
    bf->relOffset += offset;
}

int ApplyBeatPatch(const char* targetName) {
    bpsChecksum = ~0;
    patch->relOffset = 0;
    if(bpsSize < 19) return fatalError(BEAT_PATCH_TOO_SMALL);
    
    char header[5];
    beatReadString(4, header);
    if (strcmp(header, "BPS1") != 0) return fatalError(BEAT_PATCH_INVALID_HEADER);
    
    u64 patchSourceSize = beatReadNumber(patch);
    u64 patchTargetSize = beatReadNumber(patch);
    u64 patchMetaSize = beatReadNumber(patch);
    char metadata[256];
    beatReadString(patchMetaSize, metadata);

    target = initFile(targetName, BEAT_TARGET, patchTargetSize);
    if (!target) return fatalError(BEAT_INVALID_FILE_PATH);
    if (patchSourceSize > source->size) return fatalError(BEAT_SOURCE_TOO_SMALL);
    if (patchTargetSize > target->size) return fatalError(BEAT_TARGET_TOO_SMALL);
    
    bool chunkedPatch = chunkSize < patch->size;
    bool chunkedSource = chunkSize < source->size;
    bool chunkedTarget = chunkSize < target->size;
    bool chunkedCopy = chunkedPatch || chunkedSource || chunkedTarget;
    if ((!chunkedSource && !readChunk(source, 0)) ||
        (!chunkedTarget && !readChunk(target, 0)))
        return fatalError(BEAT_MEMORY);
    
    while(patch->relOffset < bpsSize - 12) {
        if (!ShowProgress(patch->currOffset, patch->size, progressText)) {
            if (ShowPrompt(true, "%s\nB button detected. Cancel?", progressText)) return fatalError(BEAT_CANCELED);
            ShowProgress(0, patch->size, progressText);
            ShowProgress(patch->currOffset, patch->size, progressText);
        }
            
        UINT length = beatReadNumber(patch);
        UINT mode = length & 3;
        length = (length >> 2) + 1;
        
        if (mode == BEAT_SOURCECOPY) { findNewOffset(source); }
        else if (mode == BEAT_TARGETCOPY) { findNewOffset(target); }
        
        if (!chunkedCopy) { // if all three files are smaller than 1 MB, no memory management is required
            switch (mode) {
                case BEAT_SOURCEREAD:
                    memcpy(&target->dataChunks[0].data[target->currOffset], &source->dataChunks[0].data[target->currOffset], length);
                    target->currOffset += length; source->currOffset += length;
                    break;
                case BEAT_TARGETREAD:
                    memcpy(&target->dataChunks[0].data[target->currOffset], &patch->dataChunks[0].data[patch->currOffset], length);
                    if (bpmIsActive) bpsChecksum = crc32_calculate(bpsChecksum, &patch->dataChunks[0].data[patch->currOffset], length);
                    target->currOffset += length; patch->currOffset += length; patch->relOffset += length;
                    break;
                case BEAT_SOURCECOPY:
                    memcpy(&target->dataChunks[0].data[target->currOffset], &source->dataChunks[0].data[source->relOffset], length);
                    target->currOffset += length; source->relOffset += length;
                    break;
                case BEAT_TARGETCOPY: // memcpy is not used due to overlapping memory regions: we may need to read data that we have just written
                    while (length--) { target->dataChunks[0].data[target->currOffset++] = target->dataChunks[0].data[target->relOffset++]; }
                    break;
            }
        } else { // otherwise, we have to check before each read that the 1 MB chunk of data that we want is currently read into memory
            u32 outChunk, outPos, inChunk, inPos;
            UINT maxlen; // this variable stops reads at the end of chunks
            while (length) { // and this one restarts them
                if (chunkedTarget) { // we can still optimize portions if the related file is smaller than 1 MB
                    outChunk = target->currOffset / chunkSize; outPos = target->currOffset % chunkSize;
                    if (!target->dataChunks[outChunk].data && !readChunk(target, outChunk)) return fatalError(BEAT_MEMORY);
                    target->dataChunks[outChunk].time = timer_ticks(timer);
                    maxlen = min(target->dataChunks[outChunk].size - outPos, length);
                } else { outChunk = 0; outPos = target->currOffset; maxlen = length; }
                switch (mode) {
                    case BEAT_SOURCEREAD:
                        if (chunkedSource) {
                            if (!source->dataChunks[outChunk].data && !readChunk(source, outChunk)) return fatalError(BEAT_MEMORY);
                            source->dataChunks[outChunk].time = timer_ticks(timer);
                            maxlen = min(source->dataChunks[outChunk].size - outPos, maxlen);
                        }
                        length -= maxlen; target->currOffset += maxlen; source->currOffset += maxlen;
                        memcpy(&target->dataChunks[outChunk].data[outPos], &source->dataChunks[outChunk].data[outPos], maxlen);
                        break;
                    case BEAT_TARGETREAD:
                        if (chunkedPatch) {
                            inChunk = patch->currOffset / chunkSize; inPos = patch->currOffset % chunkSize;
                            if (!patch->dataChunks[inChunk].data && !readChunk(patch, inChunk)) return fatalError(BEAT_MEMORY);
                            maxlen = min(patch->dataChunks[inChunk].size - inPos, maxlen);
                        } else { inChunk = 0; inPos = patch->currOffset; }
                        length -= maxlen; target->currOffset += maxlen; patch->currOffset += maxlen; patch->relOffset += maxlen;
                        memcpy(&target->dataChunks[outChunk].data[outPos], &patch->dataChunks[inChunk].data[inPos], maxlen);
                        if (bpmIsActive) bpsChecksum = crc32_calculate(bpsChecksum, &patch->dataChunks[inChunk].data[inPos], maxlen);
                        break;
                    case BEAT_SOURCECOPY:
                        if (chunkedSource) {
                            inChunk = source->relOffset / chunkSize; inPos = source->relOffset % chunkSize;
                            if (!source->dataChunks[inChunk].data && !readChunk(source, inChunk)) return fatalError(BEAT_MEMORY);
                            source->dataChunks[inChunk].time = timer_ticks(timer);
                            maxlen = min(source->dataChunks[inChunk].size - inPos, maxlen);
                        } else { inChunk = 0; inPos = source->relOffset; }
                        length -= maxlen; target->currOffset += maxlen; source->relOffset += maxlen;
                        memcpy(&target->dataChunks[outChunk].data[outPos], &source->dataChunks[inChunk].data[inPos], maxlen);
                        break;
                    case BEAT_TARGETCOPY:
                        if (chunkedTarget) {
                            inChunk = target->relOffset / chunkSize; inPos = target->relOffset % chunkSize;
                            if (!target->dataChunks[inChunk].data && !readChunk(target, inChunk)) return fatalError(BEAT_MEMORY);
                            target->dataChunks[inChunk].time = timer_ticks(timer);
                            maxlen = min(target->dataChunks[inChunk].size - inPos, maxlen);
                        } else { inChunk = 0; inPos = target->relOffset; }
                        length -= maxlen; target->currOffset += maxlen; target->relOffset += maxlen;
                        if (inChunk == outChunk) { while (maxlen--) { target->dataChunks[outChunk].data[outPos++] = target->dataChunks[inChunk].data[inPos++]; } }
                        else memcpy(&target->dataChunks[outChunk].data[outPos], &target->dataChunks[inChunk].data[inPos], maxlen);
                        break;
                }
            }
        }
    }
    
    u32 patchSourceChecksum = beatReadChecksum();
    u32 patchTargetChecksum = beatReadChecksum();
    u32 finalPatchChecksum = ~bpsChecksum;
    u32 patchPatchChecksum = beatReadChecksum();
    
    while (source->checksumNeeded <= source->size / chunkSize) { // not all source chunks are guaranteed to have been checksummed
        if (!source->dataChunks[source->checksumNeeded].data) readChunk(source, source->checksumNeeded);
        checksumChunk(source, source->checksumNeeded);
    }
    
    if (!bpmIsActive) { finalPatchChecksum = closeFile(patch); patch = NULL; }
    u32 finalSourceChecksum = closeFile(source); source = NULL;
    u32 finalTargetChecksum = closeFile(target); target = NULL;
    if(finalPatchChecksum != patchPatchChecksum) return fatalError(BEAT_PATCH_CHECKSUM_INVALID);
    if(finalSourceChecksum != patchSourceChecksum) return fatalError(BEAT_SOURCE_CHECKSUM_INVALID);
    if(finalTargetChecksum != patchTargetChecksum) return fatalError(BEAT_TARGET_CHECKSUM_INVALID);
    
    return BEAT_SUCCESS;
}

int ApplyBPSPatch(const char* patchName, const char* sourceName, const char* targetName) {
    bpmIsActive = false; timer = timer_start(); timerLastCheck = 0;
    patch = initFile(patchName, BEAT_PATCH, 0); source = initFile(sourceName, BEAT_SOURCE, 0); target = NULL;
    
    if (!CheckWritePermissions(targetName) || !patch || !source) return fatalError(BEAT_INVALID_FILE_PATH);
    
    bpsSize = patch->size;
    snprintf(progressText, 256, "%s", targetName);
    return ApplyBeatPatch(targetName);
}

int ApplyBPMPatch(const char* patchName, const char* sourcePath, const char* targetPath) {
    bpmIsActive = true; timer = timer_start(); timerLastCheck = 0;
    patch = initFile(patchName, BEAT_PATCH, 0); source = NULL; target = NULL;
    
    if ((!CheckWritePermissions(targetPath)) || !patch ||
        ((fvx_stat(targetPath, NULL) != FR_OK) && (fvx_mkdir(targetPath) != FR_OK)))
        return fatalError(BEAT_INVALID_FILE_PATH);
    
    char header[5];
    beatReadString(4, header);
    if (strcmp(header, "BPM1") != 0) return fatalError(BEAT_PATCH_INVALID_HEADER);
    u64 metadataLength = beatReadNumber();
    char metadata[256];
    beatReadString(metadataLength, metadata);

    while(patch->currOffset < patch->size - 4) {
        u64 encoding = beatReadNumber(patch);
        unsigned int action = encoding & 3;
        unsigned int targetLength = (encoding >> 2) + 1;
        char targetName[256];
        beatReadString(targetLength, targetName);
        snprintf(progressText, 256, "%s", targetName);
        
        if (!ShowProgress(patch->currOffset, patch->size, progressText)) {
            if (ShowPrompt(true, "%s\nB button detected. Cancel?", progressText)) return fatalError(BEAT_CANCELED);
            ShowProgress(0, patch->size, progressText);
            ShowProgress(patch->currOffset, patch->size, progressText);
        }

        if(action == BEAT_CREATEPATH) {
            char newPath[256];
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            if ((fvx_stat(newPath, NULL) != FR_OK) && (fvx_mkdir(newPath) != FR_OK)) return fatalError(BEAT_INVALID_FILE_PATH);
        } else if(action == BEAT_CREATEFILE) {
            char newPath[256];
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            u64 fileSize = beatReadNumber();
            if (!beatCopy(patchName, newPath, patch->currOffset, fileSize)) return fatalError(BEAT_INVALID_FILE_PATH);
            patch->currOffset += fileSize;
            while (patch->checksumNeeded <= patch->currOffset / chunkSize) {
                if (!patch->dataChunks[patch->checksumNeeded].data) readChunk(patch, patch->checksumNeeded);
                checksumChunk(patch, patch->checksumNeeded);
            }
            beatReadChecksum();
        } else {
            encoding = beatReadNumber();
            char originPath[256], sourceName[256], oldPath[256], newPath[256];
            if (encoding & 1) snprintf(originPath, 256, "%s", targetPath);
            else snprintf(originPath, 256, "%s", sourcePath);
            if ((encoding >> 1) == 0) snprintf(sourceName, 256, "%s", targetName);
            else beatReadString((encoding >> 1), sourceName);
            snprintf(oldPath, 256, "%s/%s", originPath, sourceName);
            snprintf(newPath, 256, "%s/%s", targetPath, targetName);
            if(action == BEAT_MODIFYFILE) {
                source = initFile(oldPath, BEAT_SOURCE, 0);
                if (!source) return fatalError(BEAT_INVALID_FILE_PATH);
                bpsSize = beatReadNumber();
                int result = ApplyBeatPatch(newPath);
                if (result != BEAT_SUCCESS) return result;
            } else if(action == BEAT_MIRRORFILE) {
                if (!beatCopy(oldPath, newPath, 0, 0)) return fatalError(BEAT_INVALID_FILE_PATH);
                beatReadChecksum();
            }
        }
    }

    u32 cksum = beatReadChecksum();
    u32 patchChecksum = closeFile(patch); patch = NULL;
    if (patchChecksum != cksum) return fatalError(BEAT_BPM_CHECKSUM_INVALID);

    return BEAT_SUCCESS;
}
