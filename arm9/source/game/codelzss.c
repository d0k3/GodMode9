#include "codelzss.h"
#include "language.h"
#include "ui.h"

#define CODE_COMP_SIZE(f)   ((f)->off_size_comp & 0xFFFFFF)
#define CODE_COMP_END(f)    ((int) CODE_COMP_SIZE(f) - (int) (((f)->off_size_comp >> 24) % 0xFF))
#define CODE_DEC_SIZE(f)    (CODE_COMP_SIZE(f) + (f)->addsize_dec)

#define CODE_SEG_OFFSET(s)  (((s) & 0x0FFF) + 2)
#define CODE_SEG_SIZE(s)    ((((s) >> 12) & 0xF) + 3)

typedef struct {
    u32 off_size_comp; // 0xOOSSSSSS, where O == reverse offset and S == size
    u32 addsize_dec; // decompressed size - compressed size
} PACKED_ALIGN(4) CodeLzssFooter;


u32 GetCodeLzssUncompressedSize(void* footer, u32 comp_size) {
    if (comp_size < sizeof(CodeLzssFooter)) return 0;

    CodeLzssFooter* f = (CodeLzssFooter*) footer;
    if ((CODE_COMP_SIZE(f) > comp_size) || (CODE_COMP_END(f) < 0)) return 0;

    return CODE_DEC_SIZE(f) + (comp_size - CODE_COMP_SIZE(f));
}

// see: https://github.com/zoogie/DSP1/blob/master/source/main.c#L44
u32 DecompressCodeLzss(u8* code, u32* code_size, u32 max_size) {
    u8* data_start = code;
    u8* comp_start = data_start;

    // get footer, fix comp_start offset
    if ((*code_size < sizeof(CodeLzssFooter)) || (*code_size > max_size)) return 1;
    CodeLzssFooter* footer = (CodeLzssFooter*) (void*) (data_start + *code_size - sizeof(CodeLzssFooter));
    if (CODE_COMP_SIZE(footer) <= *code_size) comp_start += *code_size - CODE_COMP_SIZE(footer);
    else return 1;

    // more sanity checks
    if ((CODE_COMP_END(footer) < 0) || (CODE_DEC_SIZE(footer) > max_size))
        return 1; // not reverse LZSS compressed code or too big uncompressed

    // set pointers
    u8* data_end = (u8*) comp_start + CODE_DEC_SIZE(footer);
    u8* ptr_in = (u8*) comp_start + CODE_COMP_END(footer);
    u8* ptr_out = data_end;

    // main decompression loop
    while ((ptr_in > comp_start) && (ptr_out > comp_start)) {
        if (!ShowProgress(data_end - ptr_out, data_end - data_start, STR_DECOMPRESSING_DOT_CODE)) {
            if (ShowPrompt(true, "%s", STR_DECOMPRESSING_DOT_CODE_B_DETECTED_CANCEL)) return 1;
            ShowProgress(0, data_end - data_start, STR_DECOMPRESSING_DOT_CODE);
            ShowProgress(data_end - ptr_out, data_end - data_start, STR_DECOMPRESSING_DOT_CODE);
        }

        // sanity check
        if (ptr_out < ptr_in) return 1;

        // read and process control byte
        u8 ctrlbyte = *(--ptr_in);
        for (int i = 7; i >= 0; i--) {
            // end conditions met?
            if ((ptr_in <= comp_start) || (ptr_out <= comp_start))
                break;

            // process control byte
            if ((ctrlbyte >> i) & 0x1) {
                // control bit set, read segment code
                ptr_in -= 2;
                u16 seg_code = getle16(ptr_in);
                if (ptr_in < comp_start) return 1; // corrupted code
                u32 seg_off = CODE_SEG_OFFSET(seg_code);
                u32 seg_len = CODE_SEG_SIZE(seg_code);

                // sanity check for output pointer
                if ((ptr_out - seg_len < comp_start) || (ptr_out + seg_off >= data_end))
                    return 1;

                // copy data to the correct place
                for (u32 c = 0; c < seg_len; c++) {
                    u8 byte = *(ptr_out + seg_off);
                    *(--ptr_out) = byte;
                }
            } else {
                // sanity check for both pointers
                if ((ptr_out == comp_start) || (ptr_in == comp_start))
                    return 1;

                // control bit not set, copy byte verbatim
                *(--ptr_out) = *(--ptr_in);
            }
        }
    }

    // check pointers
    if ((ptr_in != comp_start) || (ptr_out != comp_start))
        return 1;

    // all fine if arriving here - return the result
    *code_size = data_end - data_start;
    return 0;
}

// see https://github.com/dnasdw/3dstool/blob/master/src/backwardlz77.cpp (GPLv3)
typedef struct {
    u16 WindowPos;
    u16 WindowLen;
    s16* OffsetTable;
    s16* ReversedOffsetTable;
    s16* ByteTable;
    s16* EndTable;
} sCompressInfo;

