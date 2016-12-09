#include "exefs.h"

u32 ValidateExeFsHeader(ExeFsHeader* exefs, u32 size) {
    u8 zeroes[32] = { 0 };
    u32 data_size = 0;
    u32 n_files = 0;
    for (u32 i = 0; i < 10; i++) {
        ExeFsFileHeader* file = exefs->files + i;
        u8* hash = exefs->hashes[9 - i];
        if (file->size == 0) continue;
        if (file->offset < data_size) return 1; // overlapping data, failed
        if (memcmp(hash, zeroes, 32) == 0) return 1; // hash not set, failed
        data_size = file->offset + file->size;
        n_files++;
    }
    if (size && (data_size > (size - sizeof(ExeFsHeader)))) // exefs header not included in table
        return 1;
    return (n_files) ? 0 : 1;
}
