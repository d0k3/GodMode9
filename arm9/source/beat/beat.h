// C port of byuu's \nall\beat\patch.hpp and \multi.hpp, which were released under GPLv3
// https://github.com/eai04191/beat/blob/master/nall/beat/patch.hpp
// https://github.com/eai04191/beat/blob/master/nall/beat/multi.hpp
// Ported by Hyarion for use with VirtualFatFS

#pragma once
#include "common.h"
#include "crc32.h"
#include "fsperm.h"
#include "ui.h"
#include "vff.h"

typedef enum {
    BEAT_UNKNOWN = 0,
    BEAT_SUCCESS,
    BEAT_PATCH_TOO_SMALL,
    BEAT_PATCH_INVALID_HEADER,
    BEAT_SOURCE_TOO_SMALL,
    BEAT_TARGET_TOO_SMALL,
    BEAT_SOURCE_CHECKSUM_INVALID,
    BEAT_TARGET_CHECKSUM_INVALID,
    BEAT_PATCH_CHECKSUM_INVALID,
    BEAT_BPM_CHECKSUM_INVALID,
    BEAT_INVALID_FILE_PATH
} BPSRESULT;

typedef enum {
    BEAT_SOURCEREAD = 0,
    BEAT_TARGETREAD,
    BEAT_SOURCECOPY,
    BEAT_TARGETCOPY
} BPSMODE;

int ApplyBPSPatch(const char* modifyName, const char* sourceName, const char* targetName);

typedef enum {
    BEAT_CREATEPATH = 0,
    BEAT_CREATEFILE,
    BEAT_MODIFYFILE,
    BEAT_MIRRORFILE
} BPMACTION;

int ApplyBPMPatch(const char* patchName, const char* sourcePath, const char* targetPath);