void initTable(sCompressInfo* a_pInfo, void* a_pWork) {
    a_pInfo->WindowPos = 0;
    a_pInfo->WindowLen = 0;
    a_pInfo->OffsetTable = (s16*)(a_pWork);
    a_pInfo->ReversedOffsetTable = (s16*)(a_pWork) + 4098;
    a_pInfo->ByteTable = (s16*)(a_pWork) + 4098 + 4098;
    a_pInfo->EndTable = (s16*)(a_pWork) + 4098 + 4098 + 256;

    for (int i = 0; i < 256; i++) {
        a_pInfo->ByteTable[i] = -1;
        a_pInfo->EndTable[i] = -1;
    }
}

int search(sCompressInfo* a_pInfo, const u8* a_pSrc, int* a_nOffset, int a_nMaxSize) {
    if (a_nMaxSize < 3) {
        return 0;
    }

    const u8* pSearch = NULL;
    int nSize = 2;
    const u16 uWindowPos = a_pInfo->WindowPos;
    const u16 uWindowLen = a_pInfo->WindowLen;
    s16* pReversedOffsetTable = a_pInfo->ReversedOffsetTable;

    for (s16 nOffset = a_pInfo->EndTable[*(a_pSrc - 1)]; nOffset != -1; nOffset = pReversedOffsetTable[nOffset]) {
        if (nOffset < uWindowPos) {
            pSearch = a_pSrc + uWindowPos - nOffset;
        } else {
            pSearch = a_pSrc + uWindowLen + uWindowPos - nOffset;
        }

        if (pSearch - a_pSrc < 3) {
            continue;
        }

        if (*(pSearch - 2) != *(a_pSrc - 2) || *(pSearch - 3) != *(a_pSrc - 3)) {
            continue;
        }

        int nMaxSize = (int)((s64)min(a_nMaxSize, pSearch - a_pSrc));
        int nCurrentSize = 3;

        while (nCurrentSize < nMaxSize && *(pSearch - nCurrentSize - 1) == *(a_pSrc - nCurrentSize - 1)) {
            nCurrentSize++;
        }

        if (nCurrentSize > nSize) {
            nSize = nCurrentSize;
            *a_nOffset = (int)(pSearch - a_pSrc);
            if (nSize == a_nMaxSize) {
                break;
            }
        }
    }

    if (nSize < 3) {
        return 0;
    }

    return nSize;
}

void slideByte(sCompressInfo* a_pInfo, const u8* a_pSrc) {
    u8 uInData = *(a_pSrc - 1);
    u16 uInsertOffset = 0;
    const u16 uWindowPos = a_pInfo->WindowPos;
    const u16 uWindowLen = a_pInfo->WindowLen;
    s16* pOffsetTable = a_pInfo->OffsetTable;
    s16* pReversedOffsetTable = a_pInfo->ReversedOffsetTable;
    s16* pByteTable = a_pInfo->ByteTable;
    s16* pEndTable = a_pInfo->EndTable;

    if (uWindowLen == 4098) {
        u8 uOutData = *(a_pSrc + 4097);

        if ((pByteTable[uOutData] = pOffsetTable[pByteTable[uOutData]]) == -1) {
            pEndTable[uOutData] = -1;
        } else {
            pReversedOffsetTable[pByteTable[uOutData]] = -1;
        }

        uInsertOffset = uWindowPos;
    } else {
        uInsertOffset = uWindowLen;
    }

    s16 nOffset = pEndTable[uInData];

    if (nOffset == -1) {
        pByteTable[uInData] = uInsertOffset;
    } else {
        pOffsetTable[nOffset] = uInsertOffset;
    }

    pEndTable[uInData] = uInsertOffset;
    pOffsetTable[uInsertOffset] = -1;
    pReversedOffsetTable[uInsertOffset] = nOffset;

    if (uWindowLen == 4098) {
        a_pInfo->WindowPos = (uWindowPos + 1) % 4098;
    } else {
        a_pInfo->WindowLen++;
    }
}

static inline void slide(sCompressInfo* a_pInfo, const u8* a_pSrc, int a_nSize) {
    for (int i = 0; i < a_nSize; i++) {
        slideByte(a_pInfo, a_pSrc--);
    }
}

s64 alignBytes(s64 a_nData, s64 a_nAlignment) {
    return (a_nData + a_nAlignment - 1) / a_nAlignment * a_nAlignment;
}

bool CompressCodeLzss(const u8* a_pUncompressed, u32 a_uUncompressedSize, u8* a_pCompressed, u32* a_uCompressedSize) {
    const int s_nCompressWorkSize = (4098 + 4098 + 256 + 256) * sizeof(s16);
    bool bResult = true;

    if (a_uUncompressedSize > sizeof(CodeLzssFooter) && *a_uCompressedSize >= a_uUncompressedSize) {
        u8* pWork = malloc(s_nCompressWorkSize * sizeof(u8));
        if (!pWork) return false;

        do {
            sCompressInfo info;
            initTable(&info, pWork);

            const int nMaxSize = 0xF + 3;
            const u8* pSrc = a_pUncompressed + a_uUncompressedSize;
            u8* pDest = a_pCompressed + a_uUncompressedSize;

            while (pSrc - a_pUncompressed > 0 && pDest - a_pCompressed > 0) {
                if (!ShowProgress((u32)(a_pUncompressed + a_uUncompressedSize - pSrc), a_uUncompressedSize, STR_COMPRESSING_DOT_CODE)) {
                    if (ShowPrompt(true, "%s", STR_COMPRESSING_DOT_CODE_B_DETECTED_CANCEL)) {
                        bResult = false;
                        break;
                    }
                    ShowProgress(0, a_uUncompressedSize, STR_COMPRESSING_DOT_CODE);
                    ShowProgress((u32)(a_pUncompressed + a_uUncompressedSize - pSrc), a_uUncompressedSize, STR_COMPRESSING_DOT_CODE);
                }

                u8* pFlag = --pDest;
                *pFlag = 0;

                for (int i = 0; i < 8; i++) {
                    int nOffset = 0;
                    int nSize = search(&info, pSrc, &nOffset, (int)((s64)min((s64)min(nMaxSize, pSrc - a_pUncompressed), a_pUncompressed + a_uUncompressedSize - pSrc)));

                    if (nSize < 3) {
                        if (pDest - a_pCompressed < 1) {
                            bResult = false;
                            break;
                        }

                        slide(&info, pSrc, 1);
                        *--pDest = *--pSrc;
                    } else {
                        if (pDest - a_pCompressed < 2) {
                            bResult = false;
                            break;
                        }

                        *pFlag |= 0x80 >> i;
                        slide(&info, pSrc, nSize);
                        pSrc -= nSize;
                        nSize -= 3;
                        *--pDest = (nSize << 4 & 0xF0) | ((nOffset - 3) >> 8 & 0x0F);
                        *--pDest = (nOffset - 3) & 0xFF;
                    }

                    if (pSrc - a_pUncompressed <= 0) {
                        break;
                    }
                }

                if (!bResult) {
                    break;
                }
            }

            if (!bResult) {
                break;
            }

            *a_uCompressedSize = (u32)(a_pCompressed + a_uUncompressedSize - pDest);
        } while (false);

        free(pWork);
    } else {
        bResult = false;
    }

    if (bResult) {
        u32 uOrigSize = a_uUncompressedSize;
        u8* pCompressBuffer = a_pCompressed + a_uUncompressedSize - *a_uCompressedSize;
        u32 uCompressBufferSize = *a_uCompressedSize;
        u32 uOrigSafe = 0;
        u32 uCompressSafe = 0;
        bool bOver = false;

        while (uOrigSize > 0) {
            u8 uFlag = pCompressBuffer[--uCompressBufferSize];

            for (int i = 0; i < 8; i++) {
                if ((uFlag << i & 0x80) == 0) {
                    uCompressBufferSize--;
                    uOrigSize--;
                } else {
                    int nSize = (pCompressBuffer[--uCompressBufferSize] >> 4 & 0x0F) + 3;
                    uCompressBufferSize--;
                    uOrigSize -= nSize;

                    if (uOrigSize < uCompressBufferSize) {
                        uOrigSafe = uOrigSize;
                        uCompressSafe = uCompressBufferSize;
                        bOver = true;
                        break;
                    }
                }

                if (uOrigSize <= 0) {
                    break;
                }
            }

            if (bOver) {
                break;
            }
        }

        u32 uCompressedSize = *a_uCompressedSize - uCompressSafe;
        u32 uPadOffset = uOrigSafe + uCompressedSize;
        u32 uCompFooterOffset = (u32)(alignBytes(uPadOffset, 4));
        *a_uCompressedSize = uCompFooterOffset + sizeof(CodeLzssFooter);
        u32 uTop = *a_uCompressedSize - uOrigSafe;
        u32 uBottom = *a_uCompressedSize - uPadOffset;

        if (*a_uCompressedSize >= a_uUncompressedSize || uTop > 0xFFFFFF) {
            bResult = false;
        } else {
            memcpy(a_pCompressed, a_pUncompressed, uOrigSafe);
            memmove(a_pCompressed + uOrigSafe, pCompressBuffer + uCompressSafe, uCompressedSize);
            memset(a_pCompressed + uPadOffset, 0xFF, uCompFooterOffset - uPadOffset);
            CodeLzssFooter* pCompFooter = (void*)(a_pCompressed + uCompFooterOffset);
            pCompFooter->off_size_comp = uTop | (uBottom << 24);
            pCompFooter->addsize_dec = a_uUncompressedSize - *a_uCompressedSize;
        }
    }

    return bResult;
}
